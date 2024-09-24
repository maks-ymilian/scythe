#pragma once

#include <inttypes.h>

#include "Result.h"
#include "SyntaxTree.h"

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length);
