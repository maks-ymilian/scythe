#include "Parser.h"

#include <StringUtils.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_RESULT_LINE(message) ERROR_RESULT(message, CurrentToken(0)->lineNumber, NULL)

static Array tokens;
static size_t pointer;

static bool IsDigitBase(const char c, const int base)
{
	if (base == 10) return isdigit(c);
	if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	if (base == 2) return c == '0' || c == '1';
	if (base == 8) return c >= '0' && c <= '7';
	unreachable();
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


static Token* CurrentToken(const size_t offset)
{
	if (pointer + offset >= tokens.length)
		return tokens.array[tokens.length - 1];

	return tokens.array[pointer + offset];
}

static void Consume() { pointer++; }

static Token* Match(const TokenType* types, const size_t length)
{
	Token* current = CurrentToken(0);

	for (size_t i = 0; i < length; ++i)
	{
		if (current->type != types[i])
			continue;

		Consume();
		return current;
	}

	return NULL;
}

static Token* MatchOne(const TokenType type) { return Match((TokenType[]){type}, 1); }

static Token* Peek(const TokenType* types, const int length, const size_t offset)
{
	Token* current = CurrentToken(offset);
	if (current == NULL)
		return NULL;

	for (int i = 0; i < length; ++i)
	{
		if (current->type != types[i])
			continue;

		return current;
	}

	return NULL;
}

static Token* PeekOne(const TokenType type, const size_t offset) { return Peek((TokenType[]){type}, 1, offset); }

static Result ParseExpression(NodePtr* out);

static Result ParseArrayAccess(NodePtr* out)
{
	const Token* identifier = PeekOne(Token_Identifier, 0);
	if (identifier == NULL || PeekOne(Token_LeftSquareBracket, 1) == NULL)
		return NOT_FOUND_RESULT;

	Consume();

	Consume();

	NodePtr subscript = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&subscript));
	if (subscript.ptr == NULL)
		return ERROR_RESULT_LINE("Expected subscript");


	if (MatchOne(Token_RightSquareBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"]\"");

	*out = AllocASTNode(
		&(ArrayAccessExpr){
			.lineNumber = identifier->lineNumber,
			.subscript = subscript,
			.identifier =
				(IdentifierReference){
					.text = AllocateString(identifier->text),
					.reference = NULL_NODE,
				},
		},
		sizeof(ArrayAccessExpr), Node_ArrayAccess);
	return SUCCESS_RESULT;
}

typedef Result (*ParseFunction)(NodePtr*);

static Result ParseCommaSeparatedList(Array* outArray, const ParseFunction function)
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
				return ERROR_RESULT_LINE("Expected item after \",\"");

			ArrayAdd(outArray, &node);
		}
	}

	return SUCCESS_RESULT;
}

