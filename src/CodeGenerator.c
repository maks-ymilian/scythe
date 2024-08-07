#include "CodeGenerator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SyntaxTreePrinter.h"
#include "data-structures/Map.h"

#define ALLOC_FORMAT_STRING(formatLiteral, insert)\
const char* formatString;\
{const size_t insertLength = strlen(insert);\
const size_t formatLength = sizeof(formatLiteral) - 3;\
const size_t bufferLength = insertLength + formatLength + 1;\
char* str = malloc(bufferLength);\
snprintf(str, bufferLength, formatLiteral, insert);\
formatString = str;}
static Map types;

#define GENERATE_AND_HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

typedef uint32_t Type;

typedef struct
{
    Type type;
} SymbolAttributes;

static Map symbolTable;

static void AddType(const char* name)
{
    const Type type = types.elementCount;
    MapAdd(&types, name, &type);
}

static void AddSymbol(const char* name, const Type type)
{
    SymbolAttributes attributes;
    attributes.type = type;
    MapAdd(&symbolTable, name, &attributes);
}

static Result GenerateLiteralExpression(const LiteralExpr* in)
{
    const Token literal = in->value;
    switch (literal.type)
    {
        case NumberLiteral:
            return SUCCESS_RESULT;
        case StringLiteral:
            return SUCCESS_RESULT;
        case Identifier:
        {
            const char* name = literal.text;
            SymbolAttributes* symbol = MapGet(&symbolTable, name);
            if (symbol == NULL)
            {
                ALLOC_FORMAT_STRING("Unknown identifier \"%s\"", name);
                return ERROR_RESULT(formatString, literal.lineNumber);
            }
            return SUCCESS_RESULT;
        }
        default:
            assert(0);
    }
    return SUCCESS_RESULT;
}

static Result GenerateExpression(const ExprPtr* in);

static Result GenerateUnaryExpression(const UnaryExpr* in)
{
    GENERATE_AND_HANDLE_ERROR(GenerateExpression(&in->expression));
    return SUCCESS_RESULT;
}

static Result GenerateBinaryExpression(const BinaryExpr* in)
{
    GENERATE_AND_HANDLE_ERROR(GenerateExpression(&in->left));
    GENERATE_AND_HANDLE_ERROR(GenerateExpression(&in->right));
    return SUCCESS_RESULT;
}

static Result GenerateExpression(const ExprPtr* in)
{
    switch (in->type)
    {
        case NoExpression:
            return SUCCESS_RESULT;
        case BinaryExpression:
            return GenerateBinaryExpression(in->ptr);
        case UnaryExpression:
            return GenerateUnaryExpression(in->ptr);
        case LiteralExpression:
            return GenerateLiteralExpression(in->ptr);
        default:
            assert(0);
    }
}

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    return GenerateExpression(&in->expr);
}

static Result GenerateVariableDeclaration(const VarDeclStmt* in)
{
    const Token typeToken = in->type;
    Type type;
    if (typeToken.type == Identifier)
    {
        const char* typeName = typeToken.text;
        const Type* get = MapGet(&types, typeName);
        if (get == NULL)
        {
            ALLOC_FORMAT_STRING("Unknown type \"%s\"", typeName);
            return ERROR_RESULT(formatString, typeToken.lineNumber);
        }
        type = *get;
    }
    else
    {
        switch (typeToken.type)
        {
            case Int:
                break;
            default: assert(0);
        }

        const char* typeName = GetTokenTypeString(typeToken.type);
        const Type* get = MapGet(&types, typeName);
        assert(get != NULL);
        type = *get;
    }

    assert(in->identifier.type == Identifier);
    AddSymbol(in->identifier.text, type);

    return SUCCESS_RESULT;
}

static Result GenerateStatement(const StmtPtr* in);

static Result GenerateBlockStatement(const BlockStmt* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const StmtPtr* stmt = in->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", 69420);
        GENERATE_AND_HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

static Result GenerateSectionStatement(const SectionStmt* in)
{
    return GenerateBlockStatement(in->block.ptr);
}

static Result GenerateStatement(const StmtPtr* in)
{
    switch (in->type)
    {
        case ExpressionStatement:
            return GenerateExpressionStatement(in->ptr);
        case Section:
            return GenerateSectionStatement(in->ptr);
        case VariableDeclaration:
            return GenerateVariableDeclaration(in->ptr);
        default:
            assert(0);
    }
}

Result GenerateProgram(const Program* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const StmtPtr* stmt = in->statements.array[i];
        assert(stmt->type != ProgramRoot);
        GENERATE_AND_HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree)
{
    symbolTable = AllocateMap(sizeof(SymbolAttributes));
    types = AllocateMap(sizeof(Type));
    AddType("int");

    const StmtPtr stmt = {syntaxTree, ProgramRoot};
    PrintSyntaxTree(&stmt);

    GENERATE_AND_HANDLE_ERROR(GenerateProgram(syntaxTree));

    FreeSyntaxTree(stmt);
    FreeMap(&types);
    FreeMap(&symbolTable);

    return SUCCESS_RESULT;
}
