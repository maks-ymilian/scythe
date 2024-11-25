#include "Data.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"

static int symbolCounter = 0;

static Map types;

static ScopeNode* currentScope;
static Map publicSymbolTable;

static Map* modules;

static MemoryStream* tempStream;
static MemoryStream* finalStream;
static Array streamReadPoints;

void Write(const void* buffer, const size_t length)
{
    StreamWrite(tempStream, buffer, length);
}

size_t GetStreamPosition()
{
    return StreamGetPosition(tempStream);
}

void SetStreamPosition(const size_t pos)
{
    StreamSetPosition(tempStream, pos);
}

void BeginRead()
{
    const size_t readPoint = StreamGetPosition(tempStream);
    ArrayAdd(&streamReadPoints, &readPoint);
}

Buffer EndRead()
{
    assert(streamReadPoints.length != 0);
    const size_t readPoint = *(size_t*)streamReadPoints.array[streamReadPoints.length - 1];
    ArrayRemove(&streamReadPoints, streamReadPoints.length - 1);

    const size_t length = StreamGetPosition(tempStream) - readPoint;
    Buffer buffer = StreamRewindRead(tempStream, length);

    uint8_t* new = malloc(buffer.length);
    memcpy(new, buffer.buffer, buffer.length);
    buffer.buffer = new;
    return buffer;
}

void EndReadMove(const size_t pos)
{
    assert(streamReadPoints.length != 0);
    const size_t readPoint = *(size_t*)streamReadPoints.array[streamReadPoints.length - 1];
    ArrayRemove(&streamReadPoints, streamReadPoints.length - 1);

    const size_t length = StreamGetPosition(tempStream) - readPoint;
    const Buffer buffer = StreamRewindRead(tempStream, length);

    StreamSetPosition(tempStream, readPoint);

    StreamInsert(finalStream, buffer.buffer, buffer.length, pos);
}

Buffer ReadAll()
{
    Buffer buffer = StreamGetBuffer(finalStream);
    uint8_t* new = malloc(buffer.length);
    memcpy(new, buffer.buffer, buffer.length);
    buffer.buffer = new;
    return buffer;
}

Result GetTypeFromToken(const Token typeToken, Type* outType, const bool allowVoid)
{
    if (typeToken.type == Token_Identifier)
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
        case Token_Void:
            if (!allowVoid)
                return ERROR_RESULT_TOKEN("\"#t\" is not allowed here", typeToken.lineNumber, Token_Void);
        case Token_Int:
        case Token_Float:
        case Token_String:
        case Token_Bool:
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

Result GetSymbol(const char* name, const char* module, const long errorLineNumber, SymbolData** outSymbol)
{
    *outSymbol = NULL;

    if (module != NULL)
    {
        const Map* symbolTable = MapGet(modules, module);
        if (symbolTable == NULL)
            return ERROR_RESULT(AllocateString1Str("Could not find module \"%s\"", module), errorLineNumber);

        *outSymbol = MapGet(symbolTable, name);
        if (*outSymbol == NULL)
            return ERROR_RESULT(
                AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", name, module), errorLineNumber);

        return SUCCESS_RESULT;
    }

    SymbolData* symbol = NULL;
    const ScopeNode* scope = currentScope;
    while (true)
    {
        if (scope == NULL)
            return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", name), errorLineNumber);

        symbol = MapGet(&scope->symbolTable, name);

        if (symbol != NULL)
            break;

        scope = scope->parent;
    }

    *outSymbol = symbol;
    return SUCCESS_RESULT;
}

SymbolData* GetKnownSymbol(const char* name, const char* module)
{
    SymbolData* symbol;
    ASSERT_ERROR(GetSymbol(name, module, 0, &symbol));
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

static Result AddSymbol(const char* name, SymbolData data, const bool public, const long errorLineNumber, int* outUniqueName)
{
    data.uniqueName = symbolCounter;

    if (!MapAdd(&currentScope->symbolTable, name, &data))
        return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), errorLineNumber);

    if (public)
    {
        const bool success = MapAdd(&publicSymbolTable, name, &data);
        assert(success);
    }

    if (outUniqueName != NULL) *outUniqueName = symbolCounter;
    symbolCounter++;
    return SUCCESS_RESULT;
}

Result RegisterVariable(const Token identifier, const Type type, const Map* symbolTable, const bool public, int* outUniqueName)
{
    VariableSymbolData data;
    data.type = type;
    if (symbolTable != NULL)
    {
        assert(type.metaType == MetaType_Struct);
        data.symbolTable = *symbolTable;
    }

    const SymbolData symbolData = {.symbolType = SymbolType_Variable, .variableData = data, .uniqueName = -1};
    return AddSymbol(identifier.text, symbolData, public, identifier.lineNumber, outUniqueName);
}

Result RegisterFunction(const Token identifier, const Type returnType, const Array* funcParams, const bool public, int* outUniqueName)
{
    FunctionSymbolData data;
    data.parameters = *funcParams;
    data.returnType = returnType;

    const SymbolData symbolData = {.symbolType = SymbolType_Function, .functionData = data, .uniqueName = -1};
    return AddSymbol(identifier.text, symbolData, public, identifier.lineNumber, outUniqueName);
}

Result RegisterStruct(const Token identifier, const Array* members, const bool public, int* outUniqueName)
{
    StructSymbolData data;
    data.members = members;

    const SymbolData symbolData = {.symbolType = SymbolType_Struct, .structData = data, .uniqueName = -1};

    if (!AddType(identifier.text, MetaType_Struct))
        return ERROR_RESULT(AllocateString1Str("Type \"%s\" is already defined", identifier.text),
                            identifier.lineNumber);

    return AddSymbol(identifier.text, symbolData, public, identifier.lineNumber, outUniqueName);
}

Map GetPublicSymbolTable()
{
    return publicSymbolTable;
}

bool IsModuleName(const char* name)
{
    return MapGet(modules, name) != NULL;
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
        if (symbol.symbolType == SymbolType_Function)
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

void InitResources(Map* _modules)
{
    modules = _modules;

    streamReadPoints = AllocateArray(sizeof(size_t));
    tempStream = AllocateMemoryStream();
    finalStream = AllocateMemoryStream();

    const char section[] = "@init\n";
    StreamWrite(finalStream, section, sizeof(section) - 1);

    ScopeNode* globalScope = AllocateScopeNode();
    currentScope = globalScope;

    publicSymbolTable = AllocateMap(sizeof(SymbolData));

    types = AllocateMap(sizeof(Type));

    bool success = AddType("void", MetaType_Primitive);
    assert(success);

    success = AddType("int", MetaType_Primitive);
    assert(success);

    success = AddType("float", MetaType_Primitive);
    assert(success);

    success = AddType("string", MetaType_Primitive);
    assert(success);

    success = AddType("bool", MetaType_Primitive);
    assert(success);
}

void FreeResources()
{
    FreeMap(&types);

    while (currentScope != NULL)
        PopScope(NULL);

    FreeArray(&streamReadPoints);
    FreeMemoryStream(tempStream, true);
    FreeMemoryStream(finalStream, true);
}
