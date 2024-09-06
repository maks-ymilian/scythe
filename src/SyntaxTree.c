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

static void FreeNode(const NodePtr node)
{
    switch (node.type)
    {
        case NullNode: return;
        case BinaryExpression:
        {
            const BinaryExpr* binary = node.ptr;

            FreeNode(binary->left);
            FreeToken(&binary->operator);
            FreeNode(binary->right);

            free(node.ptr)
            DEBUG_PRINT("Freeing BinaryExpr\n");
            return;
        }
        case UnaryExpression:
        {
            const UnaryExpr* unary = node.ptr;

            FreeNode(unary->expression);
            FreeToken(&unary->operator);

            free(node.ptr)
            DEBUG_PRINT("Freeing UnaryExpr\n");
            return;
        }
        case LiteralExpression:
        {
            const LiteralExpr* literal = node.ptr;

            FreeToken(&literal->value);

            free(node.ptr)
            DEBUG_PRINT("Freeing LiteralExpr\n");
            return;
        }
        case FunctionCallExpression:
        {
            const FuncCallExpr* funcCall = node.ptr;

            FreeToken(&funcCall->identifier);
            for (int i = 0; i < funcCall->parameters.length; ++i)
                FreeNode(*(NodePtr*)funcCall->parameters.array[i]);
            FreeArray(&funcCall->parameters);

            free(node.ptr)
            DEBUG_PRINT("Freeing FunctionCallExpression\n");
            return;
        }

        // statements
        case Section:
        {
            DEBUG_PRINT("Freeing SectionStmt\n");
            const SectionStmt* sectionStmt = node.ptr;

            FreeToken(&sectionStmt->type);
            FreeNode(sectionStmt->block);

            free(node.ptr);
            return;
        }
        case ExpressionStatement:
        {
            DEBUG_PRINT("Freeing ExpressionStmt\n")
            const ExpressionStmt* expressionStmt = node.ptr;

            FreeNode(expressionStmt->expr);

            free(node.ptr);
            return;
        }
        case VariableDeclaration:
        {
            DEBUG_PRINT("Freeing VarDeclStmt\n");
            const VarDeclStmt* varDeclStmt = node.ptr;

            FreeToken(&varDeclStmt->identifier);
            FreeToken(&varDeclStmt->type);
            FreeNode(varDeclStmt->initializer);

            free(node.ptr);
            return;
        }
        case ProgramRoot:
        {
            DEBUG_PRINT("Freeing ProgramRoot\n");
            const Program* program = node.ptr;

            for (int i = 0; i < program->statements.length; ++i)
                FreeNode(*(NodePtr*)program->statements.array[i]);
            FreeArray(&program->statements);

            free(node.ptr);
            return;
        }
        case BlockStatement:
        {
            DEBUG_PRINT("Freeing BlockStatement\n");
            const BlockStmt* blockStmt = node.ptr;

            for (int i = 0; i < blockStmt->statements.length; ++i)
                FreeNode(*(NodePtr*)blockStmt->statements.array[i]);
            FreeArray(&blockStmt->statements);

            free(node.ptr);
            return;
        }
        case FunctionDeclaration:
        {
            DEBUG_PRINT("Freeing FunctionDeclaration\n");
            const FuncDeclStmt* funcDeclStmt = node.ptr;

            for (int i = 0; i < funcDeclStmt->parameters.length; ++i)
                FreeNode(*(NodePtr*)funcDeclStmt->parameters.array[i]);
            FreeArray(&funcDeclStmt->parameters);

            free(node.ptr);
            return;
        }
        default: assert(0);
    }
}

void FreeSyntaxTree(const NodePtr root)
{
    FreeNode(root);
}
