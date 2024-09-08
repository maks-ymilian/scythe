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

#define HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

#define WRITE_LITERAL(text) StreamWrite(outputText, text, sizeof(text) - 1)
#define WRITE_TEXT(text) StreamWrite(outputText, text, strlen(text));

#define GET_WRITTEN_CHARS(code, arrayName, charsLength)\
const size_t  arrayName##_pos = StreamGetPosition(outputText);\
code;\
const size_t  arrayName##_length = StreamGetPosition(outputText) -  arrayName##_pos;\
char* arrayName##_chars = (char*)StreamRewindRead(outputText,  arrayName##_length).buffer;\
charsLength = arrayName##_length;\
char arrayName[charsLength];\
memcpy(arrayName,  arrayName##_chars, charsLength)

typedef enum { PrimitiveType, StructType, EnumType } MetaType;

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
    char* name;
    NodePtr defaultValue;
} FunctionParameter;

typedef enum { VariableSymbol, FunctionSymbol, StructSymbol } SymbolType;

typedef struct
{
    Type type;
    SymbolType symbolType;
    Array parameterList;
} SymbolAttributes;

static Map symbolTable;

static MemoryStream* outputText;

static Array symbolsAddedThisScope;

static Map includedSections;

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
    assert(type.id < typeNames.length);
    return *(char**)typeNames.array[type.id];
}

static void ClearSymbolsAddedInThisScope()
{
    for (size_t i = 0; i < symbolsAddedThisScope.length; ++i)
    {
        char* name = *(char**)symbolsAddedThisScope.array[i];
        assert(MapRemove(&symbolTable, name));
        free(name);
    }
    ArrayClear(&symbolsAddedThisScope);
}

static char* AllocateString(const char* format, const char* insert)
{
    const size_t insertLength = strlen(insert);
    const size_t formatLength = strlen(format) - 2;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert);
    return str;
}

static char* AllocateString2(const char* format, const char* insert1, const char* insert2)
{
    const size_t insertLength = strlen(insert1) + strlen(insert2);
    const size_t formatLength = strlen(format) - 2;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert1, insert2);
    return str;
}

static Result AddSymbol(const char* name, const Type type, const int errorLineNumber, const SymbolType symbolType, const Array* funcParams)
{
    SymbolAttributes attributes;
    attributes.type = type;
    attributes.symbolType = symbolType;
    if (funcParams != NULL)
        attributes.parameterList = *funcParams;

    if (!MapAdd(&symbolTable, name, &attributes))
        return ERROR_RESULT(AllocateString("\"%s\" is already defined", name), errorLineNumber);

    const int nameLength = strlen(name);
    char* nameCopy = malloc(nameLength + 1);
    memcpy(nameCopy, name, nameLength + 1);
    ArrayAdd(&symbolsAddedThisScope, &nameCopy);

    return SUCCESS_RESULT;
}

static Result RegisterVariable(const char* name, const Type type, const int errorLineNumber)
{
    return AddSymbol(name, type, errorLineNumber, VariableSymbol, NULL);
}

static Result RegisterFunction(const char* name, const Type returnType, const int errorLineNumber, const Array* funcParams)
{
    return AddSymbol(name, returnType, errorLineNumber, FunctionSymbol, funcParams);
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

    for (MAP_ITERATE(i, &symbolTable))
    {
        const SymbolAttributes symbol = *(SymbolAttributes*)i->value;
        if (symbol.symbolType == FunctionSymbol)
            FreeArray(&symbol.parameterList);
    }
    FreeMap(&symbolTable);

    for (int i = 0; i < typeNames.length; ++i)
        free(*(char**)typeNames.array[i]);
    FreeArray(&typeNames);

    FreeArray(&symbolsAddedThisScope);
    FreeMap(&includedSections);
}

static void InitTables()
{
    symbolTable = AllocateMap(sizeof(SymbolAttributes));
    types = AllocateMap(sizeof(Type));
    typeNames = AllocateArray(sizeof(char*));
    symbolsAddedThisScope = AllocateArray(sizeof(char*));
    includedSections = AllocateMap(0);

    bool success = AddType("int", PrimitiveType);
    assert(success);

    success = AddType("float", PrimitiveType);
    assert(success);

    success = AddType("string", PrimitiveType);
    assert(success);

    success = AddType("bool", PrimitiveType);
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
                return ERROR_RESULT(AllocateString("Unexpected digit in number literal \"%s\"", insert), token.lineNumber);
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
            if (literal.text[0] != '\0')
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
                return ERROR_RESULT(AllocateString("Unknown identifier \"%s\"", name), literal.lineNumber);

            *outType = symbol->type;
            WRITE_LITERAL("var_");
            WRITE_TEXT(name);
            return SUCCESS_RESULT;
        }
        default:
            assert(0);
    }
}

static Result GenerateFunctionCallExpression(const FuncCallExpr* in, Type* outType)
{
    WRITE_LITERAL("function call");
    *outType = GetKnownType("int");
    return SUCCESS_RESULT;
}

