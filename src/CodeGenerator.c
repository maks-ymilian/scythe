#include "CodeGenerator.h"

#include <assert.h>
#include <stdlib.h>

#include "SyntaxTreePrinter.h"

static Result GenerateAssignment(ExprPtr* out)
{
}

static Result GenerateExpression(ExprPtr* out)
{
}

static Result GenerateExpressionStatement(ExpressionStmt* out)
{
}

static Result GenerateVariableDeclaration(VarDeclStmt* out)
{
}

static Result GenerateStatement(const StmtPtr* out);

static Result GenerateBlockStatement(const BlockStmt* out)
{
    for (int i = 0; i < out->statements.length; ++i)
    {
        const StmtPtr* stmt = out->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", 69420);
        GenerateStatement(stmt);
    }

    return SUCCESS_RESULT;
}

static Result GenerateSectionStatement(SectionStmt* out)
{
    return GenerateBlockStatement(out->block.ptr);
}

static Result GenerateStatement(const StmtPtr* out)
{
    switch (out->type)
    {
        case ExpressionStatement:
            return GenerateExpressionStatement(out->ptr);
        case Section:
            return GenerateSectionStatement(out->ptr);
        case VariableDeclaration:
            return GenerateVariableDeclaration(out->ptr);
        default:
            assert(0);
    }
}

Result GenerateProgram(const Program* out)
{
    for (int i = 0; i < out->statements.length; ++i)
    {
        const StmtPtr* stmt = out->statements.array[i];
        assert(stmt->type != ProgramRoot);
        GenerateStatement(stmt);
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree)
{
    const StmtPtr stmt = {syntaxTree, ProgramRoot};
    PrintSyntaxTree(&stmt);

    GenerateProgram(syntaxTree);

    FreeSyntaxTree(stmt);

    return SUCCESS_RESULT;
}
