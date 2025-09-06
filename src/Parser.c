#include "Parser.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "StringUtils.h"

#define ERROR_RESULT_LINE(message) ERROR_RESULT(message, CurrentToken()->lineNumber, currentFile)

static const char* currentFile;

static Array tokens;
static size_t pointer;

static Result ParseStatement(NodePtr* out);
static Result ParseExpression(NodePtr* out);
static Result ParseBlockStatement(NodePtr* out);
static Result ParseTypeAndIdentifier(Type* type, const Token** identifier);
static Result ParseModifierDeclaration(NodePtr* out);
static Result ContinueParsePrimary(NodePtr* inout, bool alreadyParsedDot);
static Result ParseFullVarDeclNoSemicolon(NodePtr* out, void* data);
static Result ParsePropertyList(NodePtr* out);

static Token* CurrentToken(void)
{
	if (pointer >= tokens.length)
		return tokens.array[tokens.length - 1];

	return tokens.array[pointer];
}

static Token* Match(const TokenType* types, const size_t length)
{
	Token* current = CurrentToken();
	for (size_t i = 0; i < length; ++i)
	{
		if (current->type == types[i])
		{
			pointer++;
			return current;
		}
	}
	return NULL;
}

static Token* MatchOne(TokenType type)
{
	return Match(&type, 1);
}

static bool IsDigitBase(char c, int base)
{
	if (base == 10) return isdigit(c);
	if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	if (base == 2) return c == '0' || c == '1';
	if (base == 8) return c >= '0' && c <= '7';
	INVALID_VALUE(base);
}

static Result StringToUInt64(const char* string, size_t stringLength, int base, int lineNumber, uint64_t* out)
{
	ASSERT(
		base == 10 ||
		base == 16 ||
		base == 8 ||
		base == 2);

	if (stringLength == 0)
		goto invalidInteger;

	for (size_t i = 0; i < stringLength; ++i)
		if (!IsDigitBase(string[i], base))
			goto invalidInteger;

	if (stringLength > 64)
		goto invalidInteger;

	char stringCopy[64 + 1];
	memcpy(stringCopy, string, stringLength);
	stringCopy[stringLength] = '\0';
	errno = 0;
	*out = strtoull(stringCopy, NULL, base);

	if (errno == ERANGE && *out == UINT64_MAX)
		goto invalidInteger;

	if (*out == 0)
	{
		bool isZero = true;
		for (size_t i = 0; i < stringLength; ++i)
			if (string[i] != '0') isZero = false;

		if (!isZero)
			goto invalidInteger;
	}

	return SUCCESS_RESULT;

invalidInteger:
	return ERROR_RESULT("Invalid integer literal", lineNumber, currentFile);
}

static Result EvaluateNumberLiteral(
	const char* string,
	size_t stringLength,
	int lineNumber,
	char** outString)
{
	ASSERT(stringLength >= 1);

	bool isInteger = true;
	for (size_t i = 0; i < stringLength; ++i)
		if (string[i] == '.') isInteger = false;

	if (!isInteger)
	{
		double floatValue;
		int consumedChars;
		if (sscanf(string, "%lf%n", &floatValue, &consumedChars) != 1)
			goto invalidFloat;

		if (fpclassify(floatValue) != FP_NORMAL && fpclassify(floatValue) != FP_ZERO)
			goto invalidFloat;

		ASSERT(consumedChars >= 0);
		if ((size_t)consumedChars != stringLength)
			goto invalidFloat;

		for (size_t i = 0; i < stringLength; ++i)
			if (!IsDigitBase(string[i], 10) && (string[i] != '.' || i == stringLength - 1))
				goto invalidFloat;

		*outString = AllocateStringLength(string, stringLength);
		return SUCCESS_RESULT;

	invalidFloat:
		return ERROR_RESULT("Invalid float literal", lineNumber, currentFile);
	}

	int base = 10;
	if (string[0] == '0' && stringLength >= 2)
	{
		if (tolower(string[1]) == 'x')
			base = 16;
		else if (tolower(string[1]) == 'o')
			base = 8;
		else if (tolower(string[1]) == 'b')
			base = 2;
	}

	uint64_t intValue;
	size_t index = base == 10 ? 0 : 2;
	ASSERT(index <= stringLength);
	PROPAGATE_ERROR(StringToUInt64(string + index, stringLength - index, base, lineNumber, &intValue));

	*outString = AllocUInt64ToString(intValue);
	return SUCCESS_RESULT;
}

typedef Result (*ParseFunction)(NodePtr*, void*);
static Result ParseCommaSeparatedList(Array* outArray, const ParseFunction function, void* data, const TokenType endToken, bool* hasEllipsis)
{
	*outArray = AllocateArray(sizeof(NodePtr));

	while (true)
	{
		NodePtr node = NULL_NODE;
		PROPAGATE_ERROR(function(&node, data));
		if (node.ptr == NULL)
		{
			if (hasEllipsis != NULL && MatchOne(Token_Ellipsis))
				*hasEllipsis = true;
			break;
		}

		ArrayAdd(outArray, &node);

		if (MatchOne(Token_Comma) == NULL)
			break;
	}

	if (MatchOne(endToken) == NULL)
		return ERROR_RESULT_LINE(
			AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));

	return SUCCESS_RESULT;
}

static Result ParseIdentifierChain(Array* array)
{
	array->array = NULL;
	array->length = 0;

	while (true)
	{
		int lineNumber = CurrentToken()->lineNumber;
		if (array->array != NULL && MatchOne(Token_Dot) == NULL)
			break;

		Token* identifier = MatchOne(Token_Identifier);
		if (identifier == NULL)
			return array->array != NULL
					   ? ERROR_RESULT("Expected identifier after \".\"", lineNumber, currentFile)
					   : NOT_FOUND_RESULT;

		if (array->array == NULL)
			*array = AllocateArray(sizeof(char*));

		char* copy = AllocateStringLength(identifier->text, identifier->textSize);
		ArrayAdd(array, &copy);
	}

	return SUCCESS_RESULT;
}

static bool ParsePrimitiveType(PrimitiveType* out, int* outLineNumber)
{
	TokenType operators[] = {
		Token_Any,
		Token_Float,
		Token_Int,
		Token_String,
		Token_Char,
		Token_Bool,
		Token_Void,
	};
	const Token* primitiveType = Match(operators, COUNTOF(operators));

	if (primitiveType == NULL)
		return false;

	*outLineNumber = primitiveType->lineNumber;
	*out = tokenTypeToPrimitiveType[primitiveType->type];
	return true;
}

