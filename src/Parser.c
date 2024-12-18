#include "Parser.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <StringUtils.h>

#define SET_LINE_NUMBER lineNumber = ((Token*)tokens.array[pointer])->lineNumber;

#define NOT_FOUND_RESULT (Result){false, false, NULL, 0}
#define ERROR_RESULT_LINE(message) ERROR_RESULT(message, lineNumber);
#define ERROR_RESULT_LINE_TOKEN(message, tokenType) ERROR_RESULT_TOKEN(message, lineNumber, tokenType)
#define NOT_FOUND_RESULT_LINE_TOKEN(message, tokenType) (Result){false, false, message, lineNumber, tokenType};

static Array tokens;
static long pointer;

static bool IsDigitBase(const char c, const int base)
{
    if (base == 10) return isdigit(c);
    if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (base == 2) return c == '0' || c == '1';
    if (base == 8) return c >= '0' && c <= '7';
    assert(0);
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
            return ERROR_RESULT("Invalid integer literal", lineNumber);

    *out = strtoull(string, NULL, base);

    if (*out == UINT64_MAX)
        return ERROR_RESULT("Invalid integer literal", lineNumber);

    if (*out == 0)
    {
        bool isZero = true;
        for (int i = 0; string[i] != '\0'; ++i)
            if (string[i] != '0') isZero = false;

        if (!isZero)
            return ERROR_RESULT("Invalid integer literal", lineNumber);
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
            return ERROR_RESULT("Invalid float literal", lineNumber);
        if (fpclassify(*outFloat) != FP_NORMAL && fpclassify(*outFloat) != FP_ZERO)
            return ERROR_RESULT("Invalid float literal", lineNumber);
        if (text[consumedChars] != '\0')
            return ERROR_RESULT("Invalid float literal", lineNumber);

        return SUCCESS_RESULT;
    }

    size_t index = 0;
    int base = 10;
    if (text[index] == '0')
    {
        if (tolower(text[index + 1]) == 'x') base = 16;
        else if (tolower(text[index + 1]) == 'o') base = 8;
        else if (tolower(text[index + 1]) == 'b') base = 2;

        if (base != 10) index += 2;
    }

    HANDLE_ERROR(StringToUInt64(text + index, base, lineNumber, outInt));
    return SUCCESS_RESULT;
}


static Token* CurrentToken(const int offset)
{
    if (pointer + offset >= tokens.length)
        return NULL;

    return tokens.array[pointer + offset];
}

static void Consume() { pointer++; }

static Token* Match(const TokenType* types, const int length)
{
    Token* current = CurrentToken(0);
    if (current == NULL)
        return NULL;

    for (int i = 0; i < length; ++i)
    {
        if (current->type != types[i])
            continue;

        Consume();
        return current;
    }

    return NULL;
}

static Token* MatchOne(const TokenType type) { return Match((TokenType[]){type}, 1); }

static Token* Peek(const TokenType* types, const int length, const int offset)
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

static Token* PeekOne(const TokenType type, const int offset) { return Peek((TokenType[]){type}, 1, offset); }

static Result ParseExpression(NodePtr* out);

