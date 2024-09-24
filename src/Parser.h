#pragma once

#include "Result.h"
#include "data-structures/Array.h"
#include "SyntaxTree.h"

Result Parse(const Array* tokenArray, NodePtr* outSyntaxTree);