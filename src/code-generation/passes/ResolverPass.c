#include "ResolverPass.h"

#include <assert.h>
#include <stdio.h>
#include <StringUtils.h>
#include <data-structures/Map.h>

typedef struct Scope Scope;

struct Scope
{
    Scope* parent;
    Map declarations;
};

static Scope* currentScope = NULL;

static void PushScope()
{
    Scope* new = malloc(sizeof(Scope));
    *new = (Scope)
    {
        .declarations = AllocateMap(sizeof(NodePtr)),
        .parent = NULL,
    };

    if (currentScope == NULL)
    {
        currentScope = new;
        return;
    }

    new->parent = currentScope;
    currentScope = new;
}

static void PopScope()
{
    assert(currentScope != NULL);
    Scope* scope = currentScope;
    currentScope = currentScope->parent;
    FreeMap(&scope->declarations);
    free(scope);
}

static Result RegisterDeclaration(const char* name, const NodePtr* node, const int lineNumber)
{
    assert(currentScope != NULL);
    if (!MapAdd(&currentScope->declarations, name, node))
        return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), lineNumber);
    return SUCCESS_RESULT;
}

static Result GetDeclaration(const char* name, NodePtr* outNode, const int lineNumber)
{
    assert(currentScope != NULL);
    const Scope* scope = currentScope;
    while (scope != NULL)
    {
        const void* get = MapGet(&scope->declarations, name);
        if (get != NULL)
        {
            *outNode = *(NodePtr*)get;
            return SUCCESS_RESULT;
        }

        scope = scope->parent;
    }

    *outNode = NULL_NODE;
    return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", name), lineNumber);
}

static Result VisitLiteral(LiteralExpr* literal)
{
    switch (literal->type)
    {
    case Literal_Identifier:
    {
        HANDLE_ERROR(GetDeclaration(
            literal->identifier.text,
            &literal->identifier.reference,
            literal->lineNumber));

        if (literal->identifier.reference.type != Node_VariableDeclaration)
            return ERROR_RESULT(AllocateString1Str("\"%s\" is not the name of a variable", literal->identifier.text),
                                literal->lineNumber);

        return SUCCESS_RESULT;
    }
    case Literal_Int:
    case Literal_Float:
    case Literal_String:
    case Literal_Boolean:
        return SUCCESS_RESULT;
    default: assert(0);
    }
}

static Result VisitExpression(const NodePtr* node)
{
    switch (node->type)
    {
    case Node_Literal: return VisitLiteral(node->ptr);
    case Node_Binary:
    {
        const BinaryExpr* binary = node->ptr;
        HANDLE_ERROR(VisitExpression(&binary->left));
        HANDLE_ERROR(VisitExpression(&binary->right));
        return SUCCESS_RESULT;
    }
    case Node_Unary:
    {
        const UnaryExpr* unary = node->ptr;
        return VisitExpression(&unary->expression);
    }
    case Node_MemberAccess:
    {
        const MemberAccessExpr* memberAccess = node->ptr;
        HANDLE_ERROR(VisitExpression(&memberAccess->value));
        HANDLE_ERROR(VisitExpression(&memberAccess->next));
        return SUCCESS_RESULT;
    }
    case Node_FunctionCall:
    {
        FuncCallExpr* funcCall = node->ptr;
        HANDLE_ERROR(GetDeclaration(
            funcCall->identifier.text,
            &funcCall->identifier.reference,
            funcCall->lineNumber));

        if (funcCall->identifier.reference.type != Node_FunctionDeclaration)
            return ERROR_RESULT(AllocateString1Str("\"%s\" is not the name of a function", funcCall->identifier.text),
                                funcCall->lineNumber);

        for (int i = 0; i < funcCall->parameters.length; ++i)
        {
            const NodePtr* node = funcCall->parameters.array[i];
            HANDLE_ERROR(VisitExpression(node));
        }

        return SUCCESS_RESULT;
    }
    case Node_ArrayAccess:
    {
        ArrayAccessExpr* arrayAccess = node->ptr;
        HANDLE_ERROR(GetDeclaration(
            arrayAccess->identifier.text,
            &arrayAccess->identifier.reference,
            arrayAccess->lineNumber));

        const NodePtr* declaration = &arrayAccess->identifier.reference;
        if (declaration->type != Node_VariableDeclaration ||
            (declaration->type == Node_VariableDeclaration && !((VarDeclStmt*)declaration->ptr)->array))
            return ERROR_RESULT(
                AllocateString1Str("\"%s\" is not the name of an array", arrayAccess->identifier.text),
                arrayAccess->lineNumber);

        HANDLE_ERROR(VisitExpression(&arrayAccess->subscript));

        return SUCCESS_RESULT;
    }
    case Node_Null: return SUCCESS_RESULT;
    default: assert(0);
    }
}

