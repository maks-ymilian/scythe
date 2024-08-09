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
static Result functionName(ExprPtr* out)\
{\
    long SET_LINE_NUMBER\
\
    ExprPtr left;\
    HANDLE_ERROR(nextFunction(&left),\
        return result;);\
\
    const TokenType validOperators[length] = {operators};\
    const Token* operator = Match(validOperators, length);\
    while (operator != NULL)\
    {\
        ExprPtr right;\
        SET_LINE_NUMBER\
        HANDLE_ERROR(nextFunction(&right),\
            return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"%t\"", operator->type);)\
\
        BinaryExpr* binary = AllocBinary((BinaryExpr){left, *operator, right});\
        left = (ExprPtr){binary, BinaryExpression};\
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

static Result ParseExpression(ExprPtr* out);

static Result ParsePrimary(ExprPtr* out)
{
    long SET_LINE_NUMBER

    const Token* token = Match((TokenType[]){NumberLiteral, StringLiteral, Identifier, LeftBracket}, 4);
    if (token == NULL)
        return UNSUCCESSFUL_RESULT_LINE_TOKEN("Unexpected token \"%t\"", CurrentToken(0)->type);

    switch (token->type)
    {
        case LeftBracket:
        {
            HANDLE_ERROR(ParseExpression(out),
                                   return ERROR_RESULT_LINE("Expected expression"));

            const Token* closingBracket = MatchOne(RightBracket);
            if (closingBracket == NULL)
                return ERROR_RESULT_LINE_TOKEN("Expected \"%t\"", RightBracket);

            return result;
        }
        case NumberLiteral:
        case StringLiteral:
        case Identifier:
        {
            LiteralExpr* literal = AllocLiteral((LiteralExpr){*token});
            *out = (ExprPtr){literal, LiteralExpression};
            return SUCCESS_RESULT;
        }
        default: assert(0);
    }
}

static Result ParseUnary(ExprPtr* out)
{
    long SET_LINE_NUMBER

    const Token* operator = Match((TokenType[]){Plus, Minus, Exclamation, PlusPlus, MinusMinus}, 5);
    if (operator == NULL)
        return ParsePrimary(out);

    UnaryExpr* unary = AllocUnary((UnaryExpr){*operator});
    ExprPtr expr;
    SET_LINE_NUMBER
    if (!ParseUnary(&expr).success)
        return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"%t\"", operator->type);
    unary->expression = expr;

    *out = (ExprPtr){unary, UnaryExpression};
    return SUCCESS_RESULT;
}

DEF_LEFT_BINARY_PRODUCTION(ParseFactor, ParseUnary, Asterisk COMMA Slash, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseTerm, ParseFactor, Plus COMMA Minus, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseComparison, ParseTerm,
                           RightAngleBracket COMMA RightAngleEquals COMMA LeftAngleBracket COMMA LeftAngleEquals, 4)
DEF_LEFT_BINARY_PRODUCTION(ParseEquality, ParseComparison, EqualsEquals COMMA ExclamationEquals, 2)

static Result ParseAssignment(ExprPtr* out)
{
    long SET_LINE_NUMBER

    ExprPtr left;
    HANDLE_ERROR(ParseEquality(&left),
                           return result);

    Array exprArray = AllocateArray(sizeof(ExprPtr));
    ArrayAdd(&exprArray, &left);

    const TokenType validOperators[5] = {Equals, PlusEquals, MinusEquals, AsteriskEquals, SlashEquals};
    const Token* operator = Match(validOperators, 5);
    Array operatorArray = AllocateArray(sizeof(Token));
    while (operator != NULL)
    {
        ArrayAdd(&operatorArray, operator);

        ExprPtr right;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseEquality(&right),
                               return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"%t\"", operator->type));

        ArrayAdd(&exprArray, &right);

        operator = Match(validOperators, 5);
    }

    ExprPtr* expr1 = exprArray.array[exprArray.length - 1];

    for (int i = exprArray.length - 2; i >= 0; --i)
    {
        operator = operatorArray.array[i];
        const ExprPtr* expr2 = exprArray.array[i];

        BinaryExpr* binaryExpr = AllocBinary((BinaryExpr){*expr2, *operator, *expr1});
        *expr1 = (ExprPtr){binaryExpr, BinaryExpression};
    }

    *out = *expr1;
    FreeArray(&exprArray);
    FreeArray(&operatorArray);

    return SUCCESS_RESULT;
}

static Result ParseExpression(ExprPtr* out)
{
    long SET_LINE_NUMBER

    return ParseAssignment(out);
}

