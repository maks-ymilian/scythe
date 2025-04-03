#include "Parser.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"

#define ERROR_RESULT_LINE(message) ERROR_RESULT(message, CurrentToken()->lineNumber, NULL)

static Array tokens;
static size_t pointer;

static Token* CurrentToken()
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

static Token* MatchOne(const TokenType type) { return Match((TokenType[]){type}, 1); }

static bool IsDigitBase(const char c, const int base)
{
	if (base == 10) return isdigit(c);
	if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	if (base == 2) return c == '0' || c == '1';
	if (base == 8) return c >= '0' && c <= '7';
	INVALID_VALUE(base);
}

static Result StringToUInt64(const char* string, const int base, const int lineNumber, uint64_t* out)
{
	assert(
		base == 10 ||
		base == 16 ||
		base == 8 ||
		base == 2);

	for (int i = 0; string[i] != '\0'; ++i)
		if (!IsDigitBase(string[i], base))
			return ERROR_RESULT("Invalid integer literal", lineNumber, NULL);

	*out = strtoull(string, NULL, base);

	if (*out == UINT64_MAX)
		return ERROR_RESULT("Invalid integer literal", lineNumber, NULL);

	if (*out == 0)
	{
		bool isZero = true;
		for (int i = 0; string[i] != '\0'; ++i)
			if (string[i] != '0') isZero = false;

		if (!isZero)
			return ERROR_RESULT("Invalid integer literal", lineNumber, NULL);
	}

	return SUCCESS_RESULT;
}

static Result EvaluateNumberLiteral(
	const char* string,
	const int lineNumber,
	bool* outIsInteger,
	double* outFloat,
	uint64_t* outInt)
{
	const size_t length = strlen(string);
	char text[length + 1];
	memcpy(text, string, length + 1);

	*outIsInteger = true;
	for (int i = 0; text[i] != '\0'; ++i)
		if (text[i] == '.') *outIsInteger = false;

	if (!*outIsInteger)
	{
		int consumedChars;
		if (sscanf(text, "%lf%n", outFloat, &consumedChars) != 1)
			return ERROR_RESULT("Invalid float literal", lineNumber, NULL);
		if (fpclassify(*outFloat) != FP_NORMAL && fpclassify(*outFloat) != FP_ZERO)
			return ERROR_RESULT("Invalid float literal", lineNumber, NULL);
		if (text[consumedChars] != '\0')
			return ERROR_RESULT("Invalid float literal", lineNumber, NULL);

		return SUCCESS_RESULT;
	}

	size_t index = 0;
	int base = 10;
	if (text[index] == '0')
	{
		if (tolower(text[index + 1]) == 'x')
			base = 16;
		else if (tolower(text[index + 1]) == 'o')
			base = 8;
		else if (tolower(text[index + 1]) == 'b')
			base = 2;

		if (base != 10) index += 2;
	}

	PROPAGATE_ERROR(StringToUInt64(text + index, base, lineNumber, outInt));
	return SUCCESS_RESULT;
}

typedef Result (*ParseFunction)(NodePtr*);
static Result ParseCommaSeparatedList(Array* outArray, const ParseFunction function, const TokenType endToken)
{
	*outArray = AllocateArray(sizeof(NodePtr));

	NodePtr firstNode = NULL_NODE;
	PROPAGATE_ERROR(function(&firstNode));
	if (firstNode.ptr != NULL)
	{
		ArrayAdd(outArray, &firstNode);
		while (true)
		{
			if (MatchOne(Token_Comma) == NULL)
				break;

			NodePtr node = NULL_NODE;
			PROPAGATE_ERROR(function(&node));
			if (node.ptr == NULL)
				return ERROR_RESULT_LINE(
					AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken()->type)));

			ArrayAdd(outArray, &node);
		}
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
		if (array->array != NULL && MatchOne(Token_Dot) == NULL)
			break;

		Token* identifier = MatchOne(Token_Identifier);
		if (identifier == NULL)
			return array->array != NULL
					   ? ERROR_RESULT_LINE("Expected identifier after \".\"")
					   : NOT_FOUND_RESULT;

		if (array->array == NULL)
			*array = AllocateArray(sizeof(char*));

		char* copy = AllocateString(identifier->text);
		ArrayAdd(array, &copy);
	}

	return SUCCESS_RESULT;
}

