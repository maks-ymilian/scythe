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

static Result AnalyzeSectionStatement(SectionStmt* out)
{
}

static Result AnalyzeVariableDeclaration(VarDeclStmt* out)
{
}

static Result AnalyzeStatement(StmtPtr* out)
{
    switch (out->type)
    {
        case ExpressionStatement:
            AnalyzeExpressionStatement(out->ptr);
            break;
        case Section:
            AnalyzeSectionStatement(out->ptr);
            break;
        case VariableDeclaration:
            AnalyzeVariableDeclaration(out->ptr);
            break;
        default:
            assert(0);
    }
}

Result AnalyzeProgram(const Program* out)
{
    for (int i = 0; i < out->statements.length; ++i)
    {
        StmtPtr* stmt = out->statements.array[i];

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
