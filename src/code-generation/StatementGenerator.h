#pragma once

#include "Common.h"

Result GenerateStatement(const NodePtr* in);

Result GeneratePushValue(NodePtr expr, Type expectedType, long lineNumber);
