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

#define WRITE_LITERAL(text) StreamWrite(currentStream, text, sizeof(text) - 1)
#define WRITE_TEXT(text) StreamWrite(currentStream, text, strlen(text));

#define GET_WRITTEN_CHARS(code, arrayName, charsLength)\
const size_t  arrayName##_pos = StreamGetPosition(currentStream);\
code;\
const size_t  arrayName##_length = StreamGetPosition(currentStream) -  arrayName##_pos;\
char* arrayName##_chars = (char*)StreamRewindRead(currentStream,  arrayName##_length).buffer;\
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

typedef struct ScopeNode ScopeNode;

struct ScopeNode
{
    ScopeNode* parent;
    Map symbolTable;
    bool isFunction;
    Type functionReturnType;
};

static ScopeNode* currentScope;

static Map includedSections;

static MemoryStream* mainStream;
static MemoryStream* functionsStream;
static MemoryStream* currentStream;

static ScopeNode* AllocateScopeNode()
{
    ScopeNode* scopeNode = malloc(sizeof(ScopeNode));
    scopeNode->parent = NULL;
    scopeNode->isFunction = false;
    scopeNode->symbolTable = AllocateMap(sizeof(SymbolAttributes));
    return scopeNode;
}

static void PopScope()
{
    for (MAP_ITERATE(i, &currentScope->symbolTable))
    {
        const SymbolAttributes symbol = *(SymbolAttributes*)i->value;
        if (symbol.symbolType == FunctionSymbol)
            FreeArray(&symbol.parameterList);
    }

    ScopeNode* parent = currentScope->parent;
    FreeMap(&currentScope->symbolTable);
    free(currentScope);
    currentScope = parent;
}

static void PushScope()
{
    ScopeNode* newScope = AllocateScopeNode();
    newScope->parent = currentScope;
    currentScope = newScope;
}

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

static char* AllocateString1Str(const char* format, const char* insert)
{
    const size_t insertLength = strlen(insert);
    const size_t formatLength = strlen(format) - 2;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert);
    return str;
}

static char* AllocateString2Str(const char* format, const char* insert1, const char* insert2)
{
    const size_t insertLength = strlen(insert1) + strlen(insert2);
    const size_t formatLength = strlen(format) - 4;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert1, insert2);
    return str;
}

static int IntCharCount(int number)
{
    int minusSign = 0;
    if (number < 0)
    {
        number = abs(number);
        minusSign = 1;
    }
    if (number < 10) return 1 + minusSign;
    if (number < 100) return 2 + minusSign;
    if (number < 1000) return 3 + minusSign;
    if (number < 10000) return 4 + minusSign;
    if (number < 100000) return 5 + minusSign;
    if (number < 1000000) return 6 + minusSign;
    if (number < 10000000) return 7 + minusSign;
    if (number < 100000000) return 8 + minusSign;
    if (number < 1000000000) return 9 + minusSign;
    return 10 + minusSign;
}

static char* AllocateString2Int(const char* format, const int insert1, const int insert2)
{
    const size_t insertLength = IntCharCount(insert1) + IntCharCount(insert2);
    const size_t formatLength = strlen(format) - 4;
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

    if (!MapAdd(&currentScope->symbolTable, name, &attributes))
        return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), errorLineNumber);

    return SUCCESS_RESULT;
}

static Result RegisterVariable(const Token identifier, const Type type)
{
    return AddSymbol(identifier.text, type, identifier.lineNumber, VariableSymbol, NULL);
}

static Result RegisterFunction(const Token identifier, const Type returnType, const Array* funcParams)
{
    return AddSymbol(identifier.text, returnType, identifier.lineNumber, FunctionSymbol, funcParams);
}

static void SetCurrentStream(MemoryStream* stream) { currentStream = stream; }

static Buffer CombineStreams()
{
    MemoryStream* stream = AllocateMemoryStream();

    const Buffer functions = StreamRewindRead(functionsStream, 0);
    StreamWrite(stream, functions.buffer, functions.length);

    const Buffer sections = StreamRewindRead(mainStream, 0);
    StreamWrite(stream, sections.buffer, sections.length);

    const Buffer out = StreamRewindRead(stream, 0);
    FreeMemoryStream(stream, false);
    return out;
}