static Result ParseType(Type* out)
{
	const size_t oldPointer = pointer;

	*out = (Type){.expr = NULL_NODE, .modifier = TypeModifier_None};

	PrimitiveType primitiveType;
	int lineNumber;
	if (ParsePrimitiveType(&primitiveType, &lineNumber))
	{
		out->expr = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = lineNumber,
				.type = Literal_PrimitiveType,
				.primitiveType = primitiveType,
			},
			sizeof(LiteralExpr), Node_Literal);
	}

	if (out->expr.ptr == NULL)
	{
		const int lineNumber = CurrentToken()->lineNumber;
		Array identifiers;
		PROPAGATE_ERROR(ParseIdentifierChain(&identifiers));
		if (identifiers.array != NULL)
			out->expr = AllocASTNode(
				&(MemberAccessExpr){
					.lineNumber = lineNumber,
					.identifiers = identifiers,
					.start = NULL_NODE,
					.funcReference = NULL,
					.typeReference = NULL,
					.varReference = NULL,
					.parentReference = NULL,
				},
				sizeof(MemberAccessExpr), Node_MemberAccess);
	}

	if (out->expr.ptr == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	if (MatchOne(Token_LeftSquareBracket))
	{
		NodePtr expr = NULL_NODE;
		PROPAGATE_ERROR(ParseExpression(&expr));
		if (expr.ptr != NULL)
		{
			pointer = oldPointer;
			return NOT_FOUND_RESULT;
		}

		if (!MatchOne(Token_RightSquareBracket))
			return ERROR_RESULT_LINE("Expected \"]\"");

		out->modifier = TypeModifier_Array;
	}
	else if (MatchOne(Token_Asterisk))
	{
		out->modifier = TypeModifier_Pointer;
	}

	return SUCCESS_RESULT;
}

static Result ParseStructInitializerPart(NodePtr* out, void* data)
{
	NodePtr access = NULL_NODE;
	PROPAGATE_ERROR(ContinueParsePrimary(&access, false));
	if (!access.ptr)
		return NOT_FOUND_RESULT;

	// add name to the start
	ASSERT(access.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = access.ptr;
	char* name = AllocateString(data);
	ArrayInsert(&memberAccess->identifiers, &name, 0);

	const int lineNumber = CurrentToken()->lineNumber;
	if (!MatchOne(Token_Equals))
		return ERROR_RESULT_LINE("Expected \"=\"");

	NodePtr expr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&expr));
	if (!expr.ptr)
		return ERROR_RESULT_LINE("Expected expression");

	*out = AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = memberAccess->lineNumber,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.left = access,
					.right = expr,
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
	return SUCCESS_RESULT;
}

static Result ParseBlockExpression(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Type type;
	PROPAGATE_ERROR(ParseType(&type));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	enum
	{
		BlockType_BlockExpression,
		BlockType_StructInitializer,
		BlockType_TypeCast,
	} blockType = BlockType_BlockExpression;

	const size_t pointerAfterType = pointer;
	if (MatchOne(Token_LeftCurlyBracket))
	{
		if (MatchOne(Token_Dot))
			blockType = BlockType_StructInitializer;
		else if (type.modifier == TypeModifier_None &&
				 type.expr.type == Node_Literal &&
				 ((LiteralExpr*)type.expr.ptr)->type == Literal_PrimitiveType &&
				 ((LiteralExpr*)type.expr.ptr)->primitiveType == Primitive_Void)
			blockType = BlockType_BlockExpression;
		else if (MatchOne(Token_RightCurlyBracket))
			blockType = BlockType_StructInitializer;
		else
		{
			NodePtr expr = NULL_NODE;
			PROPAGATE_ERROR(ParseExpression(&expr));
			if (expr.ptr && MatchOne(Token_RightCurlyBracket))
				blockType = BlockType_TypeCast;
			else
				blockType = BlockType_BlockExpression;
		}
	}
	pointer = pointerAfterType;

	NodePtr block = NULL_NODE;
	if (blockType == BlockType_BlockExpression)
	{
		PROPAGATE_ERROR(ParseBlockStatement(&block));
		if (block.ptr == NULL)
		{
			pointer = oldPointer;
			return NOT_FOUND_RESULT;
		}
	}
	else if (blockType == BlockType_StructInitializer) // the parser should not be doing these kinds of transformations but it works so i dont care
	{
		if (!MatchOne(Token_LeftCurlyBracket))
			UNREACHABLE();

		Array statements;
		char tempVariableName[] = "temp";
		PROPAGATE_ERROR(ParseCommaSeparatedList(&statements, ParseStructInitializerPart, tempVariableName, Token_RightCurlyBracket, NULL));

		NodePtr declaration = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = CurrentToken()->lineNumber,
				.type = (Type){
					.expr = CopyASTNode(type.expr),
					.modifier = type.modifier,
				},
				.name = AllocateString(tempVariableName),
				.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
				.initializer = NULL_NODE,
				.uniqueName = -1,
			},
			sizeof(VarDeclStmt), Node_VariableDeclaration);
		ArrayInsert(&statements, &declaration, 0);

		Array identifiers = AllocateArray(sizeof(char*));
		char* name = AllocateString(tempVariableName);
		ArrayAdd(&identifiers, &name);
		NodePtr returnStatement = AllocASTNode(
			&(ReturnStmt){
				.lineNumber = CurrentToken()->lineNumber,
				.expr = AllocASTNode(
					&(MemberAccessExpr){
						.lineNumber = CurrentToken()->lineNumber,
						.start = NULL_NODE,
						.identifiers = identifiers,
					},
					sizeof(MemberAccessExpr), Node_MemberAccess),
			},
			sizeof(ReturnStmt), Node_Return);
		ArrayAdd(&statements, &returnStatement);

		block = AllocASTNode(
			&(BlockStmt){
				.lineNumber = CurrentToken()->lineNumber,
				.statements = statements,
			},
			sizeof(BlockStmt), Node_BlockStatement);
	}
	else if (blockType == BlockType_TypeCast)
	{
		if (!MatchOne(Token_LeftCurlyBracket))
			UNREACHABLE();

		block = AllocASTNode(
			&(BlockStmt){
				.lineNumber = CurrentToken()->lineNumber,
				.statements = AllocateArray(sizeof(NodePtr)),
			},
			sizeof(BlockStmt), Node_BlockStatement);

		NodePtr expr = NULL_NODE;
		PROPAGATE_ERROR(ParseExpression(&expr));
		ASSERT(expr.ptr);
		NodePtr returnStatement = AllocASTNode(
			&(ReturnStmt){
				.lineNumber = CurrentToken()->lineNumber,
				.expr = expr,
			},
			sizeof(ReturnStmt), Node_Return);

		if (!MatchOne(Token_RightCurlyBracket))
			UNREACHABLE();

		ArrayAdd(&((BlockStmt*)block.ptr)->statements, &returnStatement);
	}

	*out = AllocASTNode(
		&(BlockExpr){
			.type = type,
			.block = block,
		},
		sizeof(BlockExpr), Node_BlockExpression);
	return SUCCESS_RESULT;
}

static Result ParseExpressionInBrackets(NodePtr* out)
{
	if (MatchOne(Token_LeftBracket) == NULL)
		return NOT_FOUND_RESULT;

	*out = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(out));
	if (out->ptr == NULL)
		return ERROR_RESULT_LINE("Expected expression");

	const Token* closingBracket = MatchOne(Token_RightBracket);
	if (closingBracket == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

	return SUCCESS_RESULT;
}

static Result ParseLiteral(NodePtr* out)
{
	TokenType operators[] = {
		Token_NumberLiteral,
		Token_StringLiteral,
		Token_CharLiteral,
		Token_True,
		Token_False,
	};
	const Token* token = Match(operators, COUNTOF(operators));

	if (token == NULL)
		return NOT_FOUND_RESULT;

	switch (token->type)
	{
	case Token_NumberLiteral:
	{
		char* number = NULL;
		PROPAGATE_ERROR(EvaluateNumberLiteral(token->text, token->textSize, token->lineNumber, &number));
		ASSERT(number);

		*out = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = token->lineNumber,
				.type = Literal_Number,
				.number = number,
			},
			sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}
	case Token_StringLiteral:
	{
		*out = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = token->lineNumber,
				.type = Literal_String,
				.string = AllocateStringLength(token->text, token->textSize),
			},
			sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}
	case Token_CharLiteral:
	{
		*out = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = token->lineNumber,
				.type = Literal_Char,
				.multiChar = AllocateStringLength(token->text, token->textSize),
			},
			sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}
	case Token_True:
	case Token_False:
	{
		*out = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = token->lineNumber,
				.type = Literal_Boolean,
				.boolean = token->type == Token_True,
			},
			sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}
	default: INVALID_VALUE(token->type);
	}
}

static Result ParseSizeOf(NodePtr* out)
{
	const int lineNumber = CurrentToken()->lineNumber;

	if (!MatchOne(Token_SizeOf))
		return NOT_FOUND_RESULT;

	if (!MatchOne(Token_LeftBracket))
		return ERROR_RESULT_LINE("Expected \"(\" after \"sizeof\"");

	NodePtr expr = NULL_NODE;
	Type type = (Type){.expr = NULL_NODE};
	PROPAGATE_ERROR(ParseExpression(&expr));
	if (expr.ptr == NULL)
	{
		PROPAGATE_ERROR(ParseType(&type));
		if (type.expr.ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression or type after \"sizeof\"");

		if (type.expr.type == Node_MemberAccess &&
			type.modifier == TypeModifier_None)
			expr = CopyASTNode(type.expr);
	}

	if (!MatchOne(Token_RightBracket))
		return ERROR_RESULT_LINE("Expected \")\" after \"sizeof\"");

	*out = AllocASTNode(
		&(SizeOfExpr){
			.lineNumber = lineNumber,
			.expr = expr,
			.type = type,
		},
		sizeof(SizeOfExpr), Node_SizeOf);

	return SUCCESS_RESULT;
}

static Result ParseExpressionOrVariableDeclaration(NodePtr* out, void* data)
{
	(void)data;
	PROPAGATE_FOUND(ParseExpression(out));
	PROPAGATE_FOUND(ParseFullVarDeclNoSemicolon(out, NULL));
	return NOT_FOUND_RESULT;
}

static Result ParseFunctionCall(NodePtr expr, NodePtr* out)
{
	const int lineNumber = CurrentToken()->lineNumber;

	if (MatchOne(Token_LeftBracket) == NULL)
		return NOT_FOUND_RESULT;

	Array params;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseExpressionOrVariableDeclaration, NULL, Token_RightBracket, NULL));

	*out = AllocASTNode(
		&(FuncCallExpr){
			.lineNumber = lineNumber,
			.baseExpr = expr,
			.arguments = params,
		},
		sizeof(FuncCallExpr), Node_FunctionCall);
	return SUCCESS_RESULT;
}

static Result ParseSubscript(NodePtr expr, NodePtr* out, bool* isDereference)
{
	if (isDereference)
		*isDereference = false;

	Token* firstToken = MatchOne(Token_LeftSquareBracket);
	if (firstToken == NULL)
		firstToken = MatchOne(Token_MinusRightAngle);
	if (firstToken == NULL)
		return NOT_FOUND_RESULT;

	if (firstToken->type == Token_MinusRightAngle)
	{
		if (isDereference)
			*isDereference = true;

		// syntax sugar for dereferencing
		*out = AllocASTNode(
			&(SubscriptExpr){
				.lineNumber = firstToken->lineNumber,
				.baseExpr = expr,
				.indexExpr = AllocASTNode(
					&(LiteralExpr){
						.lineNumber = firstToken->lineNumber,
						.type = Literal_Number,
						.number = AllocateString("0"),
					},
					sizeof(LiteralExpr), Node_Literal),
			},
			sizeof(SubscriptExpr), Node_Subscript);

		return SUCCESS_RESULT;
	}

	NodePtr indexExpr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&indexExpr));

	if (MatchOne(Token_RightSquareBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"]\"");

	*out = AllocASTNode(
		&(SubscriptExpr){
			.lineNumber = firstToken->lineNumber,
			.baseExpr = expr,
			.indexExpr = indexExpr,
		},
		sizeof(SubscriptExpr), Node_Subscript);

	return SUCCESS_RESULT;
}

static Result ParseCallOrSubscript(NodePtr* inout, bool* isDereference)
{
	PROPAGATE_FOUND(ParseFunctionCall(*inout, inout));
	PROPAGATE_FOUND(ParseSubscript(*inout, inout, isDereference));
	return NOT_FOUND_RESULT;
}

static Result ContinueParsePrimary(NodePtr* inout, bool alreadyParsedDot)
{
	const int lineNumber = CurrentToken()->lineNumber;

	Token* dot = NULL;
	if (!alreadyParsedDot)
		dot = MatchOne(Token_Dot);

	if (dot != NULL || alreadyParsedDot)
	{
		Array identifiers;
		PROPAGATE_ERROR(ParseIdentifierChain(&identifiers));
		if (identifiers.array == NULL)
			return ERROR_RESULT_LINE("Expected identifier after member access");

		*inout = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = lineNumber,
				.start = *inout,
				.identifiers = identifiers,
				.funcReference = NULL,
				.typeReference = NULL,
				.varReference = NULL,
				.parentReference = NULL,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess);
	}

	bool isDereference = false;
	const Result result = ParseCallOrSubscript(inout, &isDereference);
	PROPAGATE_ERROR(result);
	if (result.type == Result_NotFound && dot == NULL && !alreadyParsedDot)
		return NOT_FOUND_RESULT;

	if (inout->type == Node_Subscript)
	{
		SubscriptExpr* subscript = inout->ptr;
		if (subscript->indexExpr.ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression");
	}

	PROPAGATE_ERROR(ContinueParsePrimary(inout, isDereference));
	return SUCCESS_RESULT;
}

static Result ParseBasePrimary(NodePtr* out, Array* outIdentifiers)
{
	PROPAGATE_FOUND(ParseSizeOf(out));
	PROPAGATE_FOUND(ParseLiteral(out));
	PROPAGATE_FOUND(ParseExpressionInBrackets(out));
	PROPAGATE_FOUND(ParseBlockExpression(out));
	PROPAGATE_FOUND(ParseIdentifierChain(outIdentifiers));
	return NOT_FOUND_RESULT;
}