static Result ParseFunctionCall(NodePtr* out)
{
	const Token* identifier = PeekOne(Token_Identifier, 0);
	if (identifier == NULL || PeekOne(Token_LeftBracket, 1) == NULL)
		return NOT_FOUND_RESULT;

	if (MatchOne(Token_Identifier) == NULL)
		unreachable();
	if (MatchOne(Token_LeftBracket) == NULL)
		unreachable();

	Array params;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseExpression));


	if (MatchOne(Token_RightBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

	*out = AllocASTNode(
		&(FuncCallExpr){
			.lineNumber = identifier->lineNumber,
			.arguments = params,
			.identifier =
				(IdentifierReference){
					.text = AllocateString(identifier->text),
					.reference = NULL_NODE,
				},
		},
		sizeof(FuncCallExpr), Node_FunctionCall);
	return SUCCESS_RESULT;
}

static Result LiteralExprFromToken(const Token token, LiteralExpr* out)
{
	switch (token.type)
	{
	case Token_NumberLiteral:
	{
		bool isInteger;
		double floatValue;
		uint64_t intValue;
		PROPAGATE_ERROR(EvaluateNumberLiteral(token.text, token.lineNumber, &isInteger, &floatValue, &intValue));

		if (isInteger)
			*out = (LiteralExpr){
				.lineNumber = token.lineNumber,
				.type = Literal_Int,
				.intValue = intValue};
		else
			*out = (LiteralExpr){
				.lineNumber = token.lineNumber,
				.type = Literal_Float,
				.floatValue = AllocateString(token.text)};
		return SUCCESS_RESULT;
	}
	case Token_StringLiteral:
	{
		*out = (LiteralExpr){
			.lineNumber = token.lineNumber,
			.type = Literal_String,
			.string = AllocateString(token.text),
		};
		return SUCCESS_RESULT;
	}
	case Token_Identifier:
	{
		*out = (LiteralExpr){
			.lineNumber = token.lineNumber,
			.type = Literal_Identifier,
			.identifier =
				(IdentifierReference){
					.text = AllocateString(token.text),
					.reference = NULL_NODE,
				},
		};
		return SUCCESS_RESULT;
	}
	case Token_True:
	case Token_False:
	{
		*out = (LiteralExpr){
			.lineNumber = token.lineNumber,
			.type = Literal_Boolean,
			.boolean = token.type == Token_True,
		};
		return SUCCESS_RESULT;
	}
	default: unreachable();
	}
}

static Result ParsePrimary(NodePtr* out)
{
	const Token* token = Peek(
		(TokenType[]){
			Token_NumberLiteral,
			Token_StringLiteral,
			Token_Identifier,
			Token_LeftBracket,
			Token_True,
			Token_False,
		},
		6, 0);
	if (token == NULL)
		return (Result){
			.type = Result_NotFound,
			.errorMessage = AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken(0)->type)),
			.lineNumber = CurrentToken(0)->lineNumber,
			.filePath = NULL,
		};

	switch (token->type)
	{
	case Token_LeftBracket:
	{
		Consume();
		*out = NULL_NODE;
		PROPAGATE_ERROR(ParseExpression(out));
		if (out->ptr == NULL)
			return ERROR_RESULT_LINE("Expected expression");

		const Token* closingBracket = MatchOne(Token_RightBracket);
		if (closingBracket == NULL)
			return ERROR_RESULT_LINE("Expected \")\"");

		return SUCCESS_RESULT;
	}
	case Token_Identifier:
	{
		MemberAccessExpr* start = NULL;
		MemberAccessExpr* current = NULL;
		while (true)
		{
			NodePtr value = NULL_NODE;
			PROPAGATE_ERROR(ParseFunctionCall(&value));
			if (value.ptr == NULL)
				PROPAGATE_ERROR(ParseArrayAccess(&value));
			if (value.ptr == NULL)
			{
				const Token* token = MatchOne(Token_Identifier);
				if (token == NULL)
					return ERROR_RESULT_LINE("Expected identifier");

				LiteralExpr literal;
				PROPAGATE_ERROR(LiteralExprFromToken(*token, &literal));
				value = AllocASTNode(&literal, sizeof(LiteralExpr), Node_Literal);
			}

			assert(value.ptr != NULL);
			const NodePtr memberAccess = AllocASTNode(
				&(MemberAccessExpr){
					.value = value,
					.next = NULL_NODE,
				},
				sizeof(MemberAccessExpr), Node_MemberAccess);
			if (current != NULL) current->next = memberAccess;
			current = memberAccess.ptr;

			if (start == NULL)
				start = memberAccess.ptr;


			if (MatchOne(Token_Dot) == NULL)
				break;
		}

		*out = (NodePtr){.ptr = start, .type = Node_MemberAccess};
		return SUCCESS_RESULT;
	}
	case Token_NumberLiteral:
	case Token_StringLiteral:
	case Token_True:
	case Token_False:
	{
		Consume();
		LiteralExpr literal;
		PROPAGATE_ERROR(LiteralExprFromToken(*token, &literal));
		*out = AllocASTNode(&literal, sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}
	default: unreachable();
	}
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
	PROPAGATE_ERROR(parseFunc(&left));
	if (left.ptr == NULL)
		return NOT_FOUND_RESULT;

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
	PROPAGATE_ERROR(parseFunc(&left));
	if (left.ptr == NULL)
		return NOT_FOUND_RESULT;

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
	NodePtr expr = NULL_NODE;
	const Result result = ParseExpression(&expr);
	if (result.type != Result_Success)
		return result;
	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(&(ExpressionStmt){.expr = expr}, sizeof(ExpressionStmt), Node_ExpressionStatement);
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

static Result ParseStatement(NodePtr* out);

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


	NodePtr elseStmt = NULL_NODE;
	if (MatchOne(Token_Else))
	{
		PROPAGATE_ERROR(ParseStatement(&elseStmt));
		if (elseStmt.ptr == NULL)
			return ERROR_RESULT_LINE("Expected statement after \"else\"");
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
	Result exitResult = NOT_FOUND_RESULT;
	while (true)
	{
		NodePtr stmt = NULL_NODE;
		const Result result = ParseStatement(&stmt);
		PROPAGATE_ERROR(result);
		if (stmt.ptr == NULL)
		{
			assert(result.type == Result_NotFound);
			if (result.errorMessage != NULL)
				exitResult = result;
			break;
		}


		if (stmt.type == Node_Section)
			return ERROR_RESULT_LINE("Nested sections are not allowed");


		if (stmt.type == Node_StructDeclaration)
			return ERROR_RESULT_LINE("Struct declarations not allowed inside code blocks");

		ArrayAdd(&statements, &stmt);
	}

	if (MatchOne(Token_RightCurlyBracket) == NULL)
	{
		if (exitResult.errorMessage != NULL)
			return exitResult;
		return ERROR_RESULT_LINE(
			AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken(0)->type)));
	}

	*out = AllocASTNode(
		&(BlockStmt){
			.lineNumber = openingBrace->lineNumber,
			.statements = statements,
		},
		sizeof(BlockStmt), Node_Block);
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
	assert(block.type == Node_Block);

	*out = AllocASTNode(
		&(SectionStmt){
			.lineNumber = identifier->lineNumber,
			.sectionType = sectionType,
			.block = block},
		sizeof(SectionStmt), Node_Section);
	return SUCCESS_RESULT;
}

