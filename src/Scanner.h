#pragma once

#include "data-structures/Array.h"
#include "Result.h"

Result Scan(const char* sourceCode, Array* outTokens);
void FreeTokenArray(const Array* tokens);
