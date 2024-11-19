#pragma once

#include "Common.h"

Result GenerateProgram(const AST* in);

Result GeneratePushValue(NodePtr expr, Type expectedType, long lineNumber);
