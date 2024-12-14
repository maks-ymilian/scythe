#pragma once

#include "code-generation/Common.h"

#define VARIABLE_PREFIX "var_"

Result AnalyzeExpression(const NodePtr* in, Type* outType, const bool expectingExpression);

Result CheckAssignmentCompatibility(Type left, Type right, long lineNumber);