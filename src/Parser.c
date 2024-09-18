#include "Parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "SyntaxTreePrinter.h"

#define SET_LINE_NUMBER lineNumber = ((Token*)tokens.array[pointer])->lineNumber;

#define NOT_FOUND_RESULT (Result){false, false, NULL, 0}
#define ERROR_RESULT_LINE(message) ERROR_RESULT(message, lineNumber);
#define ERROR_RESULT_LINE_TOKEN(message, tokenType) ERROR_RESULT_TOKEN(message, lineNumber, tokenType)
#define UNSUCCESSFUL_RESULT_LINE_TOKEN(message, tokenType) (Result){false, false, message, lineNumber, tokenType};

#define HANDLE_ERROR(parseFunction, unsuccessfulCode)\
const Result result = parseFunction;\
if (result.hasError)\
    return result;\
if (!result.success)\
{unsuccessfulCode;}

#define COMMA ,
#define DEF_LEFT_BINARY_PRODUCTION(functionName, nextFunction, operators, length)\
static Result functionName(NodePtr* out)\
{\
    long SET_LINE_NUMBER\
\
    NodePtr left;\
    HANDLE_ERROR(nextFunction(&left),\
        return result;);\
\
    const TokenType validOperators[length] = {operators};\
    const Token* operator = Match(validOperators, length);\
    while (operator != NULL)\
    {\
        NodePtr right;\
        SET_LINE_NUMBER\
        HANDLE_ERROR(nextFunction(&right),\
            return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);)\
\
        BinaryExpr* binary = AllocBinary((BinaryExpr){left, *operator, right});\
        left = (NodePtr){binary, BinaryExpression};\
\
        operator = Match(validOperators, length);\
    }\
\
    *out = left;\
    return SUCCESS_RESULT;\
}

static Array tokens;
static long pointer;

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

static Result ParsePrimary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* token = Match((TokenType[]){NumberLiteral, StringLiteral, Identifier, LeftBracket, True, False}, 6);
    if (token == NULL)
        return UNSUCCESSFUL_RESULT_LINE_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->type);

    switch (token->type)
    {
        case LeftBracket:
        {
            HANDLE_ERROR(ParseExpression(out),
                         return ERROR_RESULT_LINE("Expected expression"));

            const Token* closingBracket = MatchOne(RightBracket);
            if (closingBracket == NULL)
                return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", RightBracket);

            return result;
        }
        case NumberLiteral:
        case StringLiteral:
        case Identifier:
        case True:
        case False:
        {
            LiteralExpr* literal = AllocLiteral((LiteralExpr){*token});
            *out = (NodePtr){literal, LiteralExpression};
            return SUCCESS_RESULT;
        }
        default: assert(0);
    }
}

typedef Result (*ParseFunction)(NodePtr*);

static Result ParseCommaSeparatedList(Array* outArray, const ParseFunction function)
{
    long SET_LINE_NUMBER;

    *outArray = AllocateArray(sizeof(NodePtr));

    NodePtr firstNode;
    bool found = true;
    HANDLE_ERROR(function(&firstNode), found = false);
    if (found)
    {
        ArrayAdd(outArray, &firstNode);
        while (true)
        {
            SET_LINE_NUMBER;
            if (MatchOne(Comma) == NULL)
                break;

            NodePtr node;
            HANDLE_ERROR(function(&node),
                         return ERROR_RESULT_LINE_TOKEN("Expected item after \"#t\"", Comma))

            ArrayAdd(outArray, &node);
        }
    }

    return SUCCESS_RESULT;
}

static Result ParseFunctionCall(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* identifier = PeekOne(Identifier, 0);
    if (identifier == NULL || PeekOne(LeftBracket, 1) == NULL)
        return ParsePrimary(out);

    assert(MatchOne(Identifier) != NULL);
    assert(MatchOne(LeftBracket) != NULL);

    Array params;
    HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseExpression), assert(0));

    SET_LINE_NUMBER;
    if (MatchOne(RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", RightBracket);

    FuncCallExpr* func = AllocFuncCall((FuncCallExpr){*identifier, params});
    *out = (NodePtr){func, FunctionCallExpression};

    return SUCCESS_RESULT;
}

static Result ParseUnary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* operator = Match((TokenType[]){Plus, Minus, Exclamation, PlusPlus, MinusMinus}, 5);
    if (operator == NULL)
        return ParseFunctionCall(out);

    UnaryExpr* unary = AllocUnary((UnaryExpr){*operator});
    NodePtr expr;
    SET_LINE_NUMBER
    if (!ParseUnary(&expr).success)
        return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);
    unary->expression = expr;

    *out = (NodePtr){unary, UnaryExpression};
    return SUCCESS_RESULT;
}

DEF_LEFT_BINARY_PRODUCTION(ParseFactor, ParseUnary, Asterisk COMMA Slash, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseTerm, ParseFactor, Plus COMMA Minus, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseNumberComparison, ParseTerm,
                           RightAngleBracket COMMA RightAngleEquals COMMA LeftAngleBracket COMMA LeftAngleEquals, 4)
DEF_LEFT_BINARY_PRODUCTION(ParseEquality, ParseNumberComparison, EqualsEquals COMMA ExclamationEquals, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseBooleanOperators, ParseEquality, AmpersandAmpersand COMMA PipePipe, 2)

static Result ParseAssignment(NodePtr* out)
{
    long SET_LINE_NUMBER

    NodePtr left;
    HANDLE_ERROR(ParseBooleanOperators(&left),
                 return result);

    Array exprArray = AllocateArray(sizeof(NodePtr));
    ArrayAdd(&exprArray, &left);

    const TokenType validOperators[5] = {Equals, PlusEquals, MinusEquals, AsteriskEquals, SlashEquals};
    const Token* operator = Match(validOperators, 5);
    Array operatorArray = AllocateArray(sizeof(Token));
    while (operator != NULL)
    {
        ArrayAdd(&operatorArray, operator);

        NodePtr right;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseEquality(&right),
                     return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type));

        ArrayAdd(&exprArray, &right);

        operator = Match(validOperators, 5);
    }

    NodePtr* expr1 = exprArray.array[exprArray.length - 1];

    for (int i = exprArray.length - 2; i >= 0; --i)
    {
        operator = operatorArray.array[i];
        const NodePtr* expr2 = exprArray.array[i];

        BinaryExpr* binaryExpr = AllocBinary((BinaryExpr){*expr2, *operator, *expr1});
        *expr1 = (NodePtr){binaryExpr, BinaryExpression};
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

    NodePtr expr;
    HANDLE_ERROR(ParseExpression(&expr),
                 {
                 if (MatchOne(Semicolon) == NULL) return result;
                 expr.ptr = NULL;
                 expr.type = NullNode;
                 });

    if (MatchOne(Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Semicolon)

    const ExpressionStmt* expressionStmt = AllocExpressionStmt((ExpressionStmt){expr});
    *out = (NodePtr){(void*)expressionStmt, ExpressionStatement};
    return SUCCESS_RESULT;
}

static Result ParseReturnStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* returnToken = MatchOne(Return);
    if (returnToken == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER
    NodePtr expr;
    HANDLE_ERROR(ParseExpression(&expr),
                 expr.type = NullNode;
                 expr.ptr = NULL;)

    if (MatchOne(Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Semicolon)

    const ReturnStmt* returnStmt = AllocReturnStmt((ReturnStmt){expr, *returnToken});
    *out = (NodePtr){(void*)returnStmt, ReturnStatement};

    return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out);

static Result ParseIfStatement(NodePtr* out)
{
    long SET_LINE_NUMBER;

    const Token* ifToken = MatchOne(If);
    if (ifToken == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER;
    if (MatchOne(LeftBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", LeftBracket);

    SET_LINE_NUMBER;
    NodePtr expr; {
        HANDLE_ERROR(ParseExpression(&expr),
                     return ERROR_RESULT_LINE("Expected expression in if statement"));
    }

    SET_LINE_NUMBER;
    if (MatchOne(RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", RightBracket);

    SET_LINE_NUMBER;
    NodePtr stmt;
    HANDLE_ERROR(ParseStatement(&stmt),
                 return ERROR_RESULT_LINE("Expected statement"));

    SET_LINE_NUMBER;
    NodePtr elseStmt;
    if (MatchOne(Else))
    {
        HANDLE_ERROR(ParseStatement(&elseStmt),
                     return ERROR_RESULT_LINE_TOKEN("Expected statement after \"#t\"", Else));
    }
    else
        elseStmt = (NodePtr){NULL, NullNode};

    const IfStmt* ifStmt = AllocIfStmt((IfStmt){*ifToken, expr, stmt, elseStmt});
    *out = (NodePtr){(void*)ifStmt, IfStatement};

    return SUCCESS_RESULT;
}

static Result ParseBlockStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* openingBrace = MatchOne(LeftCurlyBracket);
    if (openingBrace == NULL)
        return NOT_FOUND_RESULT;

    Array statements = AllocateArray(sizeof(NodePtr));
    Result exitResult = {0, 0, NULL};
    while (true)
    {
        NodePtr stmt;
        SET_LINE_NUMBER
        bool noStatement = false;
        HANDLE_ERROR(ParseStatement(&stmt),
                     noStatement = true);
        if (noStatement)
        {
            if (result.errorMessage != NULL)
                exitResult = result;
            break;
        }

        ArrayAdd(&statements, &stmt);
    }

    const Token* closingBrace = MatchOne(RightCurlyBracket);
    if (closingBrace == NULL)
    {
        if (exitResult.errorMessage != NULL)
            return ERROR_RESULT_LINE_TOKEN(exitResult.errorMessage, exitResult.tokenType);

        assert(0);
    }

    BlockStmt* block = AllocBlockStmt((BlockStmt){statements});
    *out = (NodePtr){block, BlockStatement};
    return SUCCESS_RESULT;
}

static Token* PeekType(const int offset)
{
    Token* type = PeekOne(Identifier, offset);
    if (type == NULL)
    {
        type = Peek((TokenType[]){Void, Int, Float, String, Bool}, 5, offset);
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

    const Token* sectionType = Match((TokenType[]){Init, Slider, Block, Sample, Serialize, GFX}, 6);
    if (sectionType == NULL)
        return NOT_FOUND_RESULT;

    NodePtr block;
    HANDLE_ERROR(ParseBlockStatement(&block),
                 return ERROR_RESULT_LINE("Expected block after section statement"));
    assert(block.type == BlockStatement);

    SectionStmt* section = AllocSectionStmt((SectionStmt){*sectionType, block});
    *out = (NodePtr){section, Section};
    return SUCCESS_RESULT;
}

static Result ParseVariableDeclaration(NodePtr* out, const bool expectSemicolon)
{
    long SET_LINE_NUMBER

    const Token* type = PeekType(0);
    const Token* name = PeekOne(Identifier, 1);

    if (!(name && type))
        return NOT_FOUND_RESULT;

    Consume();
    Consume();

    NodePtr initializer = {NULL, NullNode};
    const Token* equals = MatchOne(Equals);
    if (equals != NULL)
    {
        HANDLE_ERROR(ParseExpression(&initializer),
                     return ERROR_RESULT_LINE("Expected expression"))
    }

    if (expectSemicolon)
    {
        const Token* semicolon = MatchOne(Semicolon);
        if (semicolon == NULL)
            return ERROR_RESULT_LINE("Expected \";\"");
    }

    VarDeclStmt* varDecl = AllocVarDeclStmt((VarDeclStmt){*type, *name, initializer});
    *out = (NodePtr){varDecl, VariableDeclaration};
    return SUCCESS_RESULT;
}

static Result ParseVarDeclNoSemicolon(NodePtr* out)
{
    return ParseVariableDeclaration(out, false);
}

static Result ParseFunctionDeclaration(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* type = PeekType(0);
    const Token* identifier = PeekOne(Identifier, 1);

    if (!(identifier && type && PeekOne(LeftBracket, 2)))
        return NOT_FOUND_RESULT;

    Consume();
    Consume();
    Consume();

    Array params; {
        HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseVarDeclNoSemicolon), assert(0));
    }

    SET_LINE_NUMBER;
    if (MatchOne(RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", RightBracket);

    NodePtr block;
    HANDLE_ERROR(ParseBlockStatement(&block),
                 return ERROR_RESULT_LINE("Expected code block after function declaration"));

    FuncDeclStmt* funcDecl = AllocFuncDeclStmt((FuncDeclStmt){*type, *identifier, params, block});
    *out = (NodePtr){funcDecl, FunctionDeclaration};

    return SUCCESS_RESULT;
}

static Result ParseStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    Result result = ParseFunctionDeclaration(out);
    if (result.success || result.hasError)
        return result;

    result = ParseIfStatement(out);
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
    return result;
}

static Result ParseProgram(NodePtr* out)
{
    long SET_LINE_NUMBER

    Array stmtArray = AllocateArray(sizeof(NodePtr));

    while (true)
    {
        const Token* end = MatchOne(EndOfFile);
        if (end != NULL)
            break;

        NodePtr stmt;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseStatement(&stmt),
                     return ERROR_RESULT_LINE("Expected statement"))

        if (stmt.type != Section)
            return ERROR_RESULT_LINE("Expected section statement");

        ArrayAdd(&stmtArray, &stmt);
    }

    Program* program = AllocProgram((Program){stmtArray});
    *out = (NodePtr){program, RootNode};
    return SUCCESS_RESULT;
}

Result Parse(const Array* tokenArray, NodePtr* outSyntaxTree)
{
    tokens = *tokenArray;

    NodePtr program = {NULL};

    const Result result = ParseProgram(&program);
    FreeTokenArray(&tokens);
    if (result.success)
        *outSyntaxTree = program;

    return result;
}
