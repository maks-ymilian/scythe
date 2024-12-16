#include "Analyzer.h"

#include <assert.h>

#include "ExpressionAnalyzer.h"

static Result AnalyzeStatement(const NodePtr* node)
{
    switch (node->type)
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

static Result AnalyzeModule(const ModuleNode* module)
{
    for (int i = 0; i < module->statements.length; ++i)
        HANDLE_ERROR(AnalyzeStatement(module->statements.array[i]));
    return SUCCESS_RESULT;
}

Result Analyze(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(AnalyzeModule(node->ptr));
    }

    return SUCCESS_RESULT;
}
