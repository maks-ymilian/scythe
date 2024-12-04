#pragma once

#include <stdlib.h>

#include "SyntaxTree.h"
#include "data-structures/Map.h"
#include "data-structures/MemoryStream.h"
#include "CodeGenerator.h"
#include "Result.h"

typedef enum { MetaType_Primitive, MetaType_Struct, MetaType_Enum } MetaType;

typedef struct
{
    const char* name;
    MetaType metaType;
    int id;
    bool public;
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
    bool external;
    char* externalName;
} VariableSymbolData;

typedef struct
{
    Type returnType;
    Array parameters;
    bool external;
    char* externalName;
} FunctionSymbolData;

typedef struct
{
    const Array* members;
} StructSymbolData;

typedef enum { SymbolType_Variable, SymbolType_Function, SymbolType_Struct } SymbolType;

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

typedef enum
{
    ScopeType_Normal,
    ScopeType_Function,
    ScopeType_Loop,
} ScopeType;

typedef struct ScopeNode ScopeNode;

struct ScopeNode
{
    ScopeNode* parent;
    Map symbolTable;
    ScopeType scopeType;
    Type functionReturnType;
};

extern Module module;

void Write(const void* buffer, size_t length);
size_t GetStreamPosition();
void SetStreamPosition(size_t pos);
void BeginRead();
Buffer EndRead();
void EndReadMove(size_t pos);
Buffer ReadAll();

Result GetTypeFromToken(Token typeToken, Type* outType, bool allowVoid);
Type GetKnownType(const char* name);

Result GetSymbol(const char* name, bool searchAllModules, const char* moduleName, long errorLineNumber, SymbolData** outSymbol);
SymbolData* GetKnownSymbol(const char* name, bool searchAllModules, const char* moduleName);
Result RegisterVariable(Token identifier, Type type, const Map* symbolTable, char* externalName, bool external, bool public, int* outUniqueName);
Result RegisterFunction(Token identifier, Type returnType, const Array* funcParams, char* externalName, bool external, bool public, int* outUniqueName);
Result RegisterStruct(Token identifier, const Array* members, bool public, int* outUniqueName);
bool IsModuleName(const char* name);

void PushScope();
void PopScope(Map* outSymbolTable);
ScopeNode* GetScopeType(ScopeType type);
ScopeNode* GetCurrentScope();

void InitResources(Map* _modules);
void FreeResources();
