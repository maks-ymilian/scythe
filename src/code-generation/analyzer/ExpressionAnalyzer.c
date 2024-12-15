#include "ExpressionAnalyzer.h"

#include <assert.h>
#include <ctype.h>

#include "Analyzer.h"
#include "StringUtils.h"

Result AnalyzeExpression(const NodePtr* in)
{
    switch (in->type)
    {
    case Node_Binary: return SUCCESS_RESULT;
    case Node_Unary: return SUCCESS_RESULT;
    case Node_Literal: return SUCCESS_RESULT;
    case Node_MemberAccess: return SUCCESS_RESULT;
    default:
        assert(0);
    }
}
