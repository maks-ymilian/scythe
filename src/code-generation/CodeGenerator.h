#pragma once

#include <inttypes.h>
#include <data-structures/Map.h>

#include "Result.h"
#include "SyntaxTree.h"

typedef struct
{
    Map symbolTable;
    Map types;
} Module;

Result GenerateCode(const AST* syntaxTree, Map* modules, Module* outModule, uint8_t** outputCode, size_t* outputLength);