static Result ParsePrimary(NodePtr* out)
{
	const size_t oldPointer = pointer;
	const int lineNumber = CurrentToken()->lineNumber;

	*out = NULL_NODE;
	Array identifiers = (Array){.array = NULL, .length = 0};
	PROPAGATE_ERROR(ParseBasePrimary(out, &identifiers));

	if (out->ptr == NULL && identifiers.array == NULL)
		return NOT_FOUND_RESULT;

	if (identifiers.array != NULL)
	{
		ASSERT(out->ptr == NULL);
		*out = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = lineNumber,
				.identifiers = identifiers,
				.start = NULL_NODE,
				.funcReference = NULL,
				.typeReference = NULL,
				.varReference = NULL,
				.parentReference = NULL,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess);
	}
	ASSERT(out->ptr != NULL);

	bool isDereference = false;
	PROPAGATE_ERROR(ParseCallOrSubscript(out, &isDereference));

	if (out->type == Node_FunctionCall ||
		out->type == Node_Subscript ||
		out->type == Node_Binary ||
		out->type == Node_Unary ||
		out->type == Node_BlockExpression)
	{
		NodePtr prev = *out;
		const Result result = ContinueParsePrimary(out, isDereference);
		PROPAGATE_ERROR(result);
		if (prev.type == Node_Subscript &&
			((SubscriptExpr*)prev.ptr)->indexExpr.ptr == NULL)
		{
			if (result.type == Result_NotFound && identifiers.array != NULL)
			{
				pointer = oldPointer;
				return NOT_FOUND_RESULT;
			}
			else
				return ERROR_RESULT_LINE("Expected expression");
		}
	}

	return SUCCESS_RESULT;
}

static Result ParsePrefixUnary(NodePtr* out)
{
	TokenType operators[] = {
		Token_Plus,
		Token_Minus,
		Token_Exclamation,
		Token_PlusPlus,
		Token_MinusMinus,
		Token_Asterisk,
	};
	const Token* operator= Match(operators, COUNTOF(operators));
	if (operator== NULL)
		return ParsePrimary(out);

	NodePtr expr;
	if (ParsePrefixUnary(&expr).type != Result_Success)
		return ERROR_RESULT_LINE(
			AllocateString1Str("Expected expression after operator \"%s\"", GetTokenTypeString(operator->type)));

	*out = AllocASTNode(
		&(UnaryExpr){
			.lineNumber = operator->lineNumber,
			.expression = expr,
			.operatorType = tokenTypeToUnaryOperator[operator->type],
			.postfix = false,
		},
		sizeof(UnaryExpr), Node_Unary);
	return SUCCESS_RESULT;
}

static Result ParsePostfixUnary(NodePtr* out)
{
	Result result = ParsePrefixUnary(out);
	if (result.type != Result_Success)
		return result;

	TokenType operators[] = {Token_PlusPlus, Token_MinusMinus};
	Token* operator= Match(operators, COUNTOF(operators));
	if (operator!= NULL)
	{
		*out = AllocASTNode(
			&(UnaryExpr){
				.lineNumber = operator->lineNumber,
				.expression = *out,
				.operatorType = tokenTypeToUnaryOperator[operator->type],
				.postfix = true,
			}, sizeof(UnaryExpr), Node_Unary);
	}

	return SUCCESS_RESULT;
}

typedef Result (*ParseFunc)(NodePtr* out);

static Result ParseRightBinary(
	NodePtr* out,
	const ParseFunc parseFunc,
	const TokenType operators[],
	const size_t operatorsLength)
{
	NodePtr left = NULL_NODE;
	const Result result = parseFunc(&left);
	if (result.type != Result_Success)
		return result;

	Array exprArray = AllocateArray(sizeof(NodePtr));
	ArrayAdd(&exprArray, &left);

	const Token* op = Match(operators, operatorsLength);
	Array operatorArray = AllocateArray(sizeof(Token));
	while (op != NULL)
	{
		ArrayAdd(&operatorArray, op);

		NodePtr right = NULL_NODE;
		PROPAGATE_ERROR(parseFunc(&right));
		if (right.ptr == NULL)
			return ERROR_RESULT_LINE(
				AllocateString1Str("Expected expression after operator \"%s\"", GetTokenTypeString(op->type)));

		ArrayAdd(&exprArray, &right);

		op = Match(operators, operatorsLength);
	}

	NodePtr* expr1 = exprArray.array[exprArray.length - 1];

	for (int i = (int)exprArray.length - 2; i >= 0; --i)
	{
		op = operatorArray.array[i];
		const NodePtr* expr2 = exprArray.array[i];

		*expr1 = AllocASTNode(
			&(BinaryExpr){
				.lineNumber = op->lineNumber,
				.operatorType = tokenTypeToBinaryOperator[op->type],
				.left = *expr2,
				.right = *expr1,
			},
			sizeof(BinaryExpr), Node_Binary);
	}

	*out = *expr1;
	FreeArray(&exprArray);
	FreeArray(&operatorArray);

	return SUCCESS_RESULT;
}

static Result ParseLeftBinary(
	NodePtr* out,
	const ParseFunc parseFunc,
	const TokenType operators[],
	const size_t operatorsLength)
{
	NodePtr left = NULL_NODE;
	const Result result = parseFunc(&left);
	if (result.type != Result_Success)
		return result;

	const Token* op = Match(operators, operatorsLength);
	while (op != NULL)
	{
		NodePtr right = NULL_NODE;
		PROPAGATE_ERROR(parseFunc(&right));
		if (right.ptr == NULL)
			return ERROR_RESULT_LINE(
				AllocateString1Str("Expected expression after operator \"%s\"", GetTokenTypeString(op->type)));

		left = AllocASTNode(
			&(BinaryExpr){
				.lineNumber = op->lineNumber,
				.operatorType = tokenTypeToBinaryOperator[op->type],
				.left = left,
				.right = right,
			},
			sizeof(BinaryExpr), Node_Binary);

		op = Match(operators, operatorsLength);
	}

	*out = left;
	return SUCCESS_RESULT;
}

static Result ParseExponentiation(NodePtr* out)
{
	TokenType operators[] = {Token_Caret};
	return ParseLeftBinary(out, ParsePostfixUnary, operators, COUNTOF(operators));
}

static Result ParseMultiplicative(NodePtr* out)
{
	TokenType operators[] = {
		Token_Asterisk,
		Token_Slash,
		Token_Percent,
	};
	return ParseLeftBinary(out, ParseExponentiation, operators, COUNTOF(operators));
}

static Result ParseAdditive(NodePtr* out)
{
	TokenType operators[] = {
		Token_Plus,
		Token_Minus,
	};
	return ParseLeftBinary(out, ParseMultiplicative, operators, COUNTOF(operators));
}

static Result ParseBitShift(NodePtr* out)
{
	TokenType operators[] = {
		Token_LeftAngleLeftAngle,
		Token_RightAngleRightAngle,
	};
	return ParseLeftBinary(out, ParseAdditive, operators, COUNTOF(operators));
}

static Result ParseRelational(NodePtr* out)
{
	TokenType operators[] = {
		Token_RightAngleBracket,
		Token_RightAngleEquals,
		Token_LeftAngleBracket,
		Token_LeftAngleEquals,
	};
	return ParseLeftBinary(out, ParseBitShift, operators, COUNTOF(operators));
}

static Result ParseEquality(NodePtr* out)
{
	TokenType operators[] = {
		Token_EqualsEquals,
		Token_ExclamationEquals,
	};
	return ParseLeftBinary(out, ParseRelational, operators, COUNTOF(operators));
}

static Result ParseBitwiseAnd(NodePtr* out)
{
	TokenType operators[] = {
		Token_Ampersand,
	};
	return ParseLeftBinary(out, ParseEquality, operators, COUNTOF(operators));
}

static Result ParseXOR(NodePtr* out)
{
	TokenType operators[] = {
		Token_Tilde,
	};
	return ParseLeftBinary(out, ParseBitwiseAnd, operators, COUNTOF(operators));
}

static Result ParseBitwiseOr(NodePtr* out)
{
	TokenType operators[] = {
		Token_Pipe,
	};
	return ParseLeftBinary(out, ParseXOR, operators, COUNTOF(operators));
}

static Result ParseBooleanAnd(NodePtr* out)
{
	TokenType operators[] = {
		Token_AmpersandAmpersand,
	};
	return ParseLeftBinary(out, ParseBitwiseOr, operators, COUNTOF(operators));
}

static Result ParseBooleanOr(NodePtr* out)
{
	TokenType operators[] = {
		Token_PipePipe,
	};
	return ParseLeftBinary(out, ParseBooleanAnd, operators, COUNTOF(operators));
}

static Result ParseAssignment(NodePtr* out)
{
	TokenType operators[] = {
		Token_Equals,
		Token_PlusEquals,
		Token_MinusEquals,
		Token_AsteriskEquals,
		Token_SlashEquals,
		Token_PercentEquals,
		Token_CaretEquals,
		Token_AmpersandEquals,
		Token_PipeEquals,
		Token_TildeEquals,
	};
	return ParseRightBinary(out, ParseBooleanOr, operators, COUNTOF(operators));
}

static Result ParseExpression(NodePtr* out)
{
	return ParseAssignment(out);
}

static Result ParseExpressionStatement(NodePtr* out)
{
	const size_t oldPointer = pointer;

	NodePtr expr = NULL_NODE;
	const Result result = ParseExpression(&expr);
	if (result.type != Result_Success)
		return result;

	// special case i mean hack for variable declarations
	if (expr.type == Node_Binary)
	{
		BinaryExpr* binary = expr.ptr;
		if (binary->operatorType == Binary_Assignment &&
			binary->left.type == Node_Binary)
		{
			BinaryExpr* left = binary->left.ptr;
			if (left->operatorType == Binary_Multiply)
			{
				pointer = oldPointer;
				return NOT_FOUND_RESULT;
			}
		}
		else if (binary->operatorType == Binary_Multiply)
		{
			pointer = oldPointer;
			return NOT_FOUND_RESULT;
		}
	}

	// special case i mean hack for variable declarations
	if (expr.type == Node_MemberAccess)
	{
		const size_t before = pointer;
		if (MatchOne(Token_Identifier))
		{
			pointer = oldPointer;
			return NOT_FOUND_RESULT;
		}
		if (MatchOne(Token_LeftSquareBracket) &&
			MatchOne(Token_RightSquareBracket) &&
			MatchOne(Token_Identifier))
		{
			pointer = oldPointer;
			return NOT_FOUND_RESULT;
		}
		pointer = before;
	}

	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = ((Token*)tokens.array[pointer])->lineNumber,
			.expr = expr,
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
	return SUCCESS_RESULT;
}

static Result ParseReturnStatement(NodePtr* out)
{
	const Token* returnToken = MatchOne(Token_Return);
	if (returnToken == NULL)
		return NOT_FOUND_RESULT;

	NodePtr expr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&expr));

	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(ReturnStmt){
			.lineNumber = returnToken->lineNumber,
			.expr = expr,
		},
		sizeof(ReturnStmt), Node_Return);
	return SUCCESS_RESULT;
}

static void WrapStatementInBlock(NodePtr* node, int lineNumber)
{
	Array statements = AllocateArray(sizeof(NodePtr));
	ArrayAdd(&statements, node);

	*node = AllocASTNode(
		&(BlockStmt){
			.lineNumber = lineNumber,
			.statements = statements,
		},
		sizeof(BlockStmt), Node_BlockStatement);
}

static Result ParseIfStatement(NodePtr* out)
{
	const Token* ifToken = MatchOne(Token_If);
	if (ifToken == NULL)
		return NOT_FOUND_RESULT;

	if (MatchOne(Token_LeftBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"(\"");

	NodePtr expr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&expr));
	if (expr.ptr == NULL)
		return ERROR_RESULT_LINE("Expected expression in if statement");

	if (MatchOne(Token_RightBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

	NodePtr stmt = NULL_NODE;
	PROPAGATE_ERROR(ParseStatement(&stmt));
	if (stmt.ptr == NULL)
		return ERROR_RESULT_LINE("Expected statement");

	if (stmt.type != Node_BlockStatement)
		WrapStatementInBlock(&stmt, ifToken->lineNumber);

	NodePtr elseStmt = NULL_NODE;
	const Token* elseToken = MatchOne(Token_Else);
	if (elseToken != NULL)
	{
		PROPAGATE_ERROR(ParseStatement(&elseStmt));
		if (elseStmt.ptr == NULL)
			return ERROR_RESULT_LINE("Expected statement after \"else\"");

		if (elseStmt.type != Node_BlockStatement)
			WrapStatementInBlock(&elseStmt, elseToken->lineNumber);
	}

	*out = AllocASTNode(
		&(IfStmt){
			.lineNumber = ifToken->lineNumber,
			.expr = expr,
			.trueStmt = stmt,
			.falseStmt = elseStmt,
		},
		sizeof(IfStmt), Node_If);
	return SUCCESS_RESULT;
}

static Result ParseBlockStatement(NodePtr* out)
{
	const Token* openingBrace = MatchOne(Token_LeftCurlyBracket);
	if (openingBrace == NULL)
		return NOT_FOUND_RESULT;

	Array statements = AllocateArray(sizeof(NodePtr));
	while (true)
	{
		NodePtr stmt = NULL_NODE;
		PROPAGATE_ERROR(ParseStatement(&stmt));
		if (stmt.ptr == NULL)
			break;

		if (stmt.type == Node_Section)
			return ERROR_RESULT_LINE("Nested sections are not allowed");

		if (stmt.type == Node_StructDeclaration)
			return ERROR_RESULT_LINE("Struct declarations not allowed inside code blocks");

		ArrayAdd(&statements, &stmt);
	}

	if (MatchOne(Token_RightCurlyBracket) == NULL)
		return ERROR_RESULT_LINE(AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));

	*out = AllocASTNode(
		&(BlockStmt){
			.lineNumber = openingBrace->lineNumber,
			.statements = statements,
		},
		sizeof(BlockStmt), Node_BlockStatement);
	return SUCCESS_RESULT;
}

