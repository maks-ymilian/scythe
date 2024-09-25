#include "Data.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static Map types;

static ScopeNode* currentScope;

static MemoryStream* mainStream;
static MemoryStream* functionsStream;
static MemoryStream* currentStream;

static size_t streamUndoPoint;
static size_t streamReadPoint;

void Write(const void* buffer, const size_t length)
{
    StreamWrite(currentStream, buffer, length);
}

Buffer CombineStreams()
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

void SetCurrentStream(const StreamType stream)
{
    switch (stream)
    {
        case MainStream:
            currentStream = mainStream;
            break;
        case FunctionsStream:
            currentStream = functionsStream;
            break;
        default:
            assert(0);
    }
}

void BeginUndo()
{
    streamUndoPoint = StreamGetPosition(currentStream);
}

void EndUndo()
{
    StreamSetPosition(currentStream, streamUndoPoint);
}

void BeginRead()
{
    streamReadPoint = StreamGetPosition(currentStream);
}

Buffer EndRead()
{
    const size_t length = StreamGetPosition(currentStream) - streamReadPoint;
    Buffer buffer = StreamRewindRead(currentStream, length);
    uint8_t* new = malloc(buffer.length);
    memcpy(new, buffer.buffer, buffer.length);
    buffer.buffer = new;
    return buffer;
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

char* AllocateString1Str(const char* format, const char* insert)
{
    const size_t insertLength = strlen(insert);
    const size_t formatLength = strlen(format) - 2;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert);
    return str;
}

char* AllocateString2Str(const char* format, const char* insert1, const char* insert2)
{
    const size_t insertLength = strlen(insert1) + strlen(insert2);
    const size_t formatLength = strlen(format) - 4;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert1, insert2);
    return str;
}

char* AllocateString2Int(const char* format, const int insert1, const int insert2)
{
    const size_t insertLength = IntCharCount(insert1) + IntCharCount(insert2);
    const size_t formatLength = strlen(format) - 4;
    const size_t bufferLength = insertLength + formatLength + 1;
    char* str = malloc(bufferLength);
    snprintf(str, bufferLength, format, insert1, insert2);
    return str;
}

Result GetTypeFromToken(const Token typeToken, Type* outType, const bool allowVoid)
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

Type GetKnownType(const char* name)
{
    const Type* type = MapGet(&types, name);
    assert(type != NULL);
    return *type;
}

Result GetSymbol(const Token identifier, SymbolData** outSymbol)
{
    SymbolData* symbol;
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

SymbolData* GetKnownSymbol(const Token identifier)
{
    SymbolData* symbol;
    const Result result = GetSymbol(identifier, &symbol);
    assert(result.success);
    return symbol;
}

static bool AddType(const char* name, const MetaType metaType)
{
    Type type;
    type.name = name;
    type.id = types.elementCount;
    type.metaType = metaType;

    const bool success = MapAdd(&types, name, &type);
    if (!success)
        return false;

    return true;
}

static Result AddSymbol(const char* name, const SymbolData data, const long errorLineNumber)
{
    if (!MapAdd(&currentScope->symbolTable, name, &data))
        return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), errorLineNumber);

    return SUCCESS_RESULT;
}

Result RegisterVariable(const Token identifier, const Type type, const Map* symbolTable)
{
    VariableSymbolData data;
    data.type = type;
    if (symbolTable != NULL)
    {
        assert(type.metaType == StructType);
        data.symbolTable = *symbolTable;
    }

    SymbolData symbolData;
    symbolData.symbolType = VariableSymbol;
    symbolData.variableData = data;
    return AddSymbol(identifier.text, symbolData, identifier.lineNumber);
}

Result RegisterFunction(const Token identifier, const Type returnType, const Array* funcParams)
{
    FunctionSymbolData data;
    data.parameters = *funcParams;
    data.returnType = returnType;

    SymbolData symbolData;
    symbolData.symbolType = FunctionSymbol;
    symbolData.functionData = data;
    return AddSymbol(identifier.text, symbolData, identifier.lineNumber);
}

Result RegisterStruct(const Token identifier, const Array* members)
{
    StructSymbolData data;
    data.members = members;

    SymbolData symbolData;
    symbolData.symbolType = StructSymbol;
    symbolData.structData = data;

    AddType(identifier.text, StructType);
    return AddSymbol(identifier.text, symbolData, identifier.lineNumber);
}

static ScopeNode* AllocateScopeNode()
{
    ScopeNode* scopeNode = malloc(sizeof(ScopeNode));
    scopeNode->parent = NULL;
    scopeNode->isFunction = false;
    scopeNode->symbolTable = AllocateMap(sizeof(SymbolData));
    return scopeNode;
}

void PushScope()
{
    ScopeNode* newScope = AllocateScopeNode();
    newScope->parent = currentScope;
    currentScope = newScope;
}

void PopScope(Map* outSymbolTable)
{
    for (MAP_ITERATE(i, &currentScope->symbolTable))
    {
        const SymbolData symbol = *(SymbolData*)i->value;
        if (symbol.symbolType == FunctionSymbol)
            FreeArray(&symbol.functionData.parameters);
    }

    ScopeNode* parent = currentScope->parent;

    if (outSymbolTable == NULL)
        FreeMap(&currentScope->symbolTable);
    else
        *outSymbolTable = currentScope->symbolTable;

    free(currentScope);
    currentScope = parent;
}

ScopeNode* GetFunctionScope()
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

ScopeNode* GetCurrentScope()
{
    return currentScope;
}

void InitResources()
{
    ScopeNode* globalScope = AllocateScopeNode();
    currentScope = globalScope;

    mainStream = AllocateMemoryStream();
    functionsStream = AllocateMemoryStream();
    SetCurrentStream(MainStream);

    types = AllocateMap(sizeof(Type));

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

void FreeResources()
{
    FreeMap(&types);

    while (currentScope != NULL)
        PopScope(NULL);

    FreeMemoryStream(mainStream, true);
    FreeMemoryStream(functionsStream, true);
}
