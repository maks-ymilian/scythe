#include "CodeGenerator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SyntaxTreePrinter.h"
#include "data-structures/Map.h"
#include "data-structures/MemoryStream.h"

#define ALLOC_FORMAT_STRING(formatLiteral, insert)\
const char* formatString;\
{const size_t insertLength = strlen(insert);\
const size_t formatLength = sizeof(formatLiteral) - 3;\
const size_t bufferLength = insertLength + formatLength + 1;\
char* str = malloc(bufferLength);\
snprintf(str, bufferLength, formatLiteral, insert);\
formatString = str;}
static Map types;

#define HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

#define WRITE_STRING_LITERAL(text) WriteText(text, sizeof(text) - 1)
#define WRITE_TEXT(text) { const char* str = text; WriteText(str, strlen(str));}

typedef uint32_t Type;

typedef struct
{
    Type type;
} SymbolAttributes;

static Map symbolTable;

static MemoryStream* outputText;

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

static void WriteText(const char* text, const size_t length)
{
    StreamWrite(outputText, text, length);
}

static Result GenerateLiteralExpression(const LiteralExpr* in)
{
    const Token literal = in->value;
    switch (literal.type)
    {
        case NumberLiteral:
        {
            WRITE_TEXT(literal.text);
            return SUCCESS_RESULT;
        }
        case StringLiteral:
        {
            WRITE_STRING_LITERAL("\"");
            WRITE_TEXT(literal.text);
            WRITE_STRING_LITERAL("\"");
            return SUCCESS_RESULT;
        }
        case Identifier:
        {
            const char* name = literal.text;
            const SymbolAttributes* symbol = MapGet(&symbolTable, name);
            if (symbol == NULL)
            {
                ALLOC_FORMAT_STRING("Unknown identifier \"%s\"", name);
                return ERROR_RESULT(formatString, literal.lineNumber);
            }
            WRITE_TEXT(name);
            return SUCCESS_RESULT;
        }
        default:
            assert(0);
    }
}

static Result GenerateExpression(const ExprPtr* in);

static Result GenerateUnaryExpression(const UnaryExpr* in)
{
    HANDLE_ERROR(GenerateExpression(&in->expression));
    return SUCCESS_RESULT;
}

static Result GenerateBinaryExpression(const BinaryExpr* in)
{
    WRITE_STRING_LITERAL("(");

    HANDLE_ERROR(GenerateExpression(&in->left));

    if (in->operator.type != Plus &&
        in->operator.type != Minus &&
        in->operator.type != Slash &&
        in->operator.type != Asterisk &&
        in->operator.type != PlusEquals &&
        in->operator.type != MinusEquals &&
        in->operator.type != SlashEquals &&
        in->operator.type != AsteriskEquals &&
        in->operator.type != AmpersandAmpersand &&
        in->operator.type != PipePipe &&
        in->operator.type != EqualsEquals &&
        in->operator.type != Equals &&
        in->operator.type != ExclamationEquals &&
        in->operator.type != LeftAngleBracket &&
        in->operator.type != RightAngleBracket &&
        in->operator.type != LeftAngleEquals &&
        in->operator.type != RightAngleEquals
        )
        assert(0);
    WRITE_TEXT(GetTokenTypeString(in->operator.type));

    HANDLE_ERROR(GenerateExpression(&in->right));

    WRITE_STRING_LITERAL(")");
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
    const Result result = GenerateExpression(&in->expr);
    WRITE_STRING_LITERAL(";");
    return result;
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
        if (typeToken.type != Int)
            assert(0);

        const char* typeName = GetTokenTypeString(typeToken.type);
        const Type* get = MapGet(&types, typeName);
        assert(get != NULL);
        type = *get;
    }

    assert(in->identifier.type == Identifier);
    AddSymbol(in->identifier.text, type);

    WRITE_TEXT(in->identifier.text);
    WRITE_STRING_LITERAL("=");

    WRITE_STRING_LITERAL("0");

    WRITE_STRING_LITERAL(";");

    return SUCCESS_RESULT;
}

static Result GenerateStatement(const StmtPtr* in);

static Result GenerateBlockStatement(const BlockStmt* in)
{
    WRITE_STRING_LITERAL("(");
    for (int i = 0; i < in->statements.length; ++i)
    {
        const StmtPtr* stmt = in->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", 69420);
        HANDLE_ERROR(GenerateStatement(stmt));
    }
    WRITE_STRING_LITERAL(")");

    return SUCCESS_RESULT;
}

static Result GenerateSectionStatement(const SectionStmt* in)
{
    if (in->type.type != Init &&
        in->type.type != Slider &&
        in->type.type != Block &&
        in->type.type != Sample &&
        in->type.type != Serialize &&
        in->type.type != GFX)
        assert(0);

    WRITE_STRING_LITERAL("\n@");
    WRITE_TEXT(GetTokenTypeString(in->type.type));
    WRITE_STRING_LITERAL("\n");
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
        HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length)
{
    outputText = AllocateMemoryStream();
    symbolTable = AllocateMap(sizeof(SymbolAttributes));
    types = AllocateMap(sizeof(Type));
    AddType("int");

    const StmtPtr stmt = {syntaxTree, ProgramRoot};
    PrintSyntaxTree(&stmt);

    const Result result = GenerateProgram(syntaxTree);

    FreeSyntaxTree(stmt);
    FreeMap(&types);
    FreeMap(&symbolTable);

    if (result.hasError)
    {
        FreeMemoryStream(outputText, true);
        return result;
    }

    WRITE_STRING_LITERAL("\n");
    const Buffer outputBuffer = FreeMemoryStream(outputText, false);
    *outputCode = outputBuffer.buffer;
    *length = outputBuffer.length;

    return SUCCESS_RESULT;
}
