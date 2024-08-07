#include "SyntaxTree.h"

#include <assert.h>
#include "stdlib.h"
#include <stdio.h>

// #define ENABLE_PRINT
#ifdef ENABLE_PRINT
#define DEBUG_PRINT(message) printf(message);
#else
#define DEBUG_PRINT(message)
#endif

#define COMMA ,
#define ALLOCATE(type, paramName)\
type* new = malloc(sizeof(type));\
assert(new != NULL);\
*new = paramName;\
DEBUG_PRINT("%s" COMMA "Allocating "#type"\n");

BinaryExpr* AllocBinary(const BinaryExpr expr)
{
    ALLOCATE(BinaryExpr, expr);
    new->operator = CopyToken(expr.operator);
    return new;
}

UnaryExpr* AllocUnary(const UnaryExpr expr)
{
    ALLOCATE(UnaryExpr, expr);
    new->operator = CopyToken(expr.operator);
    return new;
}

LiteralExpr* AllocLiteral(const LiteralExpr expr)
{
    ALLOCATE(LiteralExpr, expr);
    new->value = CopyToken(expr.value);
    return new;
}

BlockStmt* AllocBlockStmt(const BlockStmt stmt)
{
    ALLOCATE(BlockStmt, stmt);
    return new;
}

SectionStmt* AllocSectionStmt(const SectionStmt stmt)
{
    ALLOCATE(SectionStmt, stmt);
    new->type = CopyToken(stmt.type);
    return new;
}

ExpressionStmt* AllocExpressionStmt(const ExpressionStmt stmt)
{
    ALLOCATE(ExpressionStmt, stmt);
    return new;
}

VarDeclStmt* AllocVarDeclStmt(const VarDeclStmt stmt)
{
    ALLOCATE(VarDeclStmt, stmt);
    new->identifier = CopyToken(stmt.identifier);
    new->type = CopyToken(stmt.type);
    return new;
}

Program* AllocProgram(const Program program)
{
    ALLOCATE(Program, program);
    return new;
}

static void FreeExpr(const ExprPtr expr)
{
    assert(expr.ptr != NULL);

    switch (expr.type)
    {
        case NoExpression: return;
        case BinaryExpression:
        {
            const BinaryExpr* binary = expr.ptr;
            FreeExpr(binary->left);
            FreeToken(&binary->operator);
            FreeExpr(binary->right);
            free((void*)binary);
            DEBUG_PRINT("Freeing BinaryExpr\n");
            return;
        }
        case UnaryExpression:
        {
            const UnaryExpr* unary = expr.ptr;
            FreeExpr(unary->expression);
            FreeToken(&unary->operator);
            free((void*)unary);
            DEBUG_PRINT("Freeing UnaryExpr\n");
            return;
        }
        case LiteralExpression:
        {
            const LiteralExpr* literal = expr.ptr;
            FreeToken(&literal->value);
            free((void*)literal);
            DEBUG_PRINT("Freeing LiteralExpr\n");
            return;
        }
        default: assert(0);
    }
}

static void FreeStmt(const StmtPtr stmt)
{
    assert(stmt.ptr != NULL);

    switch (stmt.type)
    {
        case Section:
        {
            DEBUG_PRINT("Freeing SectionStmt\n");
            const SectionStmt* sectionStmt = stmt.ptr;
            FreeToken(&sectionStmt->type);
            FreeStmt(sectionStmt->block);
            free(stmt.ptr);
            return;
        }
        case ExpressionStatement:
        {
            DEBUG_PRINT("Freeing ExpressionStmt\n")
            const ExpressionStmt* expressionStmt = stmt.ptr;
            FreeExpr(expressionStmt->expr);
            free(stmt.ptr);
            return;
        }
        case VariableDeclaration:
        {
            DEBUG_PRINT("Freeing VarDeclStmt\n");
            const VarDeclStmt* varDeclStmt = stmt.ptr;
            FreeToken(&varDeclStmt->identifier);
            FreeToken(&varDeclStmt->type);
            FreeExpr(varDeclStmt->initializer);
            free(stmt.ptr);
            return;
        }
        case ProgramRoot:
        {
            DEBUG_PRINT("Freeing ProgramRoot\n");
            const Program* program = stmt.ptr;
            FreeArray(&program->statements);
            free(stmt.ptr);
            return;
        }
        case BlockStatement:
        {
            const BlockStmt* blockStmt = stmt.ptr;
            FreeArray(&blockStmt->statements);
            return;
        }
        default: assert(0);
    }
}

void FreeSyntaxTree(const StmtPtr root)
{
    FreeStmt(root);
}