static Result ParseSectionStatement(NodePtr* out)
{
	if (MatchOne(Token_At) == NULL)
		return NOT_FOUND_RESULT;

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
		return ERROR_RESULT_LINE("Expected identifier after \"@\"");

	char sectionNames[6][10] =
		{
			"init",
			"slider",
			"block",
			"sample",
			"serialize",
			"gfx",
		};
	SectionType sectionTypes[] =
		{
			Section_Init,
			Section_Slider,
			Section_Block,
			Section_Sample,
			Section_Serialize,
			Section_GFX,
		};

	SectionType sectionType = Section_Init;
	bool sectionFound = false;
	for (size_t i = 0; i < COUNTOF(sectionTypes); ++i)
	{
		if (strncmp(identifier->text, sectionNames[i], identifier->textSize) == 0)
		{
			sectionType = sectionTypes[i];
			sectionFound = true;
			break;
		}
	}
	if (!sectionFound)
		return ERROR_RESULT_LINE("Unknown section type");

	NodePtr list = NULL_NODE;
	PROPAGATE_ERROR(ParsePropertyList(&list));

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL)
		return ERROR_RESULT_LINE("Expected block after section statement");
	ASSERT(block.type == Node_BlockStatement);

	*out = AllocASTNode(
		&(SectionStmt){
			.lineNumber = identifier->lineNumber,
			.sectionType = sectionType,
			.block = block,
			.propertyList = list,
		},
		sizeof(SectionStmt), Node_Section);
	return SUCCESS_RESULT;
}

static Result ParseExternalIdentifier(char** externalIdentifier)
{
	if (!MatchOne(Token_As))
		return NOT_FOUND_RESULT;

	const Token* token = MatchOne(Token_Identifier);
	if (token == NULL)
		return ERROR_RESULT_LINE("Expected identifier after \"as\"");

	*externalIdentifier = AllocateStringLength(token->text, token->textSize);
	return SUCCESS_RESULT;
}

