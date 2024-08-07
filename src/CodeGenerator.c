#include "CodeGenerator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SyntaxTreePrinter.h"
#include "data-structures/Map.h"

#define ALLOC_FORMAT_STRING(formatLiteral, insert)\
const char* formatString;\
{const size_t insertLength = strlen(insert) + 1;\
const size_t formatLength = sizeof(formatLiteral);\
char* str = malloc(insertLength + formatLength);\
snprintf(str, insertLength, formatLiteral, insert);\
formatString = str;}
static Map types;

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

static Result GenerateAssignment(ExprPtr* in)
{
}

static Result GenerateExpression(ExprPtr* in)
{
}

static Result GenerateExpressionStatement(ExpressionStmt* in)
{
}

static Result GenerateVariableDeclaration(VarDeclStmt* in)
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
        GenerateStatement(stmt);
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
        GenerateStatement(stmt);
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

    GenerateProgram(syntaxTree);

    FreeSyntaxTree(stmt);
    FreeMap(&types);
    FreeMap(&symbolTable);

    return SUCCESS_RESULT;
}