static Result ParseArrayAccess(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* identifier = PeekOne(Token_Identifier, 0);
    if (identifier == NULL || PeekOne(Token_LeftSquareBracket, 1) == NULL)
        return NOT_FOUND_RESULT;

    Consume();
    SET_LINE_NUMBER
    Consume();

    NodePtr subscript = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&subscript));
    if (subscript.ptr == NULL)
        return ERROR_RESULT_LINE("Expected subscript");
    SET_LINE_NUMBER

    if (MatchOne(Token_RightSquareBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightSquareBracket);

    *out = AllocASTNode(
        &(ArrayAccessExpr)
        {
            .lineNumber = identifier->lineNumber,
            .subscript = subscript,
            .identifier =
            (IdentifierReference)
            {
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
    long SET_LINE_NUMBER;

    *outArray = AllocateArray(sizeof(NodePtr));

    NodePtr firstNode = NULL_NODE;
    HANDLE_ERROR(function(&firstNode));
    if (firstNode.ptr != NULL)
    {
        ArrayAdd(outArray, &firstNode);
        while (true)
        {
            SET_LINE_NUMBER;
            if (MatchOne(Token_Comma) == NULL)
                break;

            NodePtr node = NULL_NODE;
            HANDLE_ERROR(function(&node));
            if (node.ptr == NULL)
                return ERROR_RESULT_LINE_TOKEN("Expected item after \"#t\"", Token_Comma);

            ArrayAdd(outArray, &node);
        }
    }

    return SUCCESS_RESULT;
}

static Result ParseFunctionCall(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* identifier = PeekOne(Token_Identifier, 0);
    if (identifier == NULL || PeekOne(Token_LeftBracket, 1) == NULL)
        return NOT_FOUND_RESULT;

    if (MatchOne(Token_Identifier) == NULL)
        assert(0);
    if (MatchOne(Token_LeftBracket) == NULL)
        assert(0);

    Array params;
    HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseExpression));

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    *out = AllocASTNode(
        &(FuncCallExpr)
        {
            .lineNumber = identifier->lineNumber,
            .parameters = params,
            .identifier =
            (IdentifierReference)
            {
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
        HANDLE_ERROR(EvaluateNumberLiteral(token.text, token.lineNumber, &isInteger, &floatValue, &intValue));

        if (isInteger)
            *out = (LiteralExpr)
            {
                .lineNumber = token.lineNumber,
                .type = Literal_Int,
                .intValue = intValue
            };
        else
            *out = (LiteralExpr)
            {
                .lineNumber = token.lineNumber,
                .type = Literal_Float,
                .floatValue = floatValue
            };
        return SUCCESS_RESULT;
    }
    case Token_StringLiteral:
    {
        *out = (LiteralExpr)
        {
            .lineNumber = token.lineNumber,
            .type = Literal_String,
            .string = AllocateString(token.text),
        };
        return SUCCESS_RESULT;
    }
    case Token_Identifier:
    {
        *out = (LiteralExpr)
        {
            .lineNumber = token.lineNumber,
            .type = Literal_Identifier,
            .identifier =
            (IdentifierReference)
            {
                .text = AllocateString(token.text),
                .reference = NULL_NODE,
            },
        };
        return SUCCESS_RESULT;
    }
    case Token_True:
    case Token_False:
    {
        *out = (LiteralExpr)
        {
            .lineNumber = token.lineNumber,
            .type = Literal_Boolean,
            .boolean = token.type == Token_True,
        };
        return SUCCESS_RESULT;
    }
    default: assert(0);
    }
}

static Result ParsePrimary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* token = Peek(
        (TokenType[])
        {
            Token_NumberLiteral,
            Token_StringLiteral,
            Token_Identifier,
            Token_LeftBracket,
            Token_True,
            Token_False,
        }, 6, 0);
    if (token == NULL)
        return NOT_FOUND_RESULT_LINE_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->type);

    switch (token->type)
    {
    case Token_LeftBracket:
    {
        Consume();
        *out = NULL_NODE;
        HANDLE_ERROR(ParseExpression(out));
        if (out->ptr == NULL)
            return ERROR_RESULT_LINE("Expected expression");

        const Token* closingBracket = MatchOne(Token_RightBracket);
        if (closingBracket == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

        return SUCCESS_RESULT;
    }
    case Token_Identifier:
    {
        MemberAccessExpr* start = NULL;
        MemberAccessExpr* current = NULL;
        while (true)
        {
            NodePtr value = NULL_NODE;
            HANDLE_ERROR(ParseFunctionCall(&value));
            if (value.ptr == NULL)
                HANDLE_ERROR(ParseArrayAccess(&value));
            if (value.ptr == NULL)
            {
                const Token* token = MatchOne(Token_Identifier);
                if (token == NULL)
                    return ERROR_RESULT_LINE("Expected identifier");

                LiteralExpr literal;
                HANDLE_ERROR(LiteralExprFromToken(*token, &literal));
                value = AllocASTNode(&literal, sizeof(LiteralExpr), Node_Literal);
            }

            assert(value.ptr != NULL);
            const NodePtr memberAccess = AllocASTNode(
                &(MemberAccessExpr)
                {
                    .value = value,
                    .next = NULL_NODE,
                },
                sizeof(MemberAccessExpr), Node_MemberAccess);
            if (current != NULL) current->next = memberAccess;
            current = memberAccess.ptr;

            if (start == NULL)
                start = memberAccess.ptr;

            SET_LINE_NUMBER
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
        HANDLE_ERROR(LiteralExprFromToken(*token, &literal));
        *out = AllocASTNode(&literal, sizeof(LiteralExpr), Node_Literal);
        return SUCCESS_RESULT;
    }
    default: assert(0);
    }
}

static UnaryOperator TokenTypeToUnaryOperator(const TokenType tokenType)
{
    switch (tokenType)
    {
    case Token_Plus: return Unary_Plus;
    case Token_Minus: return Unary_Minus;
    case Token_Exclamation: return Unary_Negate;
    case Token_PlusPlus: return Unary_Increment;
    case Token_MinusMinus: return Unary_Decrement;
    default: assert(0);
    }
}

static Result ParseUnary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* operator = Match((TokenType[]){Token_Plus, Token_Minus, Token_Exclamation, Token_PlusPlus, Token_MinusMinus}, 5);
    if (operator == NULL)
        return ParsePrimary(out);

    SET_LINE_NUMBER
    NodePtr expr;
    if (!ParseUnary(&expr).success)
        return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);

    *out = AllocASTNode(
        &(UnaryExpr)
        {
            .lineNumber = operator->lineNumber,
            .expression = expr,
            .operatorType = TokenTypeToUnaryOperator(operator->type),
        },
        sizeof(UnaryExpr), Node_Unary);
    return SUCCESS_RESULT;
}

static BinaryOperator TokenTypeToBinaryOperator(const TokenType tokenType)
{
    switch (tokenType)
    {
    case Token_AmpersandAmpersand: return Binary_BoolAnd;
    case Token_PipePipe: return Binary_BoolOr;
    case Token_EqualsEquals: return Binary_IsEqual;
    case Token_ExclamationEquals: return Binary_NotEqual;
    case Token_RightAngleBracket: return Binary_GreaterThan;
    case Token_RightAngleEquals: return Binary_GreaterOrEqual;
    case Token_LeftAngleBracket: return Binary_LessThan;
    case Token_LeftAngleEquals: return Binary_LessOrEqual;

    case Token_Plus: return Binary_Add;
    case Token_Minus: return Binary_Subtract;
    case Token_Asterisk: return Binary_Multiply;
    case Token_Slash: return Binary_Divide;

    case Token_Equals: return Binary_Assignment;
    case Token_PlusEquals: return Binary_AddAssign;
    case Token_MinusEquals: return Binary_SubtractAssign;
    case Token_AsteriskEquals: return Binary_MultiplyAssign;
    case Token_SlashEquals: return Binary_DivideAssign;
    default: assert(0);
    }
}

typedef Result (*ParseFunc)(NodePtr* out);

static Result ParseLeftBinary(NodePtr* out, const ParseFunc parseFunc, const TokenType operators[], const size_t operatorsLength)
{
    long SET_LINE_NUMBER

    NodePtr left = NULL_NODE;
    HANDLE_ERROR(parseFunc(&left));
    if (left.ptr == NULL)
        return NOT_FOUND_RESULT;

    const Token* operator = Match(operators, operatorsLength);
    while (operator != NULL)
    {
        SET_LINE_NUMBER
        NodePtr right = NULL_NODE;
        HANDLE_ERROR(parseFunc(&right));
        if (right.ptr == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);

        left = AllocASTNode(
            &(BinaryExpr)
            {
                .lineNumber = operator->lineNumber,
                .operatorType = TokenTypeToBinaryOperator(operator->type),
                .left = left,
                .right = right,
            },
            sizeof(BinaryExpr), Node_Binary);

        operator = Match(operators, operatorsLength);
    }

    *out = left;
    return SUCCESS_RESULT;
}