static Result ParseExpression(NodePtr* out);

static bool ParsePrimitiveType(PrimitiveType* out, int* outLineNumber)
{
	const Token* primitiveType = Match(
		(TokenType[]){
			Token_Any,
			Token_Int,
			Token_Float,
			Token_String,
			Token_Bool,
			Token_Void,
		},
		6);

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
					.reference = NULL_NODE,
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

static Result ParseBlockStatement(NodePtr* out);

static Result ParseBlockExpression(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Type type;
	PROPAGATE_ERROR(ParseType(&type));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
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
	const Token* token = Match(
		(TokenType[]){
			Token_NumberLiteral,
			Token_StringLiteral,
			Token_True,
			Token_False,
		},
		4);

	if (token == NULL)
		return NOT_FOUND_RESULT;

	switch (token->type)
	{
	case Token_NumberLiteral:
	{
		bool isInteger;
		double floatValue;
		uint64_t intValue;
		PROPAGATE_ERROR(EvaluateNumberLiteral(token->text, token->lineNumber, &isInteger, &floatValue, &intValue));

		if (isInteger)
			*out = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = token->lineNumber,
					.type = Literal_Int,
					.intValue = intValue,
				},
				sizeof(LiteralExpr), Node_Literal);
		else
			*out = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = token->lineNumber,
					.type = Literal_Float,
					.floatValue = AllocateString(token->text),
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
				.string = AllocateString(token->text),
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

static Result ParseFunctionCall(NodePtr expr, NodePtr* out)
{
	const int lineNumber = CurrentToken()->lineNumber;

	if (MatchOne(Token_LeftBracket) == NULL)
		return NOT_FOUND_RESULT;

	Array params;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseExpression, Token_RightBracket));

	*out = AllocASTNode(
		&(FuncCallExpr){
			.lineNumber = lineNumber,
			.expr = expr,
			.arguments = params,
		},
		sizeof(FuncCallExpr), Node_FunctionCall);
	return SUCCESS_RESULT;
}

static Result ParseSubscript(NodePtr expr, NodePtr* out)
{
	Token* leftBracket = MatchOne(Token_LeftSquareBracket);
	if (leftBracket == NULL)
		return NOT_FOUND_RESULT;

	NodePtr indexExpr = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&indexExpr));

	if (MatchOne(Token_RightSquareBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"]\"");

	*out = AllocASTNode(
		&(SubscriptExpr){
			.lineNumber = leftBracket->lineNumber,
			.expr = expr,
			.indexExpr = indexExpr,
		},
		sizeof(SubscriptExpr), Node_Subscript);

	return SUCCESS_RESULT;
}

static Result ParseCallOrSubscript(NodePtr* inout)
{
	PROPAGATE_FOUND(ParseFunctionCall(*inout, inout));
	PROPAGATE_FOUND(ParseSubscript(*inout, inout));
	return NOT_FOUND_RESULT;
}

static Result ContinueParsePrimary(NodePtr* inout)
{
	const int lineNumber = CurrentToken()->lineNumber;

	Token* dot = MatchOne(Token_Dot);
	if (dot != NULL)
	{
		Array identifiers;
		PROPAGATE_ERROR(ParseIdentifierChain(&identifiers));
		if (identifiers.array == NULL)
			return ERROR_RESULT_LINE("Expected identifier after \".\"");

		*inout = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = lineNumber,
				.start = *inout,
				.identifiers = identifiers,
				.reference = NULL_NODE,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess);
	}

	const Result result = ParseCallOrSubscript(inout);
	PROPAGATE_ERROR(result);
	if (result.type == Result_NotFound && dot == NULL)
		return NOT_FOUND_RESULT;

	if (inout->type == Node_Subscript)
	{
		SubscriptExpr* subscript = inout->ptr;
		if (subscript->indexExpr.ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression");
	}

	PROPAGATE_ERROR(ContinueParsePrimary(inout));
	return SUCCESS_RESULT;
}

static Result ParseBasePrimary(NodePtr* out, Array* outIdentifiers)
{
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
		assert(out->ptr == NULL);
		*out = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = lineNumber,
				.identifiers = identifiers,
				.start = NULL_NODE,
				.reference = NULL_NODE,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess);
	}
	assert(out->ptr != NULL);

	PROPAGATE_ERROR(ParseCallOrSubscript(out));

	if (out->type == Node_FunctionCall ||
		out->type == Node_Subscript)
	{
		NodePtr prev = *out;
		const Result result = ContinueParsePrimary(out);
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

static Result ParseUnary(NodePtr* out)
{
	const Token* operator= Match((TokenType[]){Token_Plus, Token_Minus, Token_Exclamation, Token_PlusPlus, Token_MinusMinus}, 5);
	if (operator== NULL)
		return ParsePrimary(out);

	NodePtr expr;
	if (ParseUnary(&expr).type != Result_Success)
		return ERROR_RESULT_LINE(
			AllocateString1Str("Expected expression after operator \"%s\"", GetTokenTypeString(operator->type)));

	*out = AllocASTNode(
		&(UnaryExpr){
			.lineNumber = operator->lineNumber,
			.expression = expr,
			.operatorType = tokenTypeToUnaryOperator[operator->type],
		},
		sizeof(UnaryExpr), Node_Unary);
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
	return ParseLeftBinary(out, ParseUnary,
		(TokenType[]){
			Token_Caret,
		},
		1);
}

static Result ParseMultiplicative(NodePtr* out)
{
	return ParseLeftBinary(out, ParseExponentiation,
		(TokenType[]){
			Token_Asterisk,
			Token_Slash,
			Token_Percent,
		},
		3);
}

static Result ParseAdditive(NodePtr* out)
{
	return ParseLeftBinary(out, ParseMultiplicative,
		(TokenType[]){
			Token_Plus,
			Token_Minus,
		},
		2);
}

static Result ParseBitShift(NodePtr* out)
{
	return ParseLeftBinary(out, ParseAdditive,
		(TokenType[]){
			Token_LeftAngleLeftAngle,
			Token_RightAngleRightAngle,
		},
		2);
}

static Result ParseRelational(NodePtr* out)
{
	return ParseLeftBinary(out, ParseBitShift,
		(TokenType[]){
			Token_RightAngleBracket,
			Token_RightAngleEquals,
			Token_LeftAngleBracket,
			Token_LeftAngleEquals,
		},
		4);
}

static Result ParseEquality(NodePtr* out)
{
	return ParseLeftBinary(out, ParseRelational,
		(TokenType[]){
			Token_EqualsEquals,
			Token_ExclamationEquals,
		},
		2);
}

static Result ParseBitwiseAnd(NodePtr* out)
{
	return ParseLeftBinary(out, ParseEquality,
		(TokenType[]){
			Token_Ampersand,
		},
		1);
}

static Result ParseXOR(NodePtr* out)
{
	return ParseLeftBinary(out, ParseBitwiseAnd,
		(TokenType[]){
			Token_Tilde,
		},
		1);
}

static Result ParseBitwiseOr(NodePtr* out)
{
	return ParseLeftBinary(out, ParseXOR,
		(TokenType[]){
			Token_Pipe,
		},
		1);
}

static Result ParseBooleanAnd(NodePtr* out)
{
	return ParseLeftBinary(out, ParseBitwiseOr,
		(TokenType[]){
			Token_AmpersandAmpersand,
		},
		1);
}

static Result ParseBooleanOr(NodePtr* out)
{
	return ParseLeftBinary(out, ParseBooleanAnd,
		(TokenType[]){
			Token_PipePipe,
		},
		1);
}

static Result ParseAssignment(NodePtr* out)
{
	return ParseRightBinary(out, ParseBooleanOr,
		(TokenType[]){
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
		},
		10);
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
			.function = NULL,
		},
		sizeof(ReturnStmt), Node_Return);
	return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out);

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

	constexpr char sectionNames[6][10] =
		{
			"init",
			"slider",
			"block",
			"sample",
			"serialize",
			"gfx",
		};
	const SectionType sectionTypes[] =
		{
			Section_Init,
			Section_Slider,
			Section_Block,
			Section_Sample,
			Section_Serialize,
			Section_GFX,
		};

	SectionType sectionType;
	bool sectionFound = false;
	for (size_t i = 0; i < sizeof(sectionTypes) / sizeof(TokenType); ++i)
	{
		if (strcmp(identifier->text, sectionNames[i]) == 0)
		{
			sectionType = sectionTypes[i];
			sectionFound = true;
			break;
		}
	}
	if (!sectionFound)
		return ERROR_RESULT_LINE(AllocateString1Str("Unknown section type \"%s\"", identifier->text));

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL)
		return ERROR_RESULT_LINE("Expected block after section statement");
	assert(block.type == Node_BlockStatement);

	*out = AllocASTNode(
		&(SectionStmt){
			.lineNumber = identifier->lineNumber,
			.sectionType = sectionType,
			.block = block},
		sizeof(SectionStmt), Node_Section);
	return SUCCESS_RESULT;
}