static void FreeResources()
{
    FreeMap(&types);

    while (currentScope != NULL)
        PopScope();

    for (int i = 0; i < typeNames.length; ++i)
        free(*(char**)typeNames.array[i]);
    FreeArray(&typeNames);

    FreeMap(&includedSections);

    FreeMemoryStream(mainStream, true);
    FreeMemoryStream(functionsStream, true);
}

static void InitResources()
{
    ScopeNode* globalScope = AllocateScopeNode();
    currentScope = globalScope;

    mainStream = AllocateMemoryStream();
    functionsStream = AllocateMemoryStream();
    SetCurrentStream(mainStream);

    types = AllocateMap(sizeof(Type));
    typeNames = AllocateArray(sizeof(char*));
    includedSections = AllocateMap(0);

    bool success = AddType("void", PrimitiveType);
    assert(success);

    success = AddType("int", PrimitiveType);
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
                return ERROR_RESULT(AllocateString1Str("Unexpected digit in number literal \"%s\"", insert), token.lineNumber);
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

static Result GetSymbol(const Token identifier, SymbolAttributes** outSymbol)
{
    SymbolAttributes* symbol;
    const ScopeNode* scope = currentScope;

    while (true)
    {
        if (scope == NULL)
            return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", identifier.text), identifier.lineNumber);

        symbol = MapGet(&scope->symbolTable, identifier.text);

        if (symbol != NULL)
            break;

        scope = scope->parent;
    }

    *outSymbol = symbol;
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
            SymbolAttributes* symbol;
            HANDLE_ERROR(GetSymbol(literal, &symbol));
            if (symbol->symbolType != VariableSymbol)
                return ERROR_RESULT("Identifier must be the name of a variable", literal.lineNumber);

            *outType = symbol->type;
            WRITE_LITERAL("var_");
            WRITE_TEXT(literal.text);
            return SUCCESS_RESULT;
        }
        case True:
        {
            WRITE_LITERAL("1");
            *outType = GetKnownType("bool");
            return SUCCESS_RESULT;
        }
        case False:
        {
            WRITE_LITERAL("0");
            *outType = GetKnownType("bool");
            return SUCCESS_RESULT;
        }
        default:
            assert(0);
    }
}

static Result CheckAssignmentCompatibility(const Type left, const Type right, const char* errorMessage, const long lineNumber)
{
    if (left.id == right.id)
        return SUCCESS_RESULT;

    const Type intType = GetKnownType("int");
    const Type floatType = GetKnownType("float");
    if ((left.id == intType.id || left.id == floatType.id) &&
        (right.id == intType.id || right.id == floatType.id))
        return SUCCESS_RESULT;

    const char* message = AllocateString2Str(
        errorMessage,
        GetTypeName(right), GetTypeName(left));
    return ERROR_RESULT(message, lineNumber);
}

