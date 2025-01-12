#pragma once

#include "Result.h"
#include "SyntaxTree.h"

void WriterPass(const AST* ast, char** outBuffer, size_t* outLength);
