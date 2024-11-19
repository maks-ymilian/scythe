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

IfStmt* AllocIfStmt(const IfStmt stmt)
{
    ALLOCATE(IfStmt, stmt);
    return new;
}

ReturnStmt* AllocReturnStmt(const ReturnStmt stmt)
{
    ALLOCATE(ReturnStmt, stmt);
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

StructDeclStmt* AllocStructDeclStmt(const StructDeclStmt stmt)
{
    ALLOCATE(StructDeclStmt, stmt);
    new->identifier = CopyToken(stmt.identifier);
    return new;
}

static void FreeNode(const NodePtr node)
{
    switch (node.type)
    {
        // expressions
        case NullNode: return;
        case BinaryExpression:
        {
            const BinaryExpr* ptr = node.ptr;

            FreeNode(ptr->left);
            FreeToken(&ptr->operator);
            FreeNode(ptr->right);

            free(node.ptr)
                    DEBUG_PRINT("Freeing BinaryExpr\n");
            return;
        }
        case UnaryExpression:
        {
            const UnaryExpr* ptr = node.ptr;

            FreeNode(ptr->expression);
            FreeToken(&ptr->operator);

            free(node.ptr)
                    DEBUG_PRINT("Freeing UnaryExpr\n");
            return;
        }
        case LiteralExpression:
        {
            const LiteralExpr* ptr = node.ptr;

            FreeToken(&ptr->value);

            if (ptr->next != NULL)
                FreeNode((NodePtr){ptr->next, LiteralExpression});

            free(node.ptr)
                    DEBUG_PRINT("Freeing LiteralExpr\n");
            return;
        }
        case FunctionCallExpression:
        {
            const FuncCallExpr* ptr = node.ptr;

            FreeToken(&ptr->identifier);
            for (int i = 0; i < ptr->parameters.length; ++i)
                FreeNode(*(NodePtr*)ptr->parameters.array[i]);
            FreeArray(&ptr->parameters);

            free(node.ptr)
                    DEBUG_PRINT("Freeing FunctionCallExpression\n");
            return;
        }

        // statements
        case Section:
        {
            DEBUG_PRINT("Freeing SectionStmt\n");
            const SectionStmt* ptr = node.ptr;

            FreeToken(&ptr->type);
            FreeNode(ptr->block);

            free(node.ptr);
            return;
        }
        case ReturnStatement:
        {
            DEBUG_PRINT("Freeing ReturnStmt\n")
            const ReturnStmt* ptr = node.ptr;

            FreeNode(ptr->expr);

            free(node.ptr);
            return;
        }
        case ExpressionStatement:
        {
            DEBUG_PRINT("Freeing ExpressionStmt\n")
            const ExpressionStmt* ptr = node.ptr;

            FreeNode(ptr->expr);

            free(node.ptr);
            return;
        }
        case VariableDeclaration:
        {
            DEBUG_PRINT("Freeing VarDeclStmt\n");
            const VarDeclStmt* ptr = node.ptr;

            FreeToken(&ptr->identifier);
            FreeToken(&ptr->type);
            FreeNode(ptr->initializer);

            free(node.ptr);
            return;
        }
        case BlockStatement:
        {
            DEBUG_PRINT("Freeing BlockStatement\n");
            const BlockStmt* ptr = node.ptr;

            for (int i = 0; i < ptr->statements.length; ++i)
                FreeNode(*(NodePtr*)ptr->statements.array[i]);
            FreeArray(&ptr->statements);

            free(node.ptr);
            return;
        }
        case IfStatement:
        {
            DEBUG_PRINT("Freeing IfStatement\n");
            const IfStmt* ptr = node.ptr;

            FreeNode(ptr->expr);
            FreeNode(ptr->trueStmt);
            FreeNode(ptr->falseStmt);

            free(node.ptr);
            return;
        }
        case FunctionDeclaration:
        {
            DEBUG_PRINT("Freeing FunctionDeclaration\n");
            const FuncDeclStmt* ptr = node.ptr;

            for (int i = 0; i < ptr->parameters.length; ++i)
                FreeNode(*(NodePtr*)ptr->parameters.array[i]);
            FreeArray(&ptr->parameters);

            free(node.ptr);
            return;
        }
        case StructDeclaration:
        {
            DEBUG_PRINT("Freeing StructDeclaration\n");
            const StructDeclStmt* ptr = node.ptr;

            for (int i = 0; i < ptr->members.length; ++i)
                FreeNode(*(NodePtr*)ptr->members.array[i]);
            FreeArray(&ptr->members);

            free(node.ptr);
            return;
        }
        default: assert(0);
    }
}

void FreeSyntaxTree(const Program root)
{
    for (int i = 0; i < root.statements.length; ++i)
    {
        const NodePtr* node = root.statements.array[i];
        FreeNode(*node);
    }
}
