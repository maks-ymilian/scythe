#pragma once

#include <stdlib.h>

#include "SyntaxTreePrinter.h"
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

typedef enum { VariableSymbol, FunctionSymbol, StructSymbol } SymbolType;

typedef struct
{
    SymbolType symbolType;
    Type type;
    Array parameters;
    const Array* members;
} SymbolAttributes;

typedef struct ScopeNode ScopeNode;

struct ScopeNode
{
    ScopeNode* parent;
    Map symbolTable;
    bool isFunction;
    Type functionReturnType;
};

typedef enum { MainStream, FunctionsStream } StreamType;

void Write(const void* buffer, size_t length);
Buffer CombineStreams();
void SetCurrentStream(StreamType stream);
void BeginUndo();
void EndUndo();
void BeginRead();
Buffer EndRead();

char* AllocateString1Str(const char* format, const char* insert);
char* AllocateString2Str(const char* format, const char* insert1, const char* insert2);
char* AllocateString2Int(const char* format, int insert1, int insert2);

Result GetTypeFromToken(Token typeToken, Type* outType, bool allowVoid);
Type GetKnownType(const char* name);

Result GetSymbol(Token identifier, SymbolAttributes** outSymbol);
SymbolAttributes* GetKnownSymbol(Token identifier);
Result RegisterVariable(Token identifier, Type type);
Result RegisterFunction(Token identifier, Type returnType, const Array* funcParams);
Result RegisterStruct(Token identifier, const Array* members);

void PushScope();
void PopScope();
ScopeNode* GetFunctionScope();
ScopeNode* GetCurrentScope();

void InitResources();
void FreeResources();