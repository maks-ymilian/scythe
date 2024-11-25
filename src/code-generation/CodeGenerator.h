#pragma once

#include <inttypes.h>
#include <data-structures/Map.h>

#include "Result.h"
#include "SyntaxTree.h"

Result GenerateCode(const AST* syntaxTree, Map* outPublicSymbols, uint8_t** outputCode, size_t* outputLength);