static Result ParseExpressionStatement(StmtPtr* out)
{
    long SET_LINE_NUMBER

    ExprPtr expr;
    bool noExpression = false;
    HANDLE_ERROR(ParseExpression(&expr),
                           {
                           expr.ptr = NULL;
                           expr.type = NoExpression;
                           noExpression = true;
                           });
    const Result expressionResult = result;

    const Token* semicolon = MatchOne(Semicolon);
    if (semicolon == NULL)
    {
        if (noExpression)
            return expressionResult;
        return ERROR_RESULT_LINE_TOKEN("Expected \"%t\"", Semicolon)
    }

    const ExpressionStmt* expressionStmt = AllocExpressionStmt((ExpressionStmt){expr});
    *out = (StmtPtr){(void*)expressionStmt, ExpressionStatement};
    return SUCCESS_RESULT;
}

static Result ParseStatement(StmtPtr* out);

static Result ParseBlockStatement(StmtPtr* out)
{
    long SET_LINE_NUMBER

    const Token* openingBrace = MatchOne(LeftCurlyBracket);
    if (openingBrace == NULL)
        return NOT_FOUND_RESULT;

    Array statements = AllocateArray(sizeof(StmtPtr));
    Result exitResult = {0, 0, NULL};
    while (true)
    {
        StmtPtr stmt;
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
    *out = (StmtPtr){block, BlockStatement};
    return SUCCESS_RESULT;
}

static Result ParseSectionStatement(StmtPtr* out)
{
    long SET_LINE_NUMBER

    const Token* at = MatchOne(At);
    if (at == NULL)
        return NOT_FOUND_RESULT;

    const Token* sectionType = Match((TokenType[]){Init, Slider, Block, Sample, Serialize, GFX}, 6);
    if (sectionType == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected section type after \"%t\"", At);

    StmtPtr block;
    HANDLE_ERROR(ParseBlockStatement(&block),
                           return ERROR_RESULT_LINE("Expected block after section statement"));
    assert(block.type == BlockStatement);

    SectionStmt* section = AllocSectionStmt((SectionStmt){*sectionType, block});
    *out = (StmtPtr){section, Section};
    return SUCCESS_RESULT;
}

static Result ParseVariableDeclaration(StmtPtr* out)
{
    long SET_LINE_NUMBER

    const Token* type = PeekOne(Identifier, 0);
    if (type != NULL)
    {
        if (PeekOne(Identifier, 1) == NULL)
            return NOT_FOUND_RESULT;
    }
    else
    {
        type = Peek((TokenType[]){Int}, 1, 0);
        if (type == NULL)
            return NOT_FOUND_RESULT;
    }
    Consume();

    SET_LINE_NUMBER
    const Token* name = MatchOne(Identifier);
    if (name == NULL)
        return ERROR_RESULT_LINE("Expected identifier after type");

    ExprPtr initializer = {NULL, NoExpression};
    const Token* equals = MatchOne(Equals);
    if (equals != NULL)
    {
        HANDLE_ERROR(ParseExpression(&initializer),
                               return ERROR_RESULT_LINE("Expected expression"))
    }

    const Token* semicolon = MatchOne(Semicolon);
    if (semicolon == NULL)
        return ERROR_RESULT_LINE("Expected \";\"");

    VarDeclStmt* varDecl = AllocVarDeclStmt((VarDeclStmt){*type, *name, initializer});
    *out = (StmtPtr){varDecl, VariableDeclaration};
    return SUCCESS_RESULT;
}

static Result ParseStatement(StmtPtr* out)
{
    long SET_LINE_NUMBER

    Result result = ParseVariableDeclaration(out);
    if (result.success || result.hasError)
        return result;

    result = ParseSectionStatement(out);
    if (result.success || result.hasError)
        return result;

    result = ParseExpressionStatement(out);
    return result;
}

static Result ParseProgram(StmtPtr* out)
{
    long SET_LINE_NUMBER

    Array stmtArray = AllocateArray(sizeof(StmtPtr));

    while (true)
    {
        const Token* end = MatchOne(EndOfFile);
        if (end != NULL)
            break;

        StmtPtr stmt;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseStatement(&stmt),
                               return ERROR_RESULT_LINE("Expected statement"))

        if (stmt.type != Section)
            return ERROR_RESULT_LINE("Expected section statement");

        ArrayAdd(&stmtArray, &stmt);
    }

    Program* program = AllocProgram((Program){stmtArray});
    *out = (StmtPtr){program, ProgramRoot};
    return SUCCESS_RESULT;
}

Result Parse(const Array* tokenArray, StmtPtr* outSyntaxTree)
{
    tokens = *tokenArray;

    StmtPtr program = {NULL};

    const Result result = ParseProgram(&program);
    FreeTokenArray(&tokens);
    if (result.success)
        *outSyntaxTree = program;

    return result;
}