static Result ParseVariableDeclaration(
	NodePtr* out,
	ModifierState modifiers,
	Type type,
	const Token* identifier,
	bool expectSemicolon,
	bool allowInitializer)
{
	char* externalIdentifier = NULL;
	PROPAGATE_ERROR(ParseExternalIdentifier(&externalIdentifier));

	NodePtr initializer = NULL_NODE;
	const Token* equals = MatchOne(Token_Equals);
	if (equals != NULL)
	{
		if (!allowInitializer)
			return ERROR_RESULT_LINE("Variable initializers are not allowed here");

		PROPAGATE_ERROR(ParseExpression(&initializer));
		if (initializer.ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression");
	}

	if (expectSemicolon)
	{
		const Token* semicolon = MatchOne(Token_Semicolon);
		if (semicolon == NULL)
			return ERROR_RESULT_LINE(
				AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));
	}

	*out = AllocASTNode(
		&(VarDeclStmt){
			.type = type,
			.lineNumber = identifier->lineNumber,
			.name = AllocateStringLength(identifier->text, identifier->textSize),
			.externalName = externalIdentifier,
			.initializer = initializer,
			.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
			.modifiers = modifiers,
			.uniqueName = -1,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseFullVarDeclNoSemicolon(NodePtr* out, void* data)
{
	(void)data;

	Type type;
	const Token* identifier;
	PROPAGATE_ERROR(ParseTypeAndIdentifier(&type, &identifier));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	return ParseVariableDeclaration(out, (ModifierState){.publicSpecified = false}, type, identifier, false, false);
}

static Result ParseFunctionDeclaration(NodePtr* out, ModifierState modifiers, const Type type, const Token* identifier)
{
	if (MatchOne(Token_LeftBracket) == NULL)
		return NOT_FOUND_RESULT;

	Array params;
	bool variadic = false;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseFullVarDeclNoSemicolon, NULL, Token_RightBracket, &variadic));

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));

	char* externalIdentifier = NULL;
	PROPAGATE_ERROR(ParseExternalIdentifier(&externalIdentifier));

	if (!block.ptr)
		if (!MatchOne(Token_Semicolon))
			return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(FuncDeclStmt){
			.type = type,
			.oldType = (Type){.expr = NULL_NODE, .modifier = TypeModifier_None},
			.lineNumber = identifier->lineNumber,
			.name = AllocateStringLength(identifier->text, identifier->textSize),
			.externalName = externalIdentifier,
			.parameters = params,
			.oldParameters = AllocateArray(sizeof(NodePtr)),
			.block = block,
			.globalReturn = NULL,
			.modifiers = modifiers,
			.variadic = variadic,
			.uniqueName = -1,
		},
		sizeof(FuncDeclStmt), Node_FunctionDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseStructDeclaration(NodePtr* out, ModifierState modifiers)
{
	if (MatchOne(Token_Struct) == NULL)
		return NOT_FOUND_RESULT;

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
		return ERROR_RESULT_LINE("Expected struct name");

	if (MatchOne(Token_LeftCurlyBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"{\" after struct name");

	Array members = AllocateArray(sizeof(NodePtr));
	while (true)
	{
		NodePtr member = NULL_NODE;
		PROPAGATE_ERROR(ParseFullVarDeclNoSemicolon(&member, NULL));
		if (member.ptr == NULL)
			break;

		if (MatchOne(Token_Semicolon) == NULL)
			return ERROR_RESULT_LINE("Expected \";\"");

		ArrayAdd(&members, &member);
	}

	if (MatchOne(Token_RightCurlyBracket) == NULL)
		return ERROR_RESULT_LINE(
			AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));

	*out = AllocASTNode(
		&(StructDeclStmt){
			.lineNumber = identifier->lineNumber,
			.name = AllocateStringLength(identifier->text, identifier->textSize),
			.members = members,
			.modifiers = modifiers,
			.isArrayType = false,
		},
		sizeof(StructDeclStmt), Node_StructDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseImportStatement(NodePtr* out, ModifierState modifiers)
{
	const Token* import = MatchOne(Token_Import);
	if (import == NULL)
		return NOT_FOUND_RESULT;

	const Token* path = MatchOne(Token_StringLiteral);
	if (path == NULL)
		return ERROR_RESULT_LINE("Expected path after \"import\"");

	*out = AllocASTNode(
		&(ImportStmt){
			.lineNumber = import->lineNumber,
			.path = AllocateStringLength(path->text, path->textSize),
			.moduleName = NULL,
			.modifiers = modifiers,
		},
		sizeof(ImportStmt), Node_Import);
	return SUCCESS_RESULT;
}

static Result ParseTypeAndIdentifier(Type* type, const Token** identifier)
{
	PROPAGATE_ERROR(ParseType(type));
	if (type->expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	*identifier = MatchOne(Token_Identifier);
	if (*identifier == NULL)
		return ERROR_RESULT_LINE("Expected identifier after type");

	return SUCCESS_RESULT;
}

static Result ParseDeclaration(NodePtr* out, ModifierState modifiers)
{
	Type type;
	const Token* identifier;
	PROPAGATE_ERROR(ParseTypeAndIdentifier(&type, &identifier));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	PROPAGATE_FOUND(ParseFunctionDeclaration(out, modifiers, type, identifier));
	return ParseVariableDeclaration(out, modifiers, type, identifier, true, true);
}

static Result ParseModifiers(ModifierState* outModifierState, bool* outHasAnyModifiers)
{
	ASSERT(outModifierState);
	*outModifierState = (ModifierState){
		.publicSpecified = false,
		.publicValue = false,
		.externalSpecified = false,
		.externalValue = false,
	};

	bool _;
	if (!outHasAnyModifiers)
		outHasAnyModifiers = &_;

	*outHasAnyModifiers = false;

	Token* token = NULL;
	TokenType operators[] = {
		Token_Public,
		Token_Private,
		Token_External,
		Token_Internal,
	};
	while ((token = Match(operators, COUNTOF(operators))))
	{
		*outHasAnyModifiers = true;

		switch (token->type)
		{
		case Token_Public:
		{
			if (outModifierState->publicSpecified)
				goto moreThanOne;

			outModifierState->publicSpecified = true;
			outModifierState->publicValue = true;
			break;
		}
		case Token_Private:
		{
			if (outModifierState->publicSpecified)
				goto moreThanOne;

			outModifierState->publicSpecified = true;
			outModifierState->publicValue = false;
			break;
		}
		case Token_External:
		{
			if (outModifierState->externalSpecified)
				goto moreThanOne;

			outModifierState->externalSpecified = true;
			outModifierState->externalValue = true;
			break;
		}
		case Token_Internal:
		{
			if (outModifierState->externalSpecified)
				goto moreThanOne;

			outModifierState->externalSpecified = true;
			outModifierState->externalValue = false;
			break;
		}
		default: INVALID_VALUE(token->type);
		}
	}

	return *outHasAnyModifiers ? SUCCESS_RESULT : NOT_FOUND_RESULT;

moreThanOne:
	return ERROR_RESULT_LINE("Cannot have more than one modifier of the same type");
}

static Result StringToPropertyType(const char* string, size_t stringSize, PropertyType* out)
{
	if (strncmp(string, "default_value", stringSize) == 0)
		*out = PropertyType_DefaultValue;
	else if (strncmp(string, "min", stringSize) == 0)
		*out = PropertyType_Min;
	else if (strncmp(string, "max", stringSize) == 0)
		*out = PropertyType_Max;
	else if (strncmp(string, "increment", stringSize) == 0)
		*out = PropertyType_Increment;
	else if (strncmp(string, "description", stringSize) == 0)
		*out = PropertyType_Description;
	else if (strncmp(string, "hidden", stringSize) == 0)
		*out = PropertyType_Hidden;
	else if (strncmp(string, "shape", stringSize) == 0)
		*out = PropertyType_Shape;
	else if (strncmp(string, "midpoint", stringSize) == 0)
		*out = PropertyType_Midpoint;
	else if (strncmp(string, "exponent", stringSize) == 0)
		*out = PropertyType_Exponent;
	else if (strncmp(string, "linear_automation", stringSize) == 0)
		*out = PropertyType_LinearAutomation;
	else if (strncmp(string, "type", stringSize) == 0)
		*out = PropertyType_Type;
	else if (strncmp(string, "width", stringSize) == 0)
		*out = PropertyType_Width;
	else if (strncmp(string, "height", stringSize) == 0)
		*out = PropertyType_Height;
	else if (strncmp(string, "name", stringSize) == 0)
		*out = PropertyType_Name;
	else if (strncmp(string, "tags", stringSize) == 0)
		*out = PropertyType_Tags;
	else if (strncmp(string, "pin", stringSize) == 0)
		*out = PropertyType_Pin;
	else if (strncmp(string, "in_pins", stringSize) == 0)
		*out = PropertyType_InPins;
	else if (strncmp(string, "out_pins", stringSize) == 0)
		*out = PropertyType_OutPins;
	else if (strncmp(string, "options", stringSize) == 0)
		*out = PropertyType_Options;
	else if (strncmp(string, "all_keyboard", stringSize) == 0)
		*out = PropertyType_AllKeyboard;
	else if (strncmp(string, "max_memory", stringSize) == 0)
		*out = PropertyType_MaxMemory;
	else if (strncmp(string, "no_meter", stringSize) == 0)
		*out = PropertyType_NoMeter;
	else if (strncmp(string, "gfx", stringSize) == 0)
		*out = PropertyType_GFX;
	else if (strncmp(string, "idle_mode", stringSize) == 0)
		*out = PropertyType_IdleMode;
	else if (strncmp(string, "hz", stringSize) == 0)
		*out = PropertyType_HZ;
	else
		return ERROR_RESULT_LINE("Invalid property type");

	return SUCCESS_RESULT;
}

static Result ParseProperty(NodePtr* out, void* data)
{
	(void)data;

	Token* property = MatchOne(Token_Identifier);
	if (!property)
		return NOT_FOUND_RESULT;

	PropertyType propertyType;
	PROPAGATE_ERROR(StringToPropertyType(property->text, property->textSize, &propertyType));

	if (!MatchOne(Token_Colon))
		return ERROR_RESULT_LINE("Expected \":\"");

	NodePtr value = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&value));
	if (!value.ptr)
		PROPAGATE_ERROR(ParsePropertyList(&value));
	if (!value.ptr)
		return ERROR_RESULT_LINE("Expected value");

	*out = AllocASTNode(
		&(PropertyNode){
			.lineNumber = property->lineNumber,
			.type = propertyType,
			.value = value,
		},
		sizeof(PropertyNode), Node_Property);
	return SUCCESS_RESULT;
}

static Result ParsePropertyList(NodePtr* out)
{
	if (!MatchOne(Token_LeftSquareBracket))
		return NOT_FOUND_RESULT;

	Array list;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&list, ParseProperty, NULL, Token_RightSquareBracket, false));

	*out = AllocASTNode(
		&(PropertyListNode){
			.list = list,
		},
		sizeof(PropertyListNode), Node_PropertyList);
	return SUCCESS_RESULT;
}

static Result ParseInputStatement(NodePtr* out, ModifierState modifiers)
{
	if (!MatchOne(Token_Input))
		return NOT_FOUND_RESULT;

	Token* name = MatchOne(Token_Identifier);
	if (!name)
		return ERROR_RESULT_LINE("Expected input name");

	NodePtr list = NULL_NODE;
	PROPAGATE_ERROR(ParsePropertyList(&list));

	if (!MatchOne(Token_Semicolon))
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(InputStmt){
			.lineNumber = name->lineNumber,
			.name = AllocateStringLength(name->text, name->textSize),
			.propertyList = list,
			.modifiers = modifiers,
		},
		sizeof(InputStmt), Node_Input);
	return SUCCESS_RESULT;
}