static Result GetTypeFromToken(const Token typeToken, Type* outType, const bool allowVoid)
{
    if (typeToken.type == Identifier)
    {
        const char* typeName = typeToken.text;
        const Type* get = MapGet(&types, typeName);
        if (get == NULL)
        {
            return ERROR_RESULT(AllocateString1Str("Unknown type \"%s\"", typeName), typeToken.lineNumber);
        }
        *outType = *get;
    }
    else
    {
        switch (typeToken.type)
        {
            case Void:
                if (!allowVoid)
                    return ERROR_RESULT_TOKEN("\"#t\" is not allowed here", typeToken.lineNumber, Void);
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

static int Max(const int a, const int b)
{
    if (a > b) return a;
    return b;
};

static Result GenerateExpression(const NodePtr* in, Type* outType, bool expectingExpression, bool convertToInteger);

static Result GenerateFunctionParameter(const Type paramType, const NodePtr* expr, const long lineNumber)
{
    Type exprType;
    HANDLE_ERROR(GenerateExpression(expr, &exprType, true, paramType.id == GetKnownType("int").id));
    HANDLE_ERROR(CheckAssignmentCompatibility(paramType, exprType,
        "Cannot assign value of type \"%s\" to function parameter of type \"%s\"", lineNumber));

    return SUCCESS_RESULT;
}

static Result GenerateFunctionCallExpression(const FuncCallExpr* in, Type* outType)
{
    SymbolAttributes* symbol;
    HANDLE_ERROR(GetSymbol(in->identifier, &symbol));
    if (symbol->symbolType != FunctionSymbol)
        return ERROR_RESULT("Identifier must be the name of a function", in->identifier.lineNumber);

    WRITE_LITERAL("func_");
    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("(");

    for (int i = 0; i < Max(symbol->parameterList.length, in->parameters.length); ++i)
    {
        const FunctionParameter* funcParam = NULL;
        if (i < symbol->parameterList.length)
            funcParam = symbol->parameterList.array[i];

        const NodePtr* callExpr = NULL;
        if (i < in->parameters.length)
            callExpr = (NodePtr*)in->parameters.array[i];

        if (funcParam == NULL && callExpr != NULL ||
            funcParam != NULL && funcParam->defaultValue.ptr == NULL && callExpr == NULL)
            return ERROR_RESULT(AllocateString2Int("Function has %d parameter(s), but is called with %d argument(s)",
                                    symbol->parameterList.length, in->parameters.length), in->identifier.lineNumber);

        const NodePtr* writeExpr;
        if (callExpr != NULL)
            writeExpr = callExpr;
        else
            writeExpr = &funcParam->defaultValue;

        HANDLE_ERROR(GenerateFunctionParameter(funcParam->type, writeExpr, in->identifier.lineNumber));
        if (i != symbol->parameterList.length - 1)
            WRITE_LITERAL(",");
    }

    WRITE_LITERAL(")");

    *outType = symbol->type;
    return SUCCESS_RESULT;
}

static Result UnaryOperatorErrorResult(const Token operator, const Type type)
{
    return ERROR_RESULT_TOKEN(AllocateString1Str("Cannot use operator \"#t\" on type \"%s\"", GetTypeName(type)),
                              operator.lineNumber, operator.type);
}

static Result GenerateUnaryExpression(const UnaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    WRITE_TEXT(GetTokenTypeString(in->operator.type));

    Type type;
    HANDLE_ERROR(GenerateExpression(&in->expression, &type, true, false));

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

    size_t leftLength;
    Type leftType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->left, &leftType, true, false)),
        left, leftLength);

    size_t operatorLength;
    GET_WRITTEN_CHARS(
        WRITE_TEXT(GetTokenTypeString(in->operator.type)),
        operator, operatorLength);

    size_t rightLength;
    Type rightType;
    GET_WRITTEN_CHARS(
        HANDLE_ERROR(GenerateExpression(&in->right, &rightType, true, false)),
        right, rightLength);

    StreamRewind(currentStream, leftLength + operatorLength + rightLength);

    const Result operatorTypeError = ERROR_RESULT_TOKEN(
        AllocateString2Str("Cannot use operator \"#t\" on types \"%s\" and \"%s\"",
            GetTypeName(leftType), GetTypeName(rightType)),
        in->operator.lineNumber, in->operator.type);

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

            HANDLE_ERROR(CheckAssignmentCompatibility(leftType, rightType,
                "Cannot assign value of type \"%s\" to variable of type \"%s\"", in->operator.lineNumber));

            *outType = leftType;

            if (leftType.id == GetKnownType("int").id)
            {
                StreamWrite(currentStream, left, leftLength);
                StreamWrite(currentStream, operator, operatorLength);
                WRITE_LITERAL("(");
                StreamWrite(currentStream, right, rightLength);
                WRITE_LITERAL("|0)");
                textWritten = true;
            }

            if (in->operator.type == Equals)
                break;

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

            StreamWrite(currentStream, left, leftLength);
            WRITE_TEXT(GetTokenTypeString(Equals));
            WRITE_LITERAL("((");
            StreamWrite(currentStream, left, leftLength);
            WRITE_TEXT(GetTokenTypeString(arithmeticOperator));
            StreamWrite(currentStream, right, rightLength);
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
                return operatorTypeError;

            if (leftType.id == floatType.id || rightType.id == floatType.id)
                *outType = floatType;
            else
                *outType = intType;

            break;
        }

        case EqualsEquals:
        case ExclamationEquals:
            // equality
        {
            if (!CheckAssignmentCompatibility(leftType, rightType, "you should not be reading this", 69420).success)
                return operatorTypeError;

            *outType = GetKnownType("bool");
            break;
        }
        case LeftAngleBracket:
        case RightAngleBracket:
        case LeftAngleEquals:
        case RightAngleEquals:
            // number comparison
        {
            const Type intType = GetKnownType("int");
            const Type floatType = GetKnownType("float");
            if ((leftType.id != intType.id &&
                 leftType.id != floatType.id) ||
                (rightType.id != intType.id &&
                 rightType.id != floatType.id))
                return operatorTypeError;

            *outType = GetKnownType("bool");
            break;
        }
        case AmpersandAmpersand:
        case PipePipe:
            // boolean operators
        {
            const Type boolType = GetKnownType("bool");
            if (leftType.id != boolType.id || rightType.id != boolType.id)
                return operatorTypeError;

            *outType = GetKnownType("bool");
            break;
        }

        default: assert(0);
    }

    if (!textWritten)
    {
        StreamWrite(currentStream, left, leftLength);
        StreamWrite(currentStream, operator, operatorLength);
        StreamWrite(currentStream, right, rightLength);
    }

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

