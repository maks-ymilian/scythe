#include "Parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <StringUtils.h>

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
        left = (NodePtr){binary, Node_Binary};\
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
static Result ParseFunctionCall(NodePtr* out);

static Result ParsePrimary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* token = Match((TokenType[]){Token_NumberLiteral, Token_StringLiteral, Token_Identifier, Token_LeftBracket, Token_True, Token_False}, 6);
    if (token == NULL)
        return UNSUCCESSFUL_RESULT_LINE_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->type);

    switch (token->type)
    {
    case Token_LeftBracket:
    {
        HANDLE_ERROR(ParseExpression(out),
                     return ERROR_RESULT_LINE("Expected expression"));

        const Token* closingBracket = MatchOne(Token_RightBracket);
        if (closingBracket == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

        return result;
    }
    case Token_Identifier:
    {
        LiteralExpr* first = AllocLiteral((LiteralExpr){.value = *token, .next = NULL_NODE});
        LiteralExpr* current = first;
        while (MatchOne(Token_Dot) != NULL)
        {
            bool funcCallFound = true;
            NodePtr funcCall;
            HANDLE_ERROR(ParseFunctionCall(&funcCall), funcCallFound = false);
            if (funcCallFound)
            {
                current->next = funcCall;
                break;
            }

            SET_LINE_NUMBER
            const Token* identifier = MatchOne(Token_Identifier);
            if (identifier == NULL)
                return ERROR_RESULT_LINE_TOKEN("Expected identifier after \"#t\"", Token_Dot);

            LiteralExpr* next = AllocLiteral((LiteralExpr){.value = *identifier, .next = NULL_NODE});
            current->next = (NodePtr){.ptr = next, .type = Node_Literal};
            current = next;
        }

        *out = (NodePtr){first, Node_Literal};
        return SUCCESS_RESULT;
    }
    case Token_NumberLiteral:
    case Token_StringLiteral:
    case Token_True:
    case Token_False:
    {
        LiteralExpr* literal = AllocLiteral((LiteralExpr){.value = *token, .next = NULL_NODE});
        *out = (NodePtr){literal, Node_Literal};
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
            if (MatchOne(Token_Comma) == NULL)
                break;

            NodePtr node;
            HANDLE_ERROR(function(&node),
                         return ERROR_RESULT_LINE_TOKEN("Expected item after \"#t\"", Token_Comma))

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
        return ParsePrimary(out);

    if (MatchOne(Token_Identifier) == NULL)
        assert(0);
    if (MatchOne(Token_LeftBracket) == NULL)
        assert(0);

    Array params;
    HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseExpression), assert(0));

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    FuncCallExpr* func = AllocFuncCall((FuncCallExpr){*identifier, params});
    *out = (NodePtr){func, Node_FunctionCall};

    return SUCCESS_RESULT;
}

static Result ParseUnary(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* operator = Match((TokenType[]){Token_Plus, Token_Minus, Token_Exclamation, Token_PlusPlus, Token_MinusMinus}, 5);
    if (operator == NULL)
        return ParseFunctionCall(out);

    UnaryExpr* unary = AllocUnary((UnaryExpr){*operator});
    NodePtr expr;
    SET_LINE_NUMBER
    if (!ParseUnary(&expr).success)
        return ERROR_RESULT_LINE_TOKEN("Expected expression after operator \"#t\"", operator->type);
    unary->expression = expr;

    *out = (NodePtr){unary, Node_Unary};
    return SUCCESS_RESULT;
}

DEF_LEFT_BINARY_PRODUCTION(ParseFactor, ParseUnary, Token_Asterisk COMMA Token_Slash, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseTerm, ParseFactor, Token_Plus COMMA Token_Minus, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseNumberComparison, ParseTerm,
                           Token_RightAngleBracket COMMA Token_RightAngleEquals COMMA Token_LeftAngleBracket COMMA Token_LeftAngleEquals, 4)
DEF_LEFT_BINARY_PRODUCTION(ParseEquality, ParseNumberComparison, Token_EqualsEquals COMMA Token_ExclamationEquals, 2)
DEF_LEFT_BINARY_PRODUCTION(ParseBooleanOperators, ParseEquality, Token_AmpersandAmpersand COMMA Token_PipePipe, 2)