static bool ParsePrimitiveType(PrimitiveType* out, int* outLineNumber)
{
	const Token* primitiveType = Match(
		(TokenType[]){
			Token_Void,
			Token_Int,
			Token_Float,
			Token_String,
			Token_Bool,
		},
		5);

	if (primitiveType == NULL)
		return false;

	*outLineNumber = primitiveType->lineNumber;
	*out = tokenTypeToPrimitiveType[primitiveType->type];
	return true;
}

static Result ParseType(NodePtr* out)
{
	const size_t oldPointer = pointer;

	PrimitiveType primitiveType;
	int lineNumber;
	if (ParsePrimitiveType(&primitiveType, &lineNumber))
	{
		*out = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = lineNumber,
				.type = Literal_PrimitiveType,
				.primitiveType = primitiveType,
			},
			sizeof(LiteralExpr), Node_Literal);
		return SUCCESS_RESULT;
	}

	PROPAGATE_ERROR(ParsePrimary(out));
	if (out->type == Node_MemberAccess)
		return SUCCESS_RESULT;

	pointer = oldPointer;
	*out = NULL_NODE;
	return NOT_FOUND_RESULT;
}

static void ParseModifiers(Token** public, Token** external)
{
	*public = MatchOne(Token_Public);
	*external = MatchOne(Token_External);
}

