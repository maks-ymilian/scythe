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

FuncCallExpr* AllocFuncCall(const FuncCallExpr expr)
{
    ALLOCATE(FuncCallExpr, expr);
    new->identifier = CopyToken(expr.identifier);
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

FuncDeclStmt* AllocFuncDeclStmt(const FuncDeclStmt stmt)
{
    ALLOCATE(FuncDeclStmt, stmt);
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
    switch (expr.type)
    {
        case NoExpression: return;
        case BinaryExpression:
        {
            const BinaryExpr* binary = expr.ptr;

            FreeExpr(binary->left);
            FreeToken(&binary->operator);
            FreeExpr(binary->right);

            free(expr.ptr)
            DEBUG_PRINT("Freeing BinaryExpr\n");
            return;
        }
        case UnaryExpression:
        {
            const UnaryExpr* unary = expr.ptr;

            FreeExpr(unary->expression);
            FreeToken(&unary->operator);

            free(expr.ptr)
            DEBUG_PRINT("Freeing UnaryExpr\n");
            return;
        }
        case LiteralExpression:
        {
            const LiteralExpr* literal = expr.ptr;

            FreeToken(&literal->value);

            free(expr.ptr)
            DEBUG_PRINT("Freeing LiteralExpr\n");
            return;
        }
        case FunctionCallExpression:
        {
            const FuncCallExpr* funcCall = expr.ptr;

            FreeToken(&funcCall->identifier);
            for (int i = 0; i < funcCall->parameters.length; ++i)
                FreeExpr(*(ExprPtr*)funcCall->parameters.array[i]);
            FreeArray(&funcCall->parameters);

            free(expr.ptr)
            DEBUG_PRINT("Freeing FunctionCallExpression\n");
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

            for (int i = 0; i < program->statements.length; ++i)
                FreeStmt(*(StmtPtr*)program->statements.array[i]);
            FreeArray(&program->statements);

            free(stmt.ptr);
            return;
        }
        case BlockStatement:
        {
            DEBUG_PRINT("Freeing BlockStatement\n");
            const BlockStmt* blockStmt = stmt.ptr;

            for (int i = 0; i < blockStmt->statements.length; ++i)
                FreeStmt(*(StmtPtr*)blockStmt->statements.array[i]);
            FreeArray(&blockStmt->statements);

            free(stmt.ptr);
            return;
        }
        case FunctionDeclaration:
        {
            DEBUG_PRINT("Freeing FunctionDeclaration\n");
            const FuncDeclStmt* funcDeclStmt = stmt.ptr;

            for (int i = 0; i < funcDeclStmt->parameters.length; ++i)
                FreeStmt(*(StmtPtr*)funcDeclStmt->parameters.array[i]);
            FreeArray(&funcDeclStmt->parameters);

            free(stmt.ptr);
            return;
        }
        default: assert(0);
    }
}

void FreeSyntaxTree(const StmtPtr root)
{
    FreeStmt(root);
}