static Result UnaryOperatorErrorResult(const Token operator, const Type type)
{
    return ERROR_RESULT_TOKEN(AllocateString("Cannot use operator \"#t\" on type \"%s\"", GetTypeName(type)),
                              operator.lineNumber, operator.type);
}

static Result GenerateExpression(const NodePtr* in, Type* outType, const bool expectingExpression);

static Result GenerateUnaryExpression(const UnaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    WRITE_TEXT(GetTokenTypeString(in->operator.type));

    Type type;
    HANDLE_ERROR(GenerateExpression(&in->expression, &type, true));

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

static Result CheckAssignmentCompatibility(const Type left, const Type right, const int lineNumber)
{
    if (left.id == right.id)
        return SUCCESS_RESULT;

    const Type intType = GetKnownType("int");
    const Type floatType = GetKnownType("float");
    if ((left.id == intType.id || left.id == floatType.id) &&
        (right.id == intType.id || right.id == floatType.id))
        return SUCCESS_RESULT;

    const char* message = AllocateString2(
        "Cannot assign value of type \"%s\" to variable of type \"%s\"",
        GetTypeName(right), GetTypeName(left));
    return ERROR_RESULT(message, lineNumber);
}

static Result GenerateBinaryExpression(const BinaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    size_t leftLength;
    Type leftType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->left, &leftType, true)),
        left, leftLength);

    size_t operatorLength;
    GET_WRITTEN_CHARS(
        WRITE_TEXT(GetTokenTypeString(in->operator.type)),
        operator, operatorLength);

    size_t rightLength;
    Type rightType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->right, &rightType, true)),
        right, rightLength);

    StreamRewind(outputText, leftLength + operatorLength + rightLength);

    bool textWritten = false;

    switch (in->operator.type)
    {
        case Equals:
        case PlusEquals:
        case MinusEquals:
        case SlashEquals:
        case AsteriskEquals:
            // assignment
        {
            bool isLvalue = false;
            if (in->left.type == LiteralExpression)
            {
                const LiteralExpr* literal = in->left.ptr;
                if (literal->value.type == Identifier)
                    isLvalue = true;
            }

            if (!isLvalue)
                return ERROR_RESULT("Cannot assign to r-value", in->operator.lineNumber);

            HANDLE_ERROR(CheckAssignmentCompatibility(leftType, rightType, in->operator.lineNumber));

            *outType = leftType;

            if (in->operator.type == Equals)
            {
                if (leftType.id == GetKnownType("int").id)
                {
                    StreamWrite(outputText, left, leftLength);
                    StreamWrite(outputText, operator, operatorLength);
                    WRITE_LITERAL("((");
                    StreamWrite(outputText, right, rightLength);
                    WRITE_LITERAL(")|0)");
                    textWritten = true;
                }

                break;
            }

            TokenType arithmeticOperator;
            switch (in->operator.type)
            {
                case PlusEquals:
                    arithmeticOperator = Plus;
                    break;
                case MinusEquals:
                    arithmeticOperator = Minus;
                    break;
                case AsteriskEquals:
                    arithmeticOperator = Asterisk;
                    break;
                case SlashEquals:
                    arithmeticOperator = Slash;
                    break;
                default: assert(0);
            }

            StreamWrite(outputText, left, leftLength);
            WRITE_TEXT(GetTokenTypeString(Equals));
            WRITE_LITERAL("((");
            StreamWrite(outputText, left, leftLength);
            WRITE_TEXT(GetTokenTypeString(arithmeticOperator));
            StreamWrite(outputText, right, rightLength);
            WRITE_LITERAL(")");
            if (leftType.id == GetKnownType("int").id)
                WRITE_LITERAL("|0");
            WRITE_LITERAL(")");

            textWritten = true;

            // no break here is intentional
        }

        case Plus:
        case Minus:
        case Slash:
        case Asterisk:
            // arithmetic
        {
            const Type intType = GetKnownType("int");
            const Type floatType = GetKnownType("float");
            if ((leftType.id != intType.id &&
                 leftType.id != floatType.id) ||
                (rightType.id != intType.id &&
                 rightType.id != floatType.id))
            {
                const char* message = AllocateString2("Operator \"#t\" can only be used on %s and %s types",
                                                      GetTypeName(intType), GetTypeName(floatType));
                return ERROR_RESULT_TOKEN(message, in->operator.lineNumber, in->operator.type);
            }

            if (leftType.id == floatType.id || rightType.id == floatType.id)
                *outType = floatType;
            else
                *outType = intType;

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
            *outType = GetKnownType("bool");
            break;
        }

        default: assert(0);
    }

    if (!textWritten)
    {
        StreamWrite(outputText, left, leftLength);
        StreamWrite(outputText, operator, operatorLength);
        StreamWrite(outputText, right, rightLength);
    }

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