static Result GenerateExpression(const NodePtr* in, Type* outType, const bool expectingExpression, const bool convertToInteger)
{
    if (in->type == NullNode)
    {
        if (expectingExpression)
            assert(0);
        return SUCCESS_RESULT;
    }

    if (convertToInteger)
        WRITE_LITERAL("(");

    Result result;

    switch (in->type)
    {
        case BinaryExpression:
            result = GenerateBinaryExpression(in->ptr, outType);
            break;
        case UnaryExpression:
            result = GenerateUnaryExpression(in->ptr, outType);
            break;
        case LiteralExpression:
            result = GenerateLiteralExpression(in->ptr, outType);
            break;
        case FunctionCallExpression:
            result = GenerateFunctionCallExpression(in->ptr, outType);
            break;
        default:
            assert(0);
    }

    if (convertToInteger)
    {
        if (outType->id == GetKnownType("float").id)
            WRITE_LITERAL("|0");
        WRITE_LITERAL(")");
    }

    return result;
}

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    Type type;
    const Result result = GenerateExpression(&in->expr, &type, false, false);
    WRITE_LITERAL(";\n");
    return result;
}

static ScopeNode* GetFunctionScope()
{
    ScopeNode* scope = currentScope;
    while (scope != NULL)
    {
        if (scope->isFunction)
            break;

        scope = scope->parent;
    }
    return scope;
}

static Result GenerateReturnStatement(const ReturnStmt* in)
{
    const ScopeNode* functionScope = GetFunctionScope();
    if (functionScope == NULL)
        return ERROR_RESULT("Return statement must be inside a function", in->returnToken.lineNumber);

    WRITE_LITERAL("(");
    WRITE_LITERAL("__hasReturned = 1;");

    Result result = SUCCESS_RESULT;
    if (in->expr.type != NullNode)
    {
        WRITE_LITERAL("__returnValue =");
        Type exprType;
        result = GenerateExpression(&in->expr, &exprType, false,
                                                 functionScope->functionReturnType.id == GetKnownType("int").id);
    }
    WRITE_LITERAL(");\n");

    return result;
}

