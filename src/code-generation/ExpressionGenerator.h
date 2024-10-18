#pragma once

#include "Common.h"

#define VARIABLE_PREFIX "var_"

Result GenerateExpression(const NodePtr* in, Type* outType, bool expectingExpression, bool convertToInteger);

Result CheckAssignmentCompatibility(Type left, Type right, long lineNumber);
