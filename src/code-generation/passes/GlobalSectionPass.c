#include "GlobalSectionPass.h"

static Result VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
    return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(const VarDeclStmt* varDecl)
{
    return SUCCESS_RESULT;
}

static Result VisitBlock(const BlockStmt* block)
{
    return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
    assert(section->block.type == Node_Block);
    HANDLE_ERROR(VisitBlock(section->block.ptr));

    return SUCCESS_RESULT;
}

static Result VisitModule(const ModuleNode* module)
{
    for (int i = 0; i < module->statements.length; ++i)
    {
        const NodePtr* stmt = module->statements.array[i];
        switch (stmt->type)
        {
        case Node_Section: return VisitSection(stmt->ptr);
        case Node_Import: return SUCCESS_RESULT;
        default: assert(0);
        }
    }

    return SUCCESS_RESULT;
}
Result GlobalSectionPass(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(VisitModule(node->ptr));
    }

    return SUCCESS_RESULT;
}