static Result GenerateVariableDeclaration(const VarDeclStmt* in)
{
    Type type;
    HANDLE_ERROR(GetTypeFromToken(in->type, &type, false));

    assert(in->identifier.type == Identifier);
    HANDLE_ERROR(RegisterVariable(in->identifier, type));

    NodePtr initializer;
    LiteralExpr zero = (LiteralExpr){(Token){NumberLiteral, in->identifier.lineNumber, "0"}};
    if (in->initializer.type == NullNode)
    {
        if (type.id == GetKnownType("int").id || type.id == GetKnownType("float").id)
            initializer = (NodePtr){&zero, LiteralExpression};
        else
            return ERROR_RESULT(AllocateString1Str("Variable of type \"%s\" must be initialized", GetTypeName(type)),
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

typedef enum { NoReturn, IsReturn, ContainsReturn } ReturnThing;

static Result CheckReturnStatement(const ReturnStmt* returnStmt, const Type returnType)
{
    const size_t streamPos = StreamGetPosition(currentStream);

    const bool isVoid = returnType.id == GetKnownType("void").id;
    if (returnStmt->expr.type == NullNode)
    {
        if (!isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" must return a value", GetTypeName(returnType)),
                returnStmt->returnToken.lineNumber);
    }
    else
    {
        if (isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" cannot return a value", GetTypeName(returnType)),
                returnStmt->returnToken.lineNumber);

        Type exprType;
        HANDLE_ERROR(GenerateExpression(&returnStmt->expr, &exprType, true, false));
        HANDLE_ERROR(CheckAssignmentCompatibility(returnType, exprType,
            "Cannot convert type \"%s\" to return type \"%s\"",
            returnStmt->returnToken.lineNumber));
    }

    StreamSetPosition(currentStream, streamPos);

    return SUCCESS_RESULT;
}

static Result CheckReturnStatements(const NodePtr node, const Type returnType)
{
    switch (node.type)
    {
        case ReturnStatement:
            HANDLE_ERROR(CheckReturnStatement(node.ptr, returnType));
            return SUCCESS_RESULT;
        case IfStatement:
        {
            const IfStmt* ifStmt = node.ptr;
            HANDLE_ERROR(CheckReturnStatements(ifStmt->trueStmt, returnType));
            HANDLE_ERROR(CheckReturnStatements(ifStmt->falseStmt, returnType));
            return SUCCESS_RESULT;
        }
        case BlockStatement:
        {
            const BlockStmt* block = node.ptr;
            for (int i = 0; i < block->statements.length; ++i)
            {
                const NodePtr* stmt = block->statements.array[i];
                HANDLE_ERROR(CheckReturnStatements(*stmt, returnType));
            }
            return SUCCESS_RESULT;
        }
        case ExpressionStatement:
        case VariableDeclaration:
        case FunctionDeclaration:
        case NullNode:
            return SUCCESS_RESULT;
        default: assert(0);
    }
}

static bool ControlPathsReturn(const NodePtr node, const bool allPathsMustReturn)
{
    switch (node.type)
    {
        case ReturnStatement:
            return true;
        case IfStatement:
        {
            const IfStmt* ifStmt = node.ptr;
            const bool trueReturns = ControlPathsReturn(ifStmt->trueStmt, allPathsMustReturn);
            const bool falseReturns = ControlPathsReturn(ifStmt->falseStmt, allPathsMustReturn);

            if (allPathsMustReturn)
                return trueReturns && falseReturns;
            return trueReturns || falseReturns;
        }
        case BlockStatement:
        {
            const BlockStmt* block = node.ptr;
            for (int i = 0; i < block->statements.length; ++i)
            {
                const NodePtr* statement = block->statements.array[i];
                if (ControlPathsReturn(*statement, allPathsMustReturn))
                    return true;
            }
            return false;
        }
        case ExpressionStatement:
        case VariableDeclaration:
        case FunctionDeclaration:
        case NullNode:
            return false;
        default: assert(0);
    }
}

static Result GenerateStatement(const NodePtr* in);

static Result GenerateFunctionBlock(const BlockStmt* in, const bool topLevel)
{
    WRITE_LITERAL("(0;\n");

    if (topLevel)
    {
        WRITE_LITERAL("__hasReturned = 0;\n");
        WRITE_LITERAL("__returnValue = 0;\n");
    }

    long returnIfStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(GenerateStatement(node));

        if (ControlPathsReturn(*node, false))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            WRITE_LITERAL("(!__hasReturned) ? (\n");
            returnIfStatementCount++;
        }
    }
    for (int i = 0; i < returnIfStatementCount; ++i)
        WRITE_LITERAL(");");

    if (topLevel)
        WRITE_LITERAL("__returnValue;\n");

    WRITE_LITERAL(");\n");

    return SUCCESS_RESULT;
}

