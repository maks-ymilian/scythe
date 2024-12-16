#pragma once

#include "Result.h"
#include "SyntaxTree.h"

Result WriterPass(const AST* ast, char** outBuffer, size_t* outLength);
