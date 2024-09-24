#pragma once

#include "Common.h"

Result GenerateExpression(const NodePtr* in, Type* outType, bool expectingExpression, bool convertToInteger);

Result CheckAssignmentCompatibility(Type left, Type right, const char* errorMessage, long lineNumber);
Result GenerateFunctionParameter(Type paramType, const NodePtr* expr, long lineNumber);