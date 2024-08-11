#include "CodeGenerator.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
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

#define HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

#define WRITE_LITERAL(text) StreamWrite(outputText, text, sizeof(text) - 1)
#define WRITE_TEXT(text) StreamWrite(outputText, text, strlen(text));

#define GET_WRITTEN_CHARS(code, chars, charsLength)\
{const size_t pos = StreamGetPosition(outputText);\
code;\
const size_t length = StreamGetPosition(outputText) - pos;\
chars = (char*)StreamRewindRead(outputText, length).buffer;\
charsLength = length;}

typedef enum
{
    Primitive, Struct, Enum
} MetaType;

typedef struct
{
    MetaType metaType;
    uint32_t id;
} Type;

static Map types;
static Array typeNames;

typedef struct
{
    Type type;
} SymbolAttributes;

static Map symbolTable;

static MemoryStream* outputText;

static bool AddType(const char* name, const MetaType metaType)
{
    Type type;
    type.id = types.elementCount;
    type.metaType = metaType;

    const bool success = MapAdd(&types, name, &type);
    if (!success)
        return false;

    char* typeName = malloc(strlen(name) + 1);
    strcpy(typeName, name);
    ArrayAdd(&typeNames, &typeName);

    return true;
}

static Type GetKnownType(const char* name)
{
    const Type* type = MapGet(&types, name);
    assert(type != NULL);
    return *type;
}

static const char* GetTypeName(const Type type)
{
    return *(char**)typeNames.array[type.id];
}

static bool AddSymbol(const char* name, const Type type)
{
    SymbolAttributes attributes;
    attributes.type = type;
    return MapAdd(&symbolTable, name, &attributes);
}

static SymbolAttributes GetKnownSymbol(const char* name)
{
    const SymbolAttributes* symbol = MapGet(&symbolTable, name);
    assert(symbol != NULL);
    return *symbol;
}

static void FreeTables()
{
    FreeMap(&types);
    FreeMap(&symbolTable);

    for (int i = 0; i < typeNames.length; ++i)
        free(*(char**)typeNames.array[i]);
    FreeArray(&typeNames);
}

static void InitTables()
{
    symbolTable = AllocateMap(sizeof(SymbolAttributes));
    types = AllocateMap(sizeof(Type));
    typeNames = AllocateArray(sizeof(char*));

    bool success = AddType("int", Primitive);
    assert(success);

    success = AddType("float", Primitive);
    assert(success);

    success = AddType("string", Primitive);
    assert(success);

    success = AddType("bool", Primitive);
    assert(success);
}

static bool IsDigitBase(const char c, const int base)
{
    if (base == 10) return isdigit(c);
    if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (base == 2) return c == '0' || c == '1';
    if (base == 8) return c >= '0' && c <= '7';
    assert(0);
}

static Result EvaluateNumberLiteral(const Token token, bool* outInteger, char* out)
{
    assert(token.type == NumberLiteral);
    assert(token.text != NULL);

    const size_t length = strlen(token.text);
    char text[length + 1];
    memcpy(text, token.text, length + 1);

    int base = 10;
    if (length >= 3 && text[0] == '0')
    {
        if (text[1] == 'x')
            base = 16;
        else if (text[1] == 'o')
            base = 8;
        else if (text[1] == 'b')
            base = 2;
    }

    bool integer = true;
    if (base == 10)
    {
        if (text[length - 1] == 'f' || text[length - 1] == 'F')
            integer = false;
        else
            integer = true;
    }

    if (!integer && base != 10)
        return ERROR_RESULT("Float literal must be base 10", token.lineNumber);

    const size_t startIndex = base == 10 ? 0 : 2;
    const size_t endIndex = integer ? length : length - 1;

    size_t i = startIndex;
    while (i < endIndex)
    {
        if (!IsDigitBase(text[i], base))
        {
            if (text[i] == '.')
            {
                if (integer)
                    return ERROR_RESULT("Dots in integer literals are not allowed", token.lineNumber);
            }
            else
            {
                const char insert[2] = {text[i], '\0'};
                ALLOC_FORMAT_STRING("Unexpected digit in number literal \"%s\"", insert);
                return ERROR_RESULT(formatString, token.lineNumber);
            }
        }

        i++;
    }

    text[endIndex] = '\0';
    if (integer)
    {
        const uint64_t number = strtoull(text + startIndex, NULL, base);
        snprintf(out, length + 1, "%llu", number);
        if (errno == ERANGE)
            return ERROR_RESULT("Invalid integer literal", token.lineNumber);
    }
    else
    {
        float _;
        const int ret = sscanf(text + startIndex, "%f", &_);
        if (ret == 0)
            return ERROR_RESULT("Invalid float literal", token.lineNumber);

        memcpy(out, text + startIndex, endIndex - startIndex + 1);
    }

    *outInteger = integer;

    return SUCCESS_RESULT;
}

static Result GenerateLiteralExpression(const LiteralExpr* in, Type* outType)
{
    const Token literal = in->value;
    switch (literal.type)
    {
        case NumberLiteral:
        {
            char number[strlen(literal.text) + 1];
            bool integer;
            HANDLE_ERROR(EvaluateNumberLiteral(literal, &integer, number));
            *outType = integer ? GetKnownType("int") : GetKnownType("float");

            WRITE_TEXT(number);
            return SUCCESS_RESULT;
        }
        case StringLiteral:
        {
            WRITE_LITERAL("\"");
            WRITE_TEXT(literal.text);
            WRITE_LITERAL("\"");
            *outType = GetKnownType("string");
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
            *outType = symbol->type;
            WRITE_TEXT(name);
            return SUCCESS_RESULT;
        }
        default:
            assert(0);
    }
}

static Result UnaryOperatorErrorResult(const Token operator, const Type type)
{
    ALLOC_FORMAT_STRING("Cannot use operator \"#t\" on type \"%s\"", GetTypeName(type));
    return ERROR_RESULT_TOKEN(formatString, operator.lineNumber, operator.type);
}

static Result GenerateExpression(const ExprPtr* in, Type* outType);

static Result GenerateUnaryExpression(const UnaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    WRITE_TEXT(GetTokenTypeString(in->operator.type));

    Type type;
    HANDLE_ERROR(GenerateExpression(&in->expression, &type));

    switch (in->operator.type)
    {
        case Exclamation:
            if (type.id != GetKnownType("bool").id)
                return UnaryOperatorErrorResult(in->operator, type);
            break;
        case Minus:
            if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
                return UnaryOperatorErrorResult(in->operator, type);
            break;
        case Plus:
            if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
                return UnaryOperatorErrorResult(in->operator, type);
            break;
        default:
            assert(0);
    }

    *outType = type;

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

static Result GenerateBinaryExpression(const BinaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    const char* left;
    size_t leftLength;
    Type leftType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->left, &leftType)),
        left, leftLength);

    const char* operator;
    size_t operatorLength;
    GET_WRITTEN_CHARS(
        WRITE_TEXT(GetTokenTypeString(in->operator.type)),
        operator, operatorLength);

    const char* right;
    size_t rightLength;
    Type rightType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->right, &rightType)),
        right, rightLength);

    printf("LEft: \"%.*s\"\n", (int)leftLength, left);
    printf("OPERatOR: \"%.*s\"\n", (int)operatorLength, operator);
    printf("RIGHT: \"%.*s\"\n\n", (int)rightLength, right);

    StreamRewind(outputText, leftLength + operatorLength + rightLength);
    StreamWrite(outputText, left, leftLength);
    StreamWrite(outputText, operator, operatorLength);
    StreamWrite(outputText, right, rightLength);

    switch (in->operator.type)
    {
        case Equals:
            // assignment
        {
            break;
        }

        case PlusEquals:
        case MinusEquals:
        case SlashEquals:
        case AsteriskEquals:
            // arithmetic assignment
        {
            break;
        }

        case Plus:
        case Minus:
        case Slash:
        case Asterisk:
            // arithmetic
        {
            break;
        }

        case AmpersandAmpersand:
        case PipePipe:
        case EqualsEquals:
        case ExclamationEquals:
        case LeftAngleBracket:
        case RightAngleBracket:
        case LeftAngleEquals:
        case RightAngleEquals:
            // comparison
        {
            break;
        }

        default: assert(0);
    }

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

static Result GenerateExpression(const ExprPtr* in, Type* outType)
{
    switch (in->type)
    {
        case NoExpression:
            return SUCCESS_RESULT;
        case BinaryExpression:
            return GenerateBinaryExpression(in->ptr, outType);
        case UnaryExpression:
            return GenerateUnaryExpression(in->ptr, outType);
        case LiteralExpression:
            return GenerateLiteralExpression(in->ptr, outType);
        default:
            assert(0);
    }
}

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    Type type;
    const Result result = GenerateExpression(&in->expr, &type);
    WRITE_LITERAL(";");
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

    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("=");

    if (in->initializer.type == NoExpression)
        WRITE_LITERAL("0");
    else
    {
        Type initializerType;
        HANDLE_ERROR(GenerateExpression(&in->initializer, &initializerType));

        // Type initializerType;
        // HANDLE_ERROR(EvaluateType(in->initializer, &initializerType));
        // HANDLE_ERROR(CheckTypeCompatibility(initializerType, type, typeToken.lineNumber));
    }

    if (!AddSymbol(in->identifier.text, type))
    {
        ALLOC_FORMAT_STRING("\"%s\" is already defined", in->identifier.text);
        return ERROR_RESULT(formatString, in->identifier.lineNumber);
    }

    WRITE_LITERAL(";");

    return SUCCESS_RESULT;
}

static Result GenerateStatement(const StmtPtr* in);

static Result GenerateBlockStatement(const BlockStmt* in)
{
    WRITE_LITERAL("(");
    for (int i = 0; i < in->statements.length; ++i)
    {
        const StmtPtr* stmt = in->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", 69420);
        HANDLE_ERROR(GenerateStatement(stmt));
    }
    WRITE_LITERAL(")");

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

    WRITE_LITERAL("\n@");
    WRITE_TEXT(GetTokenTypeString(in->type.type));
    WRITE_LITERAL("\n");
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
    InitTables();

    outputText = AllocateMemoryStream();

    const StmtPtr stmt = {syntaxTree, ProgramRoot};
    // PrintSyntaxTree(&stmt);

    const Result result = GenerateProgram(syntaxTree);

    FreeSyntaxTree(stmt);
    FreeTables();

    if (result.hasError)
    {
        FreeMemoryStream(outputText, true);
        return result;
    }

    WRITE_LITERAL("\n");

    const Buffer outputBuffer = StreamRewindRead(outputText, 0);
    FreeMemoryStream(outputText, false);
    *outputCode = outputBuffer.buffer;
    *length = outputBuffer.length;

    return SUCCESS_RESULT;
}
