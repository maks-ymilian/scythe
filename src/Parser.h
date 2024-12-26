#pragma once

#include "Result.h"
#include "SyntaxTree.h"
#include "data-structures/Array.h"

Result Parse(const Array* tokenArray, AST* outSyntaxTree);
