#pragma once

#include "Result.h"
#include "data-structures/Array.h"

Result Scan(const char* path, const char* sourceCode, size_t sourceCodeLength, Array* outTokens);
