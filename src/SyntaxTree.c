#include "SyntaxTree.h"

#include <assert.h>
#include "stdlib.h"
#include <stdio.h>
#include <string.h>

// #define ENABLE_PRINT
#ifdef ENABLE_PRINT
#define DEBUG_PRINT(message) printf(message);
#else
#define DEBUG_PRINT(message) ;
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
    new->identifier = CopyToken(stmt.identifier);
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
    new->externalIdentifier = CopyToken(stmt.externalIdentifier);
    new->type = CopyToken(stmt.type);
    return new;
}

FuncDeclStmt* AllocFuncDeclStmt(const FuncDeclStmt stmt)
{
    ALLOCATE(FuncDeclStmt, stmt);
    new->identifier = CopyToken(stmt.identifier);
    new->externalIdentifier = CopyToken(stmt.externalIdentifier);
    new->type = CopyToken(stmt.type);
    return new;
}

StructDeclStmt* AllocStructDeclStmt(const StructDeclStmt stmt)
{
    ALLOCATE(StructDeclStmt, stmt);
    new->identifier = CopyToken(stmt.identifier);
    return new;
}

ImportStmt* AllocImportStmt(const ImportStmt stmt)
{
    ALLOCATE(ImportStmt, stmt);

    new->import = CopyToken(stmt.import);

    const char* oldString = new->file;
    const size_t length = strlen(oldString) + 1;
    new->file = malloc(length);
    memcpy(new->file, oldString, length);

    return new;
}

WhileStmt* AllocWhileStmt(const WhileStmt stmt)
{
    ALLOCATE(WhileStmt, stmt);
    return new;
}

ForStmt* AllocForStmt(ForStmt stmt)
{
    ALLOCATE(ForStmt, stmt);
    return new;
}

LoopControlStmt* AllocLoopControlStmt(LoopControlStmt stmt)
{
    ALLOCATE(LoopControlStmt, stmt);
    return new;
}

static void FreeNode(const NodePtr node)
{
    switch (node.type)
    {
    // expressions
    case Node_Null: return;
    case Node_Binary:
    {
        const BinaryExpr* ptr = node.ptr;

        FreeNode(ptr->left);
        FreeToken(&ptr->operator);
        FreeNode(ptr->right);

        free(node.ptr)
                DEBUG_PRINT("Freeing BinaryExpr\n");
        return;
    }
    case Node_Unary:
    {
        const UnaryExpr* ptr = node.ptr;

        FreeNode(ptr->expression);
        FreeToken(&ptr->operator);

        free(node.ptr)
                DEBUG_PRINT("Freeing UnaryExpr\n");
        return;
    }
    case Node_Literal:
    {
        const LiteralExpr* ptr = node.ptr;

        FreeToken(&ptr->value);
        FreeNode(ptr->next);
        free(node.ptr)
                DEBUG_PRINT("Freeing LiteralExpr\n");
        return;
    }
    case Node_FunctionCall:
    {
        const FuncCallExpr* ptr = node.ptr;

        FreeToken(&ptr->identifier);
        for (int i = 0; i < ptr->parameters.length; ++i)
            FreeNode(*(NodePtr*)ptr->parameters.array[i]);
        FreeArray(&ptr->parameters);

        free(node.ptr);
        DEBUG_PRINT("Freeing FunctionCallExpression\n");
        return;
    }

    // statements
    case Node_Import:
    {
        DEBUG_PRINT("Freeing ImportStmt\n");
        const ImportStmt* ptr = node.ptr;
        FreeToken(&ptr->import);
        free(ptr->file);
        return;
    }
    case Node_Section:
    {
        DEBUG_PRINT("Freeing SectionStmt\n");
        const SectionStmt* ptr = node.ptr;

        FreeToken(&ptr->identifier);
        FreeNode(ptr->block);

        free(node.ptr);
        return;
    }
    case Node_Return:
    {
        DEBUG_PRINT("Freeing ReturnStmt\n")
        const ReturnStmt* ptr = node.ptr;

        FreeNode(ptr->expr);

        free(node.ptr);
        return;
    }
    case Node_ExpressionStatement:
    {
        DEBUG_PRINT("Freeing ExpressionStmt\n")
        const ExpressionStmt* ptr = node.ptr;

        FreeNode(ptr->expr);

        free(node.ptr);
        return;
    }
    case Node_VariableDeclaration:
    {
        DEBUG_PRINT("Freeing VarDeclStmt\n");
        const VarDeclStmt* ptr = node.ptr;

        FreeToken(&ptr->identifier);
        FreeToken(&ptr->externalIdentifier);
        FreeToken(&ptr->type);
        FreeNode(ptr->initializer);

        free(node.ptr);
        return;
    }
    case Node_Block:
    {
        DEBUG_PRINT("Freeing BlockStatement\n");
        const BlockStmt* ptr = node.ptr;

        for (int i = 0; i < ptr->statements.length; ++i)
            FreeNode(*(NodePtr*)ptr->statements.array[i]);
        FreeArray(&ptr->statements);

        free(node.ptr);
        return;
    }
    case Node_If:
    {
        DEBUG_PRINT("Freeing IfStatement\n");
        const IfStmt* ptr = node.ptr;

        FreeNode(ptr->expr);
        FreeNode(ptr->trueStmt);
        FreeNode(ptr->falseStmt);

        free(node.ptr);
        return;
    }
    case Node_FunctionDeclaration:
    {
        DEBUG_PRINT("Freeing FunctionDeclaration\n");
        const FuncDeclStmt* ptr = node.ptr;

        for (int i = 0; i < ptr->parameters.length; ++i)
            FreeNode(*(NodePtr*)ptr->parameters.array[i]);
        FreeArray(&ptr->parameters);

        FreeToken(&ptr->identifier);
        FreeToken(&ptr->externalIdentifier);

        free(node.ptr);
        return;
    }
    case Node_StructDeclaration:
    {
        DEBUG_PRINT("Freeing StructDeclaration\n");
        const StructDeclStmt* ptr = node.ptr;

        for (int i = 0; i < ptr->members.length; ++i)
            FreeNode(*(NodePtr*)ptr->members.array[i]);
        FreeArray(&ptr->members);

        free(node.ptr);
        return;
    }
    case Node_While:
    {
        DEBUG_PRINT("Freeing WhileStmt\n");
        const WhileStmt* ptr = node.ptr;

        FreeNode(ptr->expr);
        FreeNode(ptr->stmt);

        free(node.ptr);
        return;
    }
    case Node_For:
    {
        DEBUG_PRINT("Freeing ForStmt\n");
        const ForStmt* ptr = node.ptr;

        FreeNode(ptr->condition);
        FreeNode(ptr->initialization);
        FreeNode(ptr->increment);
        FreeNode(ptr->stmt);

        free(node.ptr);
        return;
    }
    case Node_LoopControl:
    {
        DEBUG_PRINT("Freeing LoopControl\n");
        free(node.ptr);
        return;
    }
    default: assert(0);
    }
}

void FreeSyntaxTree(const AST root)
{
    for (int i = 0; i < root.statements.length; ++i)
    {
        const NodePtr* node = root.statements.array[i];
        FreeNode(*node);
    }
}
