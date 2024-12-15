#pragma once

#include "Result.h"
#include "SyntaxTree.h"

Result Write(const AST* ast, char** outBuffer, size_t* outLength);