static Result GenerateExpression(const NodePtr* in, Type* outType, const bool expectingExpression)
{
    switch (in->type)
    {
        case NullNode:
            if (expectingExpression)
                assert(0);
            return SUCCESS_RESULT;
        case BinaryExpression:
            return GenerateBinaryExpression(in->ptr, outType);
        case UnaryExpression:
            return GenerateUnaryExpression(in->ptr, outType);
        case LiteralExpression:
            return GenerateLiteralExpression(in->ptr, outType);
        case FunctionCallExpression:
            return GenerateFunctionCallExpression(in->ptr, outType);
        default:
            assert(0);
    }
}

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    Type type;
    const Result result = GenerateExpression(&in->expr, &type, false);
    WRITE_LITERAL(";\n");
    return result;
}

static Result GetTypeFromToken(const Token typeToken, Type* outType)
{
    if (typeToken.type == Identifier)
    {
        const char* typeName = typeToken.text;
        const Type* get = MapGet(&types, typeName);
        if (get == NULL)
        {
            return ERROR_RESULT(AllocateString("Unknown type \"%s\"", typeName), typeToken.lineNumber);
        }
        *outType = *get;
    }
    else
    {
        switch (typeToken.type)
        {
            case Int:
            case Float:
            case String:
            case Bool:
                break;

            default: assert(0);
        }

        const char* typeName = GetTokenTypeString(typeToken.type);
        const Type* get = MapGet(&types, typeName);
        assert(get != NULL);
        *outType = *get;
    }

    return SUCCESS_RESULT;
}

static Result GenerateVariableDeclaration(const VarDeclStmt* in)
{
    Type type;
    HANDLE_ERROR(GetTypeFromToken(in->type, &type));

    assert(in->identifier.type == Identifier);
    HANDLE_ERROR(RegisterVariable(in->identifier.text, type, in->identifier.lineNumber));

    NodePtr initializer;
    LiteralExpr zero = (LiteralExpr){(Token){NumberLiteral, in->identifier.lineNumber, "0"}};
    if (in->initializer.type == NullNode)
    {
        if (type.id == GetKnownType("int").id || type.id == GetKnownType("float").id)
            initializer = (NodePtr){&zero, LiteralExpression};
        else
            return ERROR_RESULT(AllocateString("Variable of type \"%s\" must be initialized", GetTypeName(type)),
                                in->identifier.lineNumber);
    }
    else
        initializer = in->initializer;

    LiteralExpr literal = {in->identifier};
    const Token equals = {Equals, in->identifier.lineNumber, NULL};
    BinaryExpr binary = {(NodePtr){&literal, LiteralExpression}, equals, initializer};
    const ExpressionStmt stmt = {(NodePtr){&binary, BinaryExpression}};
    HANDLE_ERROR(GenerateExpressionStatement(&stmt));

    return SUCCESS_RESULT;
}

static Result GenerateFunctionDeclaration(const FuncDeclStmt* in)
{
    Type returnType;
    HANDLE_ERROR(GetTypeFromToken(in->type, &returnType));

    assert(in->identifier.type == Identifier);
    Array params = AllocateArray(sizeof(FunctionParameter));
    for (int i = 0; i < in->parameters.length; ++i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == VariableDeclaration);

        const VarDeclStmt* varDecl = node->ptr;
        FunctionParameter param;
        param.name = varDecl->identifier.text;
        param.defaultValue = varDecl->initializer;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &param.type));
        ArrayAdd(&params, &param);
    }
    HANDLE_ERROR(RegisterFunction(in->identifier.text, returnType, in->identifier.lineNumber, &params));

    WRITE_LITERAL("function declaration(");
    WRITE_LITERAL(")\n");
    return SUCCESS_RESULT;
}

static Result GenerateStatement(const NodePtr* in);

static Result GenerateBlockStatement(const BlockStmt* in)
{
    WRITE_LITERAL("(0;\n");
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* stmt = in->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", ((SectionStmt*)stmt->ptr)->type.lineNumber);
        HANDLE_ERROR(GenerateStatement(stmt));
    }
    WRITE_LITERAL(");\n");

    ClearSymbolsAddedInThisScope();

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

    if (!MapAdd(&includedSections, GetTokenTypeString(in->type.type), NULL))
        return ERROR_RESULT("Duplicate sections are not allowed", in->type.lineNumber);

    WRITE_LITERAL("\n@");
    WRITE_TEXT(GetTokenTypeString(in->type.type));
    WRITE_LITERAL("\n");
    return GenerateBlockStatement(in->block.ptr);
}

static Result GenerateStatement(const NodePtr* in)
{
    switch (in->type)
    {
        case ExpressionStatement:
            return GenerateExpressionStatement(in->ptr);
        case Section:
            return GenerateSectionStatement(in->ptr);
        case VariableDeclaration:
            return GenerateVariableDeclaration(in->ptr);
        case BlockStatement:
            return GenerateBlockStatement(in->ptr);
        case FunctionDeclaration:
            return GenerateFunctionDeclaration(in->ptr);
        default:
            assert(0);
    }
}

Result GenerateProgram(const Program* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* stmt = in->statements.array[i];
        assert(stmt->type != ProgramRoot);
        HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length)
{
    InitTables();

    outputText = AllocateMemoryStream();

    const NodePtr stmt = {syntaxTree, ProgramRoot};
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