static Result ParseAssignment(NodePtr* out)
{
    long SET_LINE_NUMBER

    NodePtr left;
    HANDLE_ERROR(ParseBooleanOperators(&left),
                 return result);

    Array exprArray = AllocateArray(sizeof(NodePtr));
    ArrayAdd(&exprArray, &left);

    const TokenType validOperators[5] = {Token_Equals, Token_PlusEquals, Token_MinusEquals, Token_AsteriskEquals, Token_SlashEquals};
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
        *expr1 = (NodePtr){binaryExpr, Node_Binary};
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
    HANDLE_ERROR(ParseExpression(&expr),);

    if (expr.type == Node_Null)
        return NOT_FOUND_RESULT;
    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon)

    const ExpressionStmt* expressionStmt = AllocExpressionStmt((ExpressionStmt){expr});
    *out = (NodePtr){(void*)expressionStmt, Node_ExpressionStatement};
    return SUCCESS_RESULT;
}

static Result ParseReturnStatement(NodePtr* out)
{
    long SET_LINE_NUMBER

    const Token* returnToken = MatchOne(Token_Return);
    if (returnToken == NULL)
        return NOT_FOUND_RESULT;

    SET_LINE_NUMBER
    NodePtr expr;
    HANDLE_ERROR(ParseExpression(&expr), expr = NULL_NODE;)

    if (MatchOne(Token_Semicolon) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon)

    const ReturnStmt* returnStmt = AllocReturnStmt((ReturnStmt){expr, *returnToken});
    *out = (NodePtr){(void*)returnStmt, Node_Return};

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
    NodePtr expr; {
        HANDLE_ERROR(ParseExpression(&expr),
                     return ERROR_RESULT_LINE("Expected expression in if statement"));
    }

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    SET_LINE_NUMBER;
    NodePtr stmt;
    HANDLE_ERROR(ParseStatement(&stmt),
                 return ERROR_RESULT_LINE("Expected statement"));

    SET_LINE_NUMBER;
    NodePtr elseStmt;
    if (MatchOne(Token_Else))
    {
        HANDLE_ERROR(ParseStatement(&elseStmt),
                     return ERROR_RESULT_LINE_TOKEN("Expected statement after \"#t\"", Token_Else));
    }
    else
        elseStmt = NULL_NODE;

    const IfStmt* ifStmt = AllocIfStmt((IfStmt){*ifToken, expr, stmt, elseStmt});
    *out = (NodePtr){(void*)ifStmt, Node_If};

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
        const Token* token = CurrentToken(0);
        return ERROR_RESULT_TOKEN("Unexpected token \"#t\"", token->lineNumber, token->type);
    }

    BlockStmt* block = AllocBlockStmt((BlockStmt){statements});
    *out = (NodePtr){block, Node_Block};
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

    NodePtr block;
    HANDLE_ERROR(ParseBlockStatement(&block),
                 return ERROR_RESULT_LINE("Expected block after section statement"));
    assert(block.type == Node_Block);

    SectionStmt* section = AllocSectionStmt((SectionStmt)
    {
        .identifier = *identifier,
        .sectionType = sectionType,
        .block = block
    });
    *out = (NodePtr){section, Node_Section};
    return SUCCESS_RESULT;
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

        HANDLE_ERROR(ParseExpression(&initializer),
                     return ERROR_RESULT_LINE("Expected expression"))
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

    VarDeclStmt* varDecl = AllocVarDeclStmt((VarDeclStmt)
    {
        .type = *type,
        .identifier = *identifier,
        .externalIdentifier = externalIdentifier,
        .initializer = initializer,
        .public = publicFound,
        .external = externalFound,
    });
    *out = (NodePtr){varDecl, Node_VariableDeclaration};
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

    Array params; {
        HANDLE_ERROR(ParseCommaSeparatedList(&params, ParseVarDeclNoSemicolon), assert(0));
    }

    SET_LINE_NUMBER;
    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    NodePtr block = NULL_NODE;
    bool blockFound = true;
    HANDLE_ERROR(ParseBlockStatement(&block), blockFound = false);
    if (!blockFound && !externalFound) return ERROR_RESULT_LINE("Expected code block after function declaration");
    if (blockFound && externalFound) return ERROR_RESULT_LINE("External functions cannot have code blocks");

    Token externalIdentifier = {};
    if (externalFound)
    {
        const Token* token = MatchOne(Token_Identifier);
        if (token != NULL) externalIdentifier = *token;
        else externalIdentifier = *identifier;

        if (MatchOne(Token_Semicolon) == NULL) return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
    }

    FuncDeclStmt* funcDecl = AllocFuncDeclStmt((FuncDeclStmt)
    {
        .type = *type,
        .identifier = *identifier,
        .externalIdentifier = externalIdentifier,
        .parameters = params,
        .block = block,
        .public = publicFound,
        .external = externalFound,
    });
    *out = (NodePtr){funcDecl, Node_FunctionDeclaration};

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
        NodePtr member;
        HANDLE_ERROR(ParseVariableDeclaration(&member, true),
                     break);
        ArrayAdd(&members, &member);
    }

    SET_LINE_NUMBER
    if (MatchOne(Token_RightCurlyBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Unexpected token \"#t\"", CurrentToken(0)->type);

    StructDeclStmt* structDecl = AllocStructDeclStmt((StructDeclStmt)
    {
        .identifier = *identifier,
        .members = members,
        .public = publicFound,
    });
    *out = (NodePtr){structDecl, Node_StructDeclaration};

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

    ImportStmt* importStmt = AllocImportStmt((ImportStmt){.file = path->text, .import = *import, .public = publicFound});
    *out = (NodePtr){.ptr = importStmt, .type = Node_Import};
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

    NodePtr expr; {
        HANDLE_ERROR(ParseExpression(&expr),
                     return ERROR_RESULT_LINE("Expected expression"));
    }
    SET_LINE_NUMBER

    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);
    SET_LINE_NUMBER

    NodePtr block = NULL_NODE;
    HANDLE_ERROR(ParseBlockStatement(&block),);
    if (block.ptr == NULL)
    {
        NodePtr stmt;
        HANDLE_ERROR(ParseStatement(&stmt),
                     return ERROR_RESULT_LINE("Expected statement after while statement"));

        Array statements = AllocateArray(sizeof(NodePtr));
        ArrayAdd(&statements, &stmt);
        BlockStmt* blockPtr = AllocBlockStmt((BlockStmt){.statements = statements,});
        block = (NodePtr){.ptr = blockPtr, .type = Node_Block};
    }

    WhileStmt* whileStmt = AllocWhileStmt((WhileStmt)
    {
        .whileToken = *whileToken,
        .expr = expr,
        .stmt = block,
    });
    *out = (NodePtr){.ptr = whileStmt, .type = Node_While};
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

    NodePtr initializer = NULL_NODE; {
        HANDLE_ERROR(ParseStatement(&initializer),);
        SET_LINE_NUMBER

        if (initializer.type != Node_Null &&
            initializer.type != Node_VariableDeclaration &&
            initializer.type != Node_ExpressionStatement)
            return ERROR_RESULT_LINE(
                "Only expression and variable declaration statements are allowed inside for loop initializers");
    }

    NodePtr condition = NULL_NODE; {
        HANDLE_ERROR(ParseExpression(&condition),);
        SET_LINE_NUMBER
        if (MatchOne(Token_Semicolon) == NULL)
            return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_Semicolon);
        SET_LINE_NUMBER
    }

    NodePtr increment = NULL_NODE; {
        HANDLE_ERROR(ParseExpression(&increment),);
        SET_LINE_NUMBER
    }

    if (MatchOne(Token_RightBracket) == NULL)
        return ERROR_RESULT_LINE_TOKEN("Expected \"#t\"", Token_RightBracket);

    NodePtr block = NULL_NODE;
    HANDLE_ERROR(ParseBlockStatement(&block),);
    if (block.ptr == NULL)
    {
        NodePtr stmt;
        HANDLE_ERROR(ParseStatement(&stmt),
                     return ERROR_RESULT_LINE("Expected statement after for statement"));

        Array statements = AllocateArray(sizeof(NodePtr));
        ArrayAdd(&statements, &stmt);
        BlockStmt* blockPtr = AllocBlockStmt((BlockStmt){.statements = statements,});
        block = (NodePtr){.ptr = blockPtr, .type = Node_Block};
    }

    ForStmt* forStmt = AllocForStmt((ForStmt)
    {
        .forToken = *forToken,
        .stmt = block,
    });
    *out = (NodePtr){.ptr = forStmt, .type = Node_For};
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

    LoopControlStmt* loopControlStmt = AllocLoopControlStmt((LoopControlStmt)
    {
        .type = token->type == Token_Break ? LoopControl_Break : LoopControl_Continue,
        .lineNumber = token->lineNumber,
    });
    *out = (NodePtr){.ptr = loopControlStmt, .type = Node_LoopControl};
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

        NodePtr stmt;
        SET_LINE_NUMBER
        HANDLE_ERROR(ParseStatement(&stmt),
                     const Token* token = CurrentToken(0);
                     return ERROR_RESULT_TOKEN("Unexpected token \"#t\"", token->lineNumber, token->type));

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