static Result VisitStatement(const NodePtr* node);

static Result VisitBlock(const BlockStmt* block)
{
    PushScope();
    for (int i = 0; i < block->statements.length; ++i)
        HANDLE_ERROR(VisitStatement(block->statements.array[i]));
    PopScope();

    return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
    switch (node->type)
    {
    case Node_VariableDeclaration:
    {
        const VarDeclStmt* varDecl = node->ptr;
        HANDLE_ERROR(VisitExpression(&varDecl->initializer));
        HANDLE_ERROR(VisitExpression(&varDecl->arrayLength));
        HANDLE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
        return SUCCESS_RESULT;
    }
    case Node_FunctionDeclaration:
    {
        const FuncDeclStmt* funcDecl = node->ptr;

        PushScope();
        for (int i = 0; i < funcDecl->parameters.length; ++i)
            HANDLE_ERROR(VisitStatement(funcDecl->parameters.array[i]));

        if (funcDecl->block.ptr != NULL)
        {
            assert(funcDecl->block.type == Node_Block);
            const BlockStmt* block = funcDecl->block.ptr;
            for (int i = 0; i < block->statements.length; ++i)
                HANDLE_ERROR(VisitStatement(block->statements.array[i]));
        }
        PopScope();

        HANDLE_ERROR(RegisterDeclaration(funcDecl->name, node, funcDecl->lineNumber));
        return SUCCESS_RESULT;
    }
    case Node_StructDeclaration:
    {
        const StructDeclStmt* structDecl = node->ptr;

        PushScope();
        for (int i = 0; i < structDecl->members.length; ++i)
            HANDLE_ERROR(VisitStatement(structDecl->members.array[i]));
        PopScope();

        HANDLE_ERROR(RegisterDeclaration(structDecl->name, node, structDecl->lineNumber));
        return SUCCESS_RESULT;
    }

    case Node_Block: return VisitBlock(node->ptr);
    case Node_ExpressionStatement: return VisitExpression(&((ExpressionStmt*)node->ptr)->expr);
    case Node_Return: return VisitExpression(&((ReturnStmt*)node->ptr)->expr);

    case Node_Section:
    {
        const SectionStmt* section = node->ptr;
        assert(section->block.type == Node_Block);
        return VisitBlock(section->block.ptr);
    }
    case Node_If:
    {
        const IfStmt* ifStmt = node->ptr;
        HANDLE_ERROR(VisitExpression(&ifStmt->expr));
        HANDLE_ERROR(VisitStatement(&ifStmt->trueStmt));
        HANDLE_ERROR(VisitStatement(&ifStmt->falseStmt));
        return SUCCESS_RESULT;
    }
    case Node_While:
    {
        const WhileStmt* whileStmt = node->ptr;
        HANDLE_ERROR(VisitExpression(&whileStmt->expr));
        HANDLE_ERROR(VisitStatement(&whileStmt->stmt));
        return SUCCESS_RESULT;
    }
    case Node_For:
    {
        const ForStmt* forStmt = node->ptr;
        HANDLE_ERROR(VisitStatement(&forStmt->initialization));
        HANDLE_ERROR(VisitExpression(&forStmt->condition));
        HANDLE_ERROR(VisitExpression(&forStmt->increment));
        HANDLE_ERROR(VisitStatement(&forStmt->stmt));
        return SUCCESS_RESULT;
    }

    case Node_LoopControl:
    case Node_Import:
    case Node_Null:
        return SUCCESS_RESULT;
    default: assert(0);
    }
}

static Result VisitModule(const ModuleNode* module)
{
    PushScope();
    for (int i = 0; i < module->statements.length; ++i)
        HANDLE_ERROR(VisitStatement(module->statements.array[i]));
    PopScope();

    return SUCCESS_RESULT;
}

Result ResolverPass(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(VisitModule(node->ptr));
    }

    return SUCCESS_RESULT;
}