static Result ParseVariableDeclaration(NodePtr* out, const bool expectSemicolon)
{
	const size_t oldPointer = pointer;

	Token* public, *external;
	ParseModifiers(&public, &external);

	NodePtr type = NULL_NODE;
	PROPAGATE_ERROR(ParseType(&type));
	if (type.ptr == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

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
			return ERROR_RESULT_LINE("Expected \";\"");
	}

	*out = AllocASTNode(
		&(VarDeclStmt){
			.type = type,
			.lineNumber = identifier->lineNumber,
			.name = AllocateString(identifier->text),
			.externalName = AllocateString(externalIdentifier.text),
			.initializer = initializer,
			.arrayLength = NULL_NODE,
			.array = false,
			.public = public != NULL,
			.external = external != NULL,
			.uniqueName = -1,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseVarDeclNoSemicolon(NodePtr* out)
{
	return ParseVariableDeclaration(out, false);
}

static Result ParseFunctionDeclaration(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Token* public, *external;
	ParseModifiers(&public, &external);

	NodePtr type = NULL_NODE;
	PROPAGATE_ERROR(ParseType(&type));
	if (type.ptr == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	if (MatchOne(Token_LeftBracket) == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	Array params;
	PROPAGATE_ERROR(ParseCommaSeparatedList(&params, ParseVarDeclNoSemicolon));

	if (MatchOne(Token_RightBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \")\"");

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
			.lineNumber = identifier->lineNumber,
			.name = AllocateString(identifier->text),
			.externalName = AllocateString(externalIdentifier.text),
			.parameters = params,
			.block = block,
			.public = public != NULL,
			.external = external != NULL,
			.uniqueName = -1,
		},
		sizeof(FuncDeclStmt), Node_FunctionDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseStructDeclaration(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Token* public, *external;
	ParseModifiers(&public, &external);

	if (MatchOne(Token_Struct) == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

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
		PROPAGATE_ERROR(ParseVariableDeclaration(&member, true));
		if (member.ptr == NULL) break;
		ArrayAdd(&members, &member);
	}


	if (MatchOne(Token_RightCurlyBracket) == NULL)
		return ERROR_RESULT_LINE(
			AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken(0)->type)));

	*out = AllocASTNode(
		&(StructDeclStmt){
			.lineNumber = identifier->lineNumber,
			.name = AllocateString(identifier->text),
			.members = members,
			.public = public != NULL,
			.uniqueName = -1,
		},
		sizeof(StructDeclStmt), Node_StructDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseArrayDeclaration(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Token* public, *external;
	ParseModifiers(&public, &external);

	NodePtr type = NULL_NODE;
	PROPAGATE_ERROR(ParseType(&type));
	if (type.ptr == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	const Token* identifier = MatchOne(Token_Identifier);
	if (identifier == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	if (MatchOne(Token_LeftSquareBracket) == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

	if (external)
		return ERROR_RESULT_LINE("\"external\" is not allowed here");

	NodePtr length = NULL_NODE;
	PROPAGATE_ERROR(ParseExpression(&length));
	if (length.ptr == NULL)
		return ERROR_RESULT_LINE("Expected array length");

	if (MatchOne(Token_RightSquareBracket) == NULL)
		return ERROR_RESULT_LINE("Expected \"]\"");

	if (MatchOne(Token_Semicolon) == NULL)
		return ERROR_RESULT_LINE("Expected \";\"");

	*out = AllocASTNode(
		&(VarDeclStmt){
			.type = type,
			.lineNumber = identifier->lineNumber,
			.name = AllocateString(identifier->text),
			.externalName = NULL,
			.arrayLength = length,
			.initializer = NULL_NODE,
			.array = true,
			.public = public != NULL,
			.external = false,
			.uniqueName = -1,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
	return SUCCESS_RESULT;
}

static Result ParseImportStatement(NodePtr* out)
{
	const size_t oldPointer = pointer;

	Token* public, *external;
	ParseModifiers(&public, &external);

	const Token* import = MatchOne(Token_Import);
	if (import == NULL)
	{
		pointer = oldPointer;
		return NOT_FOUND_RESULT;
	}

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


	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL)
	{
		NodePtr stmt = NULL_NODE;
		PROPAGATE_ERROR(ParseStatement(&stmt));
		if (stmt.ptr == NULL)
			return ERROR_RESULT_LINE("Expected statement after while statement");

		Array statements = AllocateArray(sizeof(NodePtr));
		ArrayAdd(&statements, &stmt);
		block = AllocASTNode(
			&(BlockStmt){
				.lineNumber = whileToken->lineNumber,
				.statements = statements,
			},
			sizeof(BlockStmt), Node_Block);
	}

	*out = AllocASTNode(
		&(WhileStmt){
			.lineNumber = whileToken->lineNumber,
			.expr = expr,
			.stmt = block,
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

	NodePtr block = NULL_NODE;
	PROPAGATE_ERROR(ParseBlockStatement(&block));
	if (block.ptr == NULL)
	{
		NodePtr stmt = NULL_NODE;
		PROPAGATE_ERROR(ParseStatement(&stmt));
		if (stmt.ptr == NULL)
			return ERROR_RESULT_LINE("Expected statement after for statement");

		Array statements = AllocateArray(sizeof(NodePtr));
		ArrayAdd(&statements, &stmt);
		block = AllocASTNode(
			&(BlockStmt){
				.lineNumber = forToken->lineNumber,
				.statements = statements,
			},
			sizeof(BlockStmt), Node_Block);
	}

	*out = AllocASTNode(
		&(ForStmt){
			.lineNumber = forToken->lineNumber,
			.initialization = initializer,
			.condition = condition,
			.increment = increment,
			.stmt = block,
		},
		sizeof(ForStmt), Node_For);
	return SUCCESS_RESULT;
}

static Result ParseLoopControlStatement(NodePtr* out)
{
	const Token* token = MatchOne(Token_Break);
	if (token == NULL) token = MatchOne(Token_Continue);
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
	PROPAGATE_FOUND(ParseImportStatement(out));
	PROPAGATE_FOUND(ParseFunctionDeclaration(out));
	PROPAGATE_FOUND(ParseStructDeclaration(out));
	PROPAGATE_FOUND(ParseIfStatement(out));
	PROPAGATE_FOUND(ParseLoopControlStatement(out));
	PROPAGATE_FOUND(ParseWhileStatement(out));
	PROPAGATE_FOUND(ParseForStatement(out));
	PROPAGATE_FOUND(ParseArrayDeclaration(out));
	PROPAGATE_FOUND(ParseVariableDeclaration(out, true));
	PROPAGATE_FOUND(ParseSectionStatement(out));
	PROPAGATE_FOUND(ParseBlockStatement(out));
	PROPAGATE_FOUND(ParseReturnStatement(out));
	PROPAGATE_FOUND(ParseExpressionStatement(out));

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
				AllocateString1Str("Unexpected token \"%s\"", GetTokenTypeString(CurrentToken(0)->type)));

		if (stmt.type == Node_Import)
		{
			if (!allowImportStatements)
				return ERROR_RESULT_LINE("Import statements must be at the top of the file");
		}
		else
		{
			allowImportStatements = false;
		}

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