static Result ParseVariableDeclaration(
	NodePtr* out,
	const Token* public,
	const Token* external,
	const Type type,
	const Token* identifier,
	const bool expectSemicolon)
{
	NodePtr initializer = NULL_NODE;
	const Token* equals = MatchOne(Token_Equals);
	if (equals != NULL)
	{
		if (external)
			return ERROR_RESULT_LINE("External variable declarations cannot have an initializer");

		PROPAGATE_ERROR(ParseExpression(&initializer));
		if (initializer.ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression");
	}

	Token externalIdentifier = {};
	if (external)
	{
		const Token* token = MatchOne(Token_Identifier);
		if (token != NULL)
			externalIdentifier = *token;
		else
			externalIdentifier = *identifier;
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
			.name = AllocateString(identifier->text),
			.externalName = AllocateString(externalIdentifier.text),
			.initializer = initializer,
			.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
			.public = public != NULL,
			.external = external != NULL,
			.uniqueName = -1,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseTypeAndIdentifier(Type* type, const Token** identifier);

static Result ParseVarDeclNoSemicolon(NodePtr* out)
{
	Type type;
	const Token* identifier;
	PROPAGATE_ERROR(ParseTypeAndIdentifier(&type, &identifier));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	return ParseVariableDeclaration(out, NULL, NULL, type, identifier, false);
}

static Result ParseFunctionDeclaration(
	NodePtr* out,
	const Token* public,
	const Token* external,
	const Type type,
	const Token* identifier)
{
	if (MatchOne(Token_LeftBracket) == NULL)
		return NOT_FOUND_RESULT;

	Array params;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseVarDeclNoSemicolon, Token_RightBracket));

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL && !external) return ERROR_RESULT_LINE("Expected code block after function declaration");
	if (block.ptr != NULL && external) return ERROR_RESULT_LINE("External functions cannot have code blocks");

	Token externalIdentifier = {};
	if (external)
	{
		const Token* token = MatchOne(Token_Identifier);
		if (token != NULL)
			externalIdentifier = *token;
		else
			externalIdentifier = *identifier;

		if (MatchOne(Token_Semicolon) == NULL) return ERROR_RESULT_LINE("Expected \";\"");
	}

	*out = AllocASTNode(
		&(FuncDeclStmt){
			.type = type,
			.oldType = (Type){.expr = NULL_NODE, .modifier = TypeModifier_None},
			.lineNumber = identifier->lineNumber,
			.name = AllocateString(identifier->text),
			.externalName = AllocateString(externalIdentifier.text),
			.parameters = params,
			.oldParameters = AllocateArray(sizeof(NodePtr)),
			.block = block,
			.globalReturn = NULL,
			.public = public != NULL,
			.external = external != NULL,
			.uniqueName = -1,
		},
		sizeof(FuncDeclStmt), Node_FunctionDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseModifierDeclaration(NodePtr* out);

static Result ParseStructDeclaration(NodePtr* out, const Token* public, const Token* external)
{
	if (MatchOne(Token_Struct) == NULL)
		return NOT_FOUND_RESULT;

	if (external)
		return ERROR_RESULT_LINE("\"external\" is not allowed here");

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
		return ERROR_RESULT_LINE("Expected struct name");

	if (MatchOne(Token_LeftCurlyBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"{\" after struct name");

	Array members = AllocateArray(sizeof(NodePtr));
	while (true)
	{
		NodePtr member = NULL_NODE;
		PROPAGATE_ERROR(ParseVarDeclNoSemicolon(&member));
		if (member.ptr == NULL) break;

		assert(member.type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = member.ptr;
		if (varDecl->initializer.ptr != NULL)
			return ERROR_RESULT_LINE("Variable initializers are not allowed here");

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
			.name = AllocateString(identifier->text),
			.members = members,
			.public = public != NULL,
		},
		sizeof(StructDeclStmt), Node_StructDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseImportStatement(NodePtr* out, const Token* public, const Token* external)
{
	const Token* import = MatchOne(Token_Import);
	if (import == NULL)
		return NOT_FOUND_RESULT;

	if (external)
		return ERROR_RESULT_LINE("\"external\" is not allowed here");

	const Token* path = MatchOne(Token_StringLiteral);
	if (path == NULL)
		return ERROR_RESULT_LINE("Expected path after \"import\"");

	*out = AllocASTNode(
		&(ImportStmt){
			.lineNumber = import->lineNumber,
			.path = AllocateString(path->text),
			.moduleName = NULL,
			.public = public != NULL,
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

static Result ParseDeclaration(NodePtr* out, const Token* public, const Token* external)
{
	Type type;
	const Token* identifier;
	PROPAGATE_ERROR(ParseTypeAndIdentifier(&type, &identifier));
	if (type.expr.ptr == NULL)
		return NOT_FOUND_RESULT;

	PROPAGATE_FOUND(ParseFunctionDeclaration(out, public, external, type, identifier));

	return ParseVariableDeclaration(out, public, external, type, identifier, true);
}

static void ParseModifiers(Token** public, Token** external)
{
	*public = MatchOne(Token_Public);
	*external = MatchOne(Token_External);

	if (*public == NULL && *external != NULL)
		*public = MatchOne(Token_Public);
}

static Result ParseModifierDeclaration(NodePtr* out)
{
	Token* public, *external;
	ParseModifiers(&public, &external);

	PROPAGATE_FOUND(ParseStructDeclaration(out, public, external));
	PROPAGATE_FOUND(ParseImportStatement(out, public, external));
	PROPAGATE_FOUND(ParseDeclaration(out, public, external));

	if (public == NULL && external == NULL)
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
	const Token* token = Match((TokenType[]){Token_Break, Token_Continue}, 2);
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

static Result ParseStatement(NodePtr* out)
{
	PROPAGATE_FOUND(ParseIfStatement(out));
	PROPAGATE_FOUND(ParseLoopControlStatement(out));
	PROPAGATE_FOUND(ParseWhileStatement(out));
	PROPAGATE_FOUND(ParseForStatement(out));
	PROPAGATE_FOUND(ParseSectionStatement(out));
	PROPAGATE_FOUND(ParseBlockStatement(out));
	PROPAGATE_FOUND(ParseReturnStatement(out));
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

		if (stmt.type == Node_Import)
		{
			if (!allowImportStatements)
				return ERROR_RESULT_LINE("Import statements must be at the top of the file");
		}
		else
			allowImportStatements = false;

		if (stmt.type != Node_Section &&
			stmt.type != Node_FunctionDeclaration &&
			stmt.type != Node_StructDeclaration &&
			stmt.type != Node_VariableDeclaration &&
			stmt.type != Node_Import)
			return ERROR_RESULT_LINE(
				"Expected section statement, variable declaration, struct declaration, or function declaration");

		ArrayAdd(&stmtArray, &stmt);
	}

	*out = (AST){.nodes = stmtArray};
	return SUCCESS_RESULT;
}

Result Parse(const Array* tokenArray, AST* outSyntaxTree)
{
	pointer = 0;
	tokens = *tokenArray;

	return ParseProgram(outSyntaxTree);
}
