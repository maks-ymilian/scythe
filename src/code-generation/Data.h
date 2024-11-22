#pragma once

#include <stdlib.h>

#include "SyntaxTree.h"
#include "data-structures/Map.h"
#include "data-structures/MemoryStream.h"
#include "Result.h"

typedef enum { PrimitiveType, StructType, EnumType } MetaType;

typedef struct
{
    const char* name;
    MetaType metaType;
    uint32_t id;
} Type;

typedef struct
{
    Type type;
    char* name;
    NodePtr defaultValue;
} FunctionParameter;

typedef struct
{
    Type type;
    Map symbolTable;
} VariableSymbolData;

typedef struct
{
    Type returnType;
    Array parameters;
} FunctionSymbolData;

typedef struct
{
    const Array* members;
} StructSymbolData;

typedef enum { VariableSymbol, FunctionSymbol, StructSymbol } SymbolType;

typedef struct
{
    int uniqueName;
    SymbolType symbolType;

    union
    {
        VariableSymbolData variableData;
        FunctionSymbolData functionData;
        StructSymbolData structData;
    };
} SymbolData;

typedef struct ScopeNode ScopeNode;

struct ScopeNode
{
    ScopeNode* parent;
    Map symbolTable;
    bool isFunction;
    Type functionReturnType;
};

typedef enum { MainStream, FunctionsStream, ExpressionStream } StreamType;

void Write(const void* buffer, size_t length);
size_t GetStreamPosition();
void SetStreamPosition(size_t pos);
void BeginRead();
Buffer EndRead();
void EndReadMove(size_t pos);
Buffer ReadAll();

Result GetTypeFromToken(Token typeToken, Type* outType, bool allowVoid);
Type GetKnownType(const char* name);

Result GetSymbol(const char* name, long errorLineNumber, SymbolData** outSymbol);
SymbolData* GetKnownSymbol(const char* name);
Result RegisterVariable(Token identifier, Type type, const Map* symbolTable, int* outUniqueName);
Result RegisterFunction(Token identifier, Type returnType, const Array* funcParams, int* outUniqueName);
Result RegisterStruct(Token identifier, const Array* members, int* outUniqueName);

void PushScope();
void PopScope(Map* outSymbolTable);
ScopeNode* GetFunctionScope();
ScopeNode* GetCurrentScope();

void InitResources();
void FreeResources();
