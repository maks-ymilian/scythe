#include "SyntaxTreePrinter.h"

#include "stdio.h"
#include <assert.h>

static void PrintBinaryExpr(const BinaryExpr* expr);
static void PrintUnaryExpr(const UnaryExpr* expr);
static void PrintLiteralExpr(const LiteralExpr* expr);

static void PrintExpr(const NodePtr expr)
{
    if (expr.type == NullNode)
        return;

    assert(expr.ptr != NULL);

    switch (expr.type)
    {
        case BinaryExpression: PrintBinaryExpr(expr.ptr);
            return;
        case UnaryExpression: PrintUnaryExpr(expr.ptr);
            return;
        case LiteralExpression: PrintLiteralExpr(expr.ptr);
            return;
        default: assert(0);
    }
}

static void PrintBinaryExpr(const BinaryExpr* expr)
{
    printf("(");
    PrintExpr(expr->left);
    const char* symbol = GetTokenTypeString(expr->operator.type);
    printf(" %s ", symbol);
    PrintExpr(expr->right);
    printf(")");
}

static void PrintUnaryExpr(const UnaryExpr* expr)
{
    printf("(");
    const char* symbol = GetTokenTypeString(expr->operator.type);
    printf("%s", symbol);
    PrintExpr(expr->expression);
    printf(")");
}

static void PrintLiteralExpr(const LiteralExpr* expr)
{
    printf("%s", expr->value.text);
}

static void PrintStmt(const NodePtr stmt)
{
    assert(stmt.ptr != NULL);

    switch (stmt.type)
    {
        case ProgramRoot:
        {
            const Program* program = stmt.ptr;
            for (int i = 0; i < program->statements.length; ++i)
                PrintStmt(*(NodePtr*)program->statements.array[i]);
            return;
        }
        case Section:
        {
            const SectionStmt* sectionStmt = stmt.ptr;
            printf("%s\n", GetTokenTypeString(sectionStmt->type.type));
            PrintStmt(sectionStmt->block);
            return;
        }
        case ExpressionStatement:
        {
            PrintExpr(((ExpressionStmt*)stmt.ptr)->expr);
            return;
        }
        case VariableDeclaration:
        {
            const VarDeclStmt* varDeclStmt = stmt.ptr;
            printf("%s %s",
                   GetTokenTypeString(varDeclStmt->type.type),
                   varDeclStmt->identifier.text);

            if (varDeclStmt->initializer.type != NullNode)
            {
                printf(" = ");
                PrintExpr(varDeclStmt->initializer);
            }
            else printf("\n");
            return;
        }
        case BlockStatement:
        {
            const BlockStmt* blockStmt = stmt.ptr;
            for (int i = 0; i < blockStmt->statements.length; ++i)
                PrintSyntaxTree(blockStmt->statements.array[i]);
            return;
        }
        default: assert(0);
    }
}

void PrintSyntaxTree(const NodePtr* root)
{
    PrintStmt(*root);
    printf("\n");
}
