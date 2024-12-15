#include "Analyzer.h"

#include <assert.h>

#include "ExpressionAnalyzer.h"

static Result AnalyzeStatement(const NodePtr* in)
{
    switch (in->type)
    {
    case Node_Block: SUCCESS_RESULT;
    case Node_Import: return SUCCESS_RESULT;
    case Node_Return: return SUCCESS_RESULT;
    case Node_Section: return SUCCESS_RESULT;
    case Node_VariableDeclaration: return SUCCESS_RESULT;
    case Node_FunctionDeclaration: return SUCCESS_RESULT;
    case Node_StructDeclaration: return SUCCESS_RESULT;
    case Node_If: return SUCCESS_RESULT;
    case Node_While: return SUCCESS_RESULT;
    case Node_For: return SUCCESS_RESULT;
    case Node_LoopControl: return SUCCESS_RESULT;
    default: assert(0);
    }
}

Result Analyze(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
        HANDLE_ERROR(AnalyzeStatement(ast->nodes.array[i]));

    return SUCCESS_RESULT;
}
