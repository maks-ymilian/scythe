#pragma once

#include "Common.h"

Result GenerateProgram(const Program* in);

Result GeneratePushValue(NodePtr expr, Type expectedType, long lineNumber);