static Result ParseModifierDeclaration(NodePtr* out)
{
	ModifierState modifierState;
	bool hasAnyModifiers;
	PROPAGATE_ERROR(ParseModifiers(&modifierState, &hasAnyModifiers));

	PROPAGATE_FOUND(ParseInputStatement(out, modifierState));
	PROPAGATE_FOUND(ParseStructDeclaration(out, modifierState));
	PROPAGATE_FOUND(ParseImportStatement(out, modifierState));
	PROPAGATE_FOUND(ParseDeclaration(out, modifierState));

	if (!hasAnyModifiers)
		return NOT_FOUND_RESULT;

	return ERROR_RESULT_LINE("Expected declaration after modifiers");
}

static Result ParseWhileStatement(NodePtr* out)
{
	const Token* whileToken = MatchOne(Token_While);
	if (whileToken == NULL) return NOT_FOUND_RESULT;

	if (MatchOne(Token_LeftBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"(\"");

	NodePtr expr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&expr));
	if (expr.ptr == NULL)
		return ERROR_RESULT_LINE("Expected expression");

	if (MatchOne(Token_RightBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

	NodePtr stmt = NULL_NODE;
	PROPAGATE_ERROR(ParseStatement(&stmt));
	if (stmt.ptr == NULL)
		return ERROR_RESULT_LINE("Expected statement for loop body");

	if (stmt.type != Node_BlockStatement)
		WrapStatementInBlock(&stmt, whileToken->lineNumber);

	*out = AllocASTNode(
		&(WhileStmt){
			.lineNumber = whileToken->lineNumber,
			.expr = expr,
			.stmt = stmt,
		},
		sizeof(WhileStmt), Node_While);
	return SUCCESS_RESULT;
}

static Result ParseForStatement(NodePtr* out)
{
	const Token* forToken = MatchOne(Token_For);
	if (forToken == NULL)
		return NOT_FOUND_RESULT;

	if (MatchOne(Token_LeftBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"(\"");

	NodePtr initializer = NULL_NODE;
	PROPAGATE_ERROR(ParseStatement(&initializer));

	if (initializer.type != Node_Null &&
		initializer.type != Node_VariableDeclaration &&
		initializer.type != Node_ExpressionStatement)
		return ERROR_RESULT_LINE(
			"Only expression and variable declaration statements are allowed inside for loop initializers");

	if (initializer.type == Node_Null)
	{
		if (MatchOne(Token_Semicolon) == NULL)
			return ERROR_RESULT_LINE("Expected \";\"");
	}

	NodePtr condition = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&condition));

	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	NodePtr increment = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&increment));

	if (MatchOne(Token_RightBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

	NodePtr stmt = NULL_NODE;
	PROPAGATE_ERROR(ParseStatement(&stmt));
	if (stmt.ptr == NULL)
		return ERROR_RESULT_LINE("Expected statement for loop body");

	if (stmt.type != Node_BlockStatement)
		WrapStatementInBlock(&stmt, forToken->lineNumber);

	*out = AllocASTNode(
		&(ForStmt){
			.lineNumber = forToken->lineNumber,
			.initialization = initializer,
			.condition = condition,
			.increment = increment,
			.stmt = stmt,
		},
		sizeof(ForStmt), Node_For);
	return SUCCESS_RESULT;
}

static Result ParseLoopControlStatement(NodePtr* out)
{
	TokenType operators[] = {Token_Break, Token_Continue};
	const Token* token = Match(operators, COUNTOF(operators));
	if (token == NULL) return NOT_FOUND_RESULT;

	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(LoopControlStmt){
			.lineNumber = token->lineNumber,
			.type = token->type == Token_Break ? LoopControl_Break : LoopControl_Continue,
		},
		sizeof(LoopControlStmt), Node_LoopControl);
	return SUCCESS_RESULT;
}

static Result ParseModifierStatement(NodePtr* out)
{
	int lineNumber = CurrentToken()->lineNumber;
	size_t oldPointer = pointer;

	ModifierState modifierState;
	bool hasAnyModifiers;
	PROPAGATE_ERROR(ParseModifiers(&modifierState, &hasAnyModifiers));

	if (!MatchOne(Token_Colon))
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	*out = AllocASTNode(
		&(ModifierStmt){
			.lineNumber = lineNumber,
			.modifierState = modifierState,
		},
		sizeof(ModifierStmt), Node_Modifier);
	return SUCCESS_RESULT;
}

static Result ParseDescStatement(NodePtr* out)
{
	Token* keyword = MatchOne(Token_Desc);
	if (!keyword)
		return NOT_FOUND_RESULT;

	NodePtr list = NULL_NODE;
	PROPAGATE_ERROR(ParsePropertyList(&list));

	if (!MatchOne(Token_Semicolon))
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(DescStmt){
			.lineNumber = keyword->lineNumber,
			.propertyList = list,
		},
		sizeof(DescStmt), Node_Desc);
	return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out)
{
	PROPAGATE_FOUND(ParseIfStatement(out));
	PROPAGATE_FOUND(ParseLoopControlStatement(out));
	PROPAGATE_FOUND(ParseWhileStatement(out));
	PROPAGATE_FOUND(ParseForStatement(out));
	PROPAGATE_FOUND(ParseSectionStatement(out));
	PROPAGATE_FOUND(ParseDescStatement(out));
	PROPAGATE_FOUND(ParseBlockStatement(out));
	PROPAGATE_FOUND(ParseReturnStatement(out));
	PROPAGATE_FOUND(ParseModifierStatement(out));
	PROPAGATE_FOUND(ParseExpressionStatement(out));
	PROPAGATE_FOUND(ParseModifierDeclaration(out));
	return NOT_FOUND_RESULT;
}

static Result ParseProgram(AST* out)
{
	Array stmtArray = AllocateArray(sizeof(NodePtr));

	bool allowImportStatements = true;
	while (true)
	{
		if (MatchOne(Token_EndOfFile) != NULL)
			break;

		NodePtr stmt = NULL_NODE;

		PROPAGATE_ERROR(ParseStatement(&stmt));
		if (stmt.ptr == NULL)
			return ERROR_RESULT_LINE(
				AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));

		if (stmt.type != Node_Import &&
			stmt.type != Node_Modifier)
			allowImportStatements = false;

		if (stmt.type == Node_Import && !allowImportStatements)
			return ERROR_RESULT_LINE("Import statements must be at the top of the file");

		if (stmt.type != Node_Section &&
			stmt.type != Node_FunctionDeclaration &&
			stmt.type != Node_StructDeclaration &&
			stmt.type != Node_VariableDeclaration &&
			stmt.type != Node_Modifier &&
			stmt.type != Node_Import &&
			stmt.type != Node_Input &&
			stmt.type != Node_Desc)
			return ERROR_RESULT_LINE(
				"Expected section statement, variable declaration, struct declaration, or function declaration");

		ArrayAdd(&stmtArray, &stmt);
	}

	*out = (AST){.nodes = stmtArray};
	return SUCCESS_RESULT;
}

Result Parse(const char* path, const Array* tokenArray, AST* outSyntaxTree)
{
	currentFile = path;

	pointer = 0;
	tokens = *tokenArray;

	return ParseProgram(outSyntaxTree);
}