static Result ParseFactor(NodePtr* out)
{
    return ParseLeftBinary(out, ParseUnary, (TokenType[]){Token_Asterisk, Token_Slash}, 2);
}

static Result ParseTerm(NodePtr* out)
{
    return ParseLeftBinary(out, ParseFactor, (TokenType[]){Token_Plus, Token_Minus}, 2);
}

static Result ParseNumberComparison(NodePtr* out)
{
    return ParseLeftBinary(
        out,
        ParseTerm,
        (TokenType[])
        {
            Token_RightAngleBracket,
            Token_RightAngleEquals,
            Token_LeftAngleBracket,
            Token_LeftAngleEquals
        },
        4);
}

static Result ParseEquality(NodePtr* out)
{
    return ParseLeftBinary(out, ParseNumberComparison, (TokenType[]){Token_EqualsEquals, Token_ExclamationEquals}, 2);
}

static Result ParseBooleanOperators(NodePtr* out)
{
    return ParseLeftBinary(out, ParseEquality, (TokenType[]){Token_AmpersandAmpersand, Token_PipePipe}, 2);
}

static Result ParseAssignment(NodePtr* out)
{
    long SET_LINE_NUMBER

    NodePtr left = NULL_NODE;
    HANDLE_ERROR(ParseBooleanOperators(&left));
    if (left.ptr == NULL)
        return NOT_FOUND_RESULT;

    Array exprArray = AllocateArray(sizeof(NodePtr));
    ArrayAdd(&exprArray, &left);

    const TokenType validOperators[5] = {Token_Equals, Token_PlusEquals, Token_MinusEquals, Token_AsteriskEquals, Token_SlashEquals};
    const Token* operator = Match(validOperators, 5);
    Array operatorArray = AllocateArray(sizeof(Token));
    while (operator != NULL)
    {
        ArrayAdd(&operatorArray, operator);

        SET_LINE_NUMBER
        NodePtr right = NULL_NODE;
        HANDLE_ERROR(ParseEquality(&right));
        if (right.ptr == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);

        ArrayAdd(&exprArray, &right);

        operator = Match(validOperators, 5);
    }

    NodePtr* expr1 = exprArray.array[exprArray.length - 1];

    for (int i = exprArray.length - 2; i >= 0; --i)
    {
        operator = operatorArray.array[i];
        const NodePtr* expr2 = exprArray.array[i];

        *expr1 = AllocASTNode(
            &(BinaryExpr)
            {
                .lineNumber = operator->lineNumber,
                .operatorType = TokenTypeToBinaryOperator(operator->type),
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

static Result ParseExpression(NodePtr* out)
{
    long SET_LINE_NUMBER

    return ParseAssignment(out);
}

static Result ParseExpressionStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    NodePtr expr = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&expr));

    if (expr.type == Node_Null)
        return NOT_FOUND_RESULT;
    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon)

    *out = AllocASTNode(&(ExpressionStmt){.expr = expr}, sizeof(ExpressionStmt), Node_ExpressionStatement);
    return SUCCESS_RESULT;
}

static Result ParseReturnStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* returnToken = MatchOne(Token_Return);
    if (returnToken == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER
    NodePtr expr = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&expr));

    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon)

    *out = AllocASTNode(
        &(ReturnStmt)
        {
            .lineNumber = returnToken->lineNumber,
            .expr = expr,
        },
        sizeof(ReturnStmt), Node_Return);
    return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out);

