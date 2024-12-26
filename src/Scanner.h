#pragma once

#include "Result.h"
#include "data-structures/Array.h"

Result Scan(const char* sourceCode, Array* outTokens);
void FreeTokenArray(const Array* tokens);
