#include "Writer.h"

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
    WRITE_LITERAL("@");
    WRITE_TEXT(section->identifier.text);
    WRITE_LITERAL("\n");

    assert(section->block.type == Node_Block);
    HANDLE_ERROR(VisitBlock(section->block.ptr));

    return SUCCESS_RESULT;
}

Result WriteAST(const AST* ast)
{
    for (int i = 0; i < ast->statements.length; ++i)
    {
        const NodePtr* stmt = ast->statements.array[i];
        switch (stmt->type)
        {
        case Node_Section: return VisitSection(stmt->ptr);
        case Node_Import: return SUCCESS_RESULT;
        default: assert(0);
        }
    }

    return SUCCESS_RESULT;
}