static Result ParseIfStatement(NodePtr* out)
{
    long SET_LINE_NUMBER;

    const Token* ifToken = MatchOne(Token_If);
    if (ifToken == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER;
    if (MatchOne(Token_LeftBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_LeftBracket);

    SET_LINE_NUMBER;
    NodePtr expr = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&expr));
    if (expr.ptr == NULL)
        return ERROR_RESULT_LINE("Expected expression in if statement");

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    SET_LINE_NUMBER;
    NodePtr stmt = NULL_NODE;
    HANDLE_ERROR(ParseStatement(&stmt));
    if (stmt.ptr == NULL)
        return ERROR_RESULT_LINE("Expected statement");

    SET_LINE_NUMBER;
    NodePtr elseStmt = NULL_NODE;
    if (MatchOne(Token_Else))
    {
        HANDLE_ERROR(ParseStatement(&elseStmt));
        if (elseStmt.ptr == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected statement after \"#t\"", Token_Else);
    }

    *out = AllocASTNode(
        &(IfStmt)
        {
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
    long SET_LINE_NUMBER

    const Token* openingBrace = MatchOne(Token_LeftCurlyBracket);
    if (openingBrace == NULL)
        return NOT_FOUND_RESULT;

    Array statements = AllocateArray(sizeof(NodePtr));
    Result exitResult = {0, 0, NULL};
    while (true)
    {
        SET_LINE_NUMBER
        NodePtr stmt = NULL_NODE;
        const Result result = ParseStatement(&stmt);
        HANDLE_ERROR(result);
        if (stmt.ptr == NULL)
        {
            if (result.errorMessage != NULL)
                exitResult = result;
            break;
        }

        SET_LINE_NUMBER
        if (stmt.type == Node_Section)
            return ERROR_RESULT_LINE("Nested sections are not allowed");

        SET_LINE_NUMBER
        if (stmt.type == Node_StructDeclaration)
            return ERROR_RESULT_LINE("Struct declarations not allowed inside code blocks");

        ArrayAdd(&statements, &stmt);
    }

    if (MatchOne(Token_RightCurlyBracket) == NULL)
    {
        if (exitResult.errorMessage != NULL)
            return ERROR_RESULT_LINE_TOKEN(exitResult.errorMessage, exitResult.tokenType);
        return ERROR_RESULT_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->lineNumber, CurrentToken(0)->type);
    }

    *out = AllocASTNode(
        &(BlockStmt)
        {
            .statements = statements,
        },
        sizeof(BlockStmt), Node_Block);
    return SUCCESS_RESULT;
}

static Token* PeekType(const int offset)
{
    Token* type = PeekOne(Token_Identifier, offset);
    if (type == NULL)
    {
        type = Peek((TokenType[]){Token_Void, Token_Int, Token_Float, Token_String, Token_Bool}, 5, offset);
        if (type == NULL)
            return NULL;
    }
    return type;
}

static Token* MatchType()
{
    Token* type = PeekType(0);
    if (type != NULL)
        Consume();
    return type;
}

static Result ParseSectionStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    if (MatchOne(Token_At) == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER;

    const Token* identifier = MatchOne(Token_Identifier);
    if (identifier == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected identifier after \"#t\"", Token_At);

    const char sectionNames[6][10] =
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

    SectionType sectionType = -1;
    bool sectionFound = false;
    for (int i = 0; i < sizeof(sectionTypes) / sizeof(TokenType); ++i)
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
    HANDLE_ERROR(ParseBlockStatement(&block));
    if (block.ptr == NULL)
        return ERROR_RESULT_LINE("Expected block after section statement");
    assert(block.type == Node_Block);

    *out = AllocASTNode(
        &(SectionStmt)
        {
            .lineNumber = identifier->lineNumber,
            .sectionType = sectionType,
            .block = block
        },
        sizeof(SectionStmt), Node_Section);
    return SUCCESS_RESULT;
}

static PrimitiveType TokenTypeToPrimitiveType(const TokenType tokenType)
{
    switch (tokenType)
    {
    case Token_Void: return Primitive_Void;
    case Token_Int: return Primitive_Int;
    case Token_Float: return Primitive_Float;
    case Token_String: return Primitive_String;
    case Token_Bool: return Primitive_Bool;
    default: assert(0);
    }
}

static TypeReference TokenToTypeReference(const Token token)
{
    switch (token.type)
    {
    case Token_Void:
    case Token_Int:
    case Token_Float:
    case Token_String:
    case Token_Bool:
        return (TypeReference)
        {
            .text = NULL,
            .primitive = true,
            .value.primitiveType = TokenTypeToPrimitiveType(token.type),
        };
    case Token_Identifier:
        return (TypeReference)
        {
            .text = AllocateString(token.text),
            .primitive = false,
            .value.reference = NULL_NODE,
        };
    default: assert(0);
    }
}

static Result ParseVariableDeclaration(NodePtr* out, const bool expectSemicolon)
{
    long SET_LINE_NUMBER

    int modifierCount = 0;

    const bool publicFound = PeekOne(Token_Public, 0 + modifierCount) != NULL;
    if (publicFound) modifierCount++;
    const bool externalFound = PeekOne(Token_External, 0 + modifierCount) != NULL;
    if (externalFound) modifierCount++;

    const Token* type = PeekType(0 + modifierCount);
    const Token* identifier = PeekOne(Token_Identifier, 1 + modifierCount);

    if (!(identifier && type))
        return NOT_FOUND_RESULT;

    for (int i = 0; i < modifierCount; ++i) Consume();
    Consume();
    Consume();

    NodePtr initializer = NULL_NODE;
    const Token* equals = MatchOne(Token_Equals);
    if (equals != NULL)
    {
        if (externalFound)
            return ERROR_RESULT_LINE("External variable declarations cannot have an initializer");

        HANDLE_ERROR(ParseExpression(&initializer));
        if (initializer.ptr == NULL)
            return ERROR_RESULT_LINE("Expected expression");
    }

    Token externalIdentifier = {};
    if (externalFound)
    {
        const Token* token = MatchOne(Token_Identifier);
        if (token != NULL) externalIdentifier = *token;
        else externalIdentifier = *identifier;
    }

    if (expectSemicolon)
    {
        const Token* semicolon = MatchOne(Token_Semicolon);
        if (semicolon == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
    }

    *out = AllocASTNode(
        &(VarDeclStmt)
        {
            .type = TokenToTypeReference(*type),
            .lineNumber = type->lineNumber,
            .name = AllocateString(identifier->text),
            .externalName = AllocateString(externalIdentifier.text),
            .initializer = initializer,
            .arrayLength = NULL_NODE,
            .array = false,
            .public = publicFound,
            .external = externalFound,
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
    long SET_LINE_NUMBER

    int modifierCount = 0;

    const bool publicFound = PeekOne(Token_Public, 0 + modifierCount) != NULL;
    if (publicFound) modifierCount++;
    const bool externalFound = PeekOne(Token_External, 0 + modifierCount) != NULL;
    if (externalFound) modifierCount++;

    const Token* type = PeekType(0 + modifierCount);
    const Token* identifier = PeekOne(Token_Identifier, 1 + modifierCount);

    if (!(identifier && type && PeekOne(Token_LeftBracket, 2 + modifierCount)))
        return NOT_FOUND_RESULT;

    for (int i = 0; i < modifierCount; ++i) Consume();
    Consume();
    Consume();
    Consume();

    Array params;
    HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseVarDeclNoSemicolon));

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    NodePtr block = NULL_NODE;
    HANDLE_ERROR(ParseBlockStatement(&block));
    if (block.ptr == NULL && !externalFound) return ERROR_RESULT_LINE("Expected code block after function declaration");
    if (block.ptr != NULL && externalFound) return ERROR_RESULT_LINE("External functions cannot have code blocks");

    Token externalIdentifier = {};
    if (externalFound)
    {
        const Token* token = MatchOne(Token_Identifier);
        if (token != NULL) externalIdentifier = *token;
        else externalIdentifier = *identifier;

        if (MatchOne(Token_Semicolon) == NULL) return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
    }

    *out = AllocASTNode(
        &(FuncDeclStmt)
        {
            .type = TokenToTypeReference(*type),
            .lineNumber = type->lineNumber,
            .name = AllocateString(identifier->text),
            .externalName = AllocateString(externalIdentifier.text),
            .parameters = params,
            .block = block,
            .public = publicFound,
            .external = externalFound,
        },
        sizeof(FuncDeclStmt), Node_FunctionDeclaration);
    return SUCCESS_RESULT;
}

static Result ParseStructDeclaration(NodePtr* out)
{
    long SET_LINE_NUMBER

    const bool publicFound = PeekOne(Token_Public, 0) != NULL;
    if (PeekOne(Token_Struct, publicFound) == NULL)
        return NOT_FOUND_RESULT;

    if (publicFound) Consume();
    Consume();

    SET_LINE_NUMBER
    const Token* identifier = MatchOne(Token_Identifier);
    if (identifier == NULL)
        return ERROR_RESULT_LINE("Expected struct name");

    SET_LINE_NUMBER
    if (MatchOne(Token_LeftCurlyBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\" after struct name", Token_LeftCurlyBracket);

    Array members = AllocateArray(sizeof(NodePtr));
    while (true)
    {
        NodePtr member = NULL_NODE;
        HANDLE_ERROR(ParseVariableDeclaration(&member, true));
        if (member.ptr == NULL) break;
        ArrayAdd(&members, &member);
    }

    SET_LINE_NUMBER
    if (MatchOne(Token_RightCurlyBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->type);

    *out = AllocASTNode(
        &(StructDeclStmt)
        {
            .lineNumber = identifier->lineNumber,
            .name = AllocateString(identifier->text),
            .members = members,
            .public = publicFound,
        },
        sizeof(StructDeclStmt), Node_StructDeclaration);
    return SUCCESS_RESULT;
}

static Result ParseArrayDeclaration(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* type = PeekType(0);
    if (type == NULL) return NOT_FOUND_RESULT;

    const Token* identifier = PeekOne(Token_Identifier, 1);
    if (identifier == NULL) return NOT_FOUND_RESULT;

    if (PeekOne(Token_LeftSquareBracket, 2) == NULL)
        return NOT_FOUND_RESULT;

    Consume();
    Consume();
    Consume();
    SET_LINE_NUMBER;

    NodePtr length = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&length));
    if (length.ptr == NULL)
        return ERROR_RESULT_LINE("Expected array length");
    SET_LINE_NUMBER;

    if (MatchOne(Token_RightSquareBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightSquareBracket);
    SET_LINE_NUMBER;

    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);

    *out = AllocASTNode(
        &(VarDeclStmt)
        {
            .type = TokenToTypeReference(*type),
            .lineNumber = identifier->lineNumber,
            .name = AllocateString(identifier->text),
            .externalName = NULL,
            .arrayLength = length,
            .initializer = NULL_NODE,
            .array = true,
            .public = false,
            .external = false,
        },
        sizeof(VarDeclStmt), Node_VariableDeclaration);
    return SUCCESS_RESULT;
}

static Result ParseImportStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const bool publicFound = PeekOne(Token_Public, 0) != NULL;

    const Token* import = PeekOne(Token_Import, 0 + publicFound);
    if (import == NULL)
        return NOT_FOUND_RESULT;

    const Token* path = PeekOne(Token_StringLiteral, 1 + publicFound);
    if (path == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected path after \"#t\"", Token_Import);

    if (publicFound) Consume();
    Consume();
    Consume();

    *out = AllocASTNode(
        &(ImportStmt)
        {
            .lineNumber = import->lineNumber,
            .path = AllocateString(path->text),
            .moduleName = NULL,
            .public = publicFound,
        },
        sizeof(ImportStmt), Node_Import);
    return SUCCESS_RESULT;
}

static Result ParseWhileStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* whileToken = MatchOne(Token_While);
    if (whileToken == NULL) return NOT_FOUND_RESULT;

    if (MatchOne(Token_LeftBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_LeftBracket);
    SET_LINE_NUMBER

    NodePtr expr = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&expr));
    if (expr.ptr == NULL)
        return ERROR_RESULT_LINE("Expected expression");
    SET_LINE_NUMBER

    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);
    SET_LINE_NUMBER

    NodePtr block = NULL_NODE;
    HANDLE_ERROR(ParseBlockStatement(&block));
    if (block.ptr == NULL)
    {
        NodePtr stmt = NULL_NODE;
        HANDLE_ERROR(ParseStatement(&stmt));
        if (stmt.ptr == NULL)
            return ERROR_RESULT_LINE("Expected statement after while statement");

        Array statements = AllocateArray(sizeof(NodePtr));
        ArrayAdd(&statements, &stmt);
        block = AllocASTNode(&(BlockStmt){.statements = statements,}, sizeof(BlockStmt), Node_Block);
    }

    *out = AllocASTNode(
        &(WhileStmt)
        {
            .lineNumber = whileToken->lineNumber,
            .expr = expr,
            .stmt = block,
        },
        sizeof(WhileStmt), Node_While);
    return SUCCESS_RESULT;
}

