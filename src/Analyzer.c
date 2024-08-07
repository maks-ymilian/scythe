#include "Analyzer.h"

#include <assert.h>
#include <stdlib.h>

#include "SyntaxTreePrinter.h"

static Result AnalyzeAssignment(ExprPtr* out)
{
}

static Result AnalyzeExpression(ExprPtr* out)
{
}

static Result AnalyzeExpressionStatement(ExpressionStmt* out)
{
}

static Result AnalyzeVariableDeclaration(VarDeclStmt* out)
{
}

static Result AnalyzeStatement(const StmtPtr* out);

static Result AnalyzeBlockStatement(const BlockStmt* out)
{
    for (int i = 0; i < out->statements.length; ++i)
    {
        const StmtPtr* stmt = out->statements.array[i];
        if (stmt->type == Section) return ERROR_RESULT("Nested sections are not allowed", 69420);
        AnalyzeStatement(stmt);
    }

    return SUCCESS_RESULT;
}

static Result AnalyzeSectionStatement(SectionStmt* out)
{
    return AnalyzeBlockStatement(out->block.ptr);
}

static Result AnalyzeStatement(const StmtPtr* out)
{
    switch (out->type)
    {
        case ExpressionStatement:
            return AnalyzeExpressionStatement(out->ptr);
        case Section:
            return AnalyzeSectionStatement(out->ptr);
        case VariableDeclaration:
            return AnalyzeVariableDeclaration(out->ptr);
        default:
            assert(0);
    }
}

Result AnalyzeProgram(const Program* out)
{
    for (int i = 0; i < out->statements.length; ++i)
    {
        const StmtPtr* stmt = out->statements.array[i];
        assert(stmt->type != ProgramRoot);
        AnalyzeStatement(stmt);
    }

    return SUCCESS_RESULT;
}

Result Analyze(Program* syntaxTree)
{
    const StmtPtr stmt = {syntaxTree, ProgramRoot};
    PrintSyntaxTree(&stmt);

    AnalyzeProgram(syntaxTree);

    FreeSyntaxTree(stmt);

    return SUCCESS_RESULT;
}
