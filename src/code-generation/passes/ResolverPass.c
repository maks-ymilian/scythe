#include "ResolverPass.h"

#include <assert.h>
#include <stdio.h>

static Result VisitExpression(const NodePtr* in)
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

static Result VisitStatement(const NodePtr* node)
{
    switch (node->type)
    {
    case Node_Block:
    {
        const BlockStmt* block = node->ptr;
        for (int i = 0; i < block->statements.length; ++i)
            HANDLE_ERROR(VisitStatement(block->statements.array[i]));
        return SUCCESS_RESULT;
    }
    case Node_ExpressionStatement: return VisitExpression(&((ExpressionStmt*)node->ptr)->expr);
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

static Result VisitModule(const ModuleNode* module)
{
    for (int i = 0; i < module->statements.length; ++i)
        HANDLE_ERROR(VisitStatement(module->statements.array[i]));
    return SUCCESS_RESULT;
}

Result ResolverPass(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(VisitModule(node->ptr));
    }

    return SUCCESS_RESULT;
}