static Result ParseForStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* forToken = MatchOne(Token_For);
    if (forToken == NULL)
        return NOT_FOUND_RESULT;
    SET_LINE_NUMBER

    if (MatchOne(Token_LeftBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_LeftBracket);
    SET_LINE_NUMBER

    NodePtr initializer = NULL_NODE;
    HANDLE_ERROR(ParseStatement(&initializer));
    SET_LINE_NUMBER

    if (initializer.type != Node_Null &&
        initializer.type != Node_VariableDeclaration &&
        initializer.type != Node_ExpressionStatement)
        return ERROR_RESULT_LINE(
            "Only expression and variable declaration statements are allowed inside for loop initializers");

    if (initializer.type == Node_Null)
    {
        if (MatchOne(Token_Semicolon) == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
        SET_LINE_NUMBER
    }

    NodePtr condition = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&condition));
    SET_LINE_NUMBER
    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
    SET_LINE_NUMBER

    NodePtr increment = NULL_NODE;
    HANDLE_ERROR(ParseExpression(&increment));
    SET_LINE_NUMBER

    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    NodePtr block = NULL_NODE;
    HANDLE_ERROR(ParseBlockStatement(&block));
    if (block.ptr == NULL)
    {
        NodePtr stmt = NULL_NODE;
        HANDLE_ERROR(ParseStatement(&stmt));
        if (stmt.ptr == NULL)
            return ERROR_RESULT_LINE("Expected statement after for statement");

        Array statements = AllocateArray(sizeof(NodePtr));
        ArrayAdd(&statements, &stmt);
        block = AllocASTNode(&(BlockStmt){.statements = statements,}, sizeof(BlockStmt), Node_Block);
    }

    *out = AllocASTNode(
        &(ForStmt)
        {
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
    long SET_LINE_NUMBER

    const Token* token = MatchOne(Token_Break);
    if (token == NULL) token = MatchOne(Token_Continue);
    if (token == NULL) return NOT_FOUND_RESULT;

    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE("Expected \";\"");

    *out = AllocASTNode(
        &(LoopControlStmt)
        {
            .lineNumber = token->lineNumber,
            .type = token->type == Token_Break ? LoopControl_Break : LoopControl_Continue,
        },
        sizeof(LoopControlStmt), Node_LoopControl);
    return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    Result result = ParseImportStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseFunctionDeclaration(out);
    if (result.success || result.hasError)
        return result;

    result = ParseStructDeclaration(out);
    if (result.success || result.hasError)
        return result;

    result = ParseIfStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseLoopControlStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseWhileStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseForStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseArrayDeclaration(out);
    if (result.success || result.hasError)
        return result;

    result = ParseVariableDeclaration(out, true);
    if (result.success || result.hasError)
        return result;

    result = ParseSectionStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseBlockStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseReturnStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseExpressionStatement(out);
    if (result.success || result.hasError)
        return result;

    return NOT_FOUND_RESULT;
}

static Result ParseProgram(AST* out)
{
    long SET_LINE_NUMBER

    Array stmtArray = AllocateArray(sizeof(NodePtr));

    bool allowImportStatements = true;
    while (true)
    {
        if (MatchOne(Token_EndOfFile) != NULL)
            break;

        NodePtr stmt = NULL_NODE;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseStatement(&stmt));
        if (stmt.ptr == NULL)
            return ERROR_RESULT_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->lineNumber, CurrentToken(0)->type);

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

    *out = (AST){stmtArray};
    return SUCCESS_RESULT;
}

Result Parse(const Array* tokenArray, AST* outSyntaxTree)
{
    pointer = 0;
    tokens = *tokenArray;

    return ParseProgram(outSyntaxTree);
}