static Result GenerateFunctionDeclaration(const FuncDeclStmt* in)
{
    SetCurrentStream(functionsStream);
    PushScope();

    Type returnType;
    HANDLE_ERROR(GetTypeFromToken(in->type, &returnType, true));

    currentScope->isFunction = true;
    currentScope->functionReturnType = returnType;

    assert(in->block.type == BlockStatement);

    HANDLE_ERROR(CheckReturnStatements(in->block, returnType));
    if (!ControlPathsReturn(in->block, true) && returnType.id != GetKnownType("void").id)
        return ERROR_RESULT("Not all control paths return a value", in->identifier.lineNumber);

    assert(in->identifier.type == Identifier);

    WRITE_LITERAL("function ");
    WRITE_LITERAL("func_");
    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("(");

    if (in->parameters.length > 40)
        return ERROR_RESULT("Functions can only have up to 40 parameters", in->identifier.lineNumber);

    Array params = AllocateArray(sizeof(FunctionParameter));
    bool hasOptionalParams = false;
    for (int i = 0; i < in->parameters.length; ++i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == VariableDeclaration);

        const VarDeclStmt* varDecl = node->ptr;

        if (varDecl->initializer.ptr != NULL)
            hasOptionalParams = true;
        else if (hasOptionalParams == true)
            return ERROR_RESULT("Optional parameters must be at end of parameter list", in->identifier.lineNumber);

        FunctionParameter param;
        param.name = varDecl->identifier.text;
        param.defaultValue = varDecl->initializer;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &param.type, false));
        ArrayAdd(&params, &param);

        WRITE_LITERAL("var_");
        WRITE_TEXT(varDecl->identifier.text);
        if (i < in->parameters.length - 1)
            WRITE_LITERAL(",");

        HANDLE_ERROR(RegisterVariable(varDecl->identifier, param.type));

        if (param.defaultValue.ptr != NULL)
        {
            const size_t beforePos = StreamGetPosition(currentStream);
            HANDLE_ERROR(GenerateFunctionParameter(param.type, &param.defaultValue, varDecl->identifier.lineNumber));
            StreamSetPosition(currentStream, beforePos);
        }
    }

    WRITE_LITERAL(")\n");
    HANDLE_ERROR(GenerateFunctionBlock(in->block.ptr, true));

    PopScope();
    SetCurrentStream(mainStream);

    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params));

    return SUCCESS_RESULT;
}

static Result GenerateStructDeclaration(const StructDeclStmt* in)
{
    return SUCCESS_RESULT;
}

static Result GenerateBlockStatement(const BlockStmt* in)
{
    PushScope();

    WRITE_LITERAL("(0;\n");
    for (int i = 0; i < in->statements.length; ++i)
        HANDLE_ERROR(GenerateStatement(in->statements.array[i]));
    WRITE_LITERAL(");\n");

    PopScope();

    return SUCCESS_RESULT;
}

static Result GenerateIfStatement(const IfStmt* in)
{
    Type exprType;
    HANDLE_ERROR(GenerateExpression(&in->expr, &exprType, true, false));
    const Type boolType = GetKnownType("bool");
    if (exprType.id != boolType.id)
        return ERROR_RESULT(
            AllocateString1Str("Expression inside if statement must be evaluate to a \"%s\"", GetTypeName(boolType)),
            in->ifToken.lineNumber);

    WRITE_LITERAL("?(\n");
    HANDLE_ERROR(GenerateStatement(&in->trueStmt));
    WRITE_LITERAL(")");
    if (in->falseStmt.type != NullNode)
    {
        WRITE_LITERAL(":(\n");
        HANDLE_ERROR(GenerateStatement(&in->falseStmt));
        WRITE_LITERAL(")");
    }
    WRITE_LITERAL(";\n");

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
        case ReturnStatement:
            return GenerateReturnStatement(in->ptr);
        case Section:
            return GenerateSectionStatement(in->ptr);
        case VariableDeclaration:
            return GenerateVariableDeclaration(in->ptr);
        case BlockStatement:
        {
            const ScopeNode* functionScope = GetFunctionScope();
            return functionScope == NULL ? GenerateBlockStatement(in->ptr) : GenerateFunctionBlock(in->ptr, false);
        }
        case FunctionDeclaration:
            return GenerateFunctionDeclaration(in->ptr);
        case StructDeclaration:
            return GenerateStructDeclaration(in->ptr);
        case IfStatement:
            return GenerateIfStatement(in->ptr);
        default:
            assert(0);
    }
}

Result GenerateProgram(const Program* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* stmt = in->statements.array[i];
        assert(stmt->type != RootNode);
        HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length)
{
    InitResources();

    SetCurrentStream(functionsStream);
    WRITE_LITERAL("@init\n");
    SetCurrentStream(mainStream);

    const Result result = GenerateProgram(syntaxTree);

    WRITE_LITERAL("\n");
    const Buffer outputBuffer = CombineStreams();

    FreeSyntaxTree((NodePtr){syntaxTree, RootNode});
    FreeResources();

    if (result.hasError)
        return result;

    *outputCode = outputBuffer.buffer;
    *length = outputBuffer.length;

    return SUCCESS_RESULT;
}
