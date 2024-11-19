#pragma once

#include <inttypes.h>

#include "Result.h"
#include "SyntaxTree.h"

Result GenerateCode(const Program* syntaxTree, uint8_t** outputCode, size_t* outputLength);
