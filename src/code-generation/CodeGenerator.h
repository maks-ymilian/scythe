#pragma once

#include "Result.h"
#include "SyntaxTree.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength);
