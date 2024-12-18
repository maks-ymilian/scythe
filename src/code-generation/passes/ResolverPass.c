#include "ResolverPass.h"

#include <assert.h>
#include <stdio.h>
#include <StringUtils.h>
#include <data-structures/Map.h>

static Array currentAccessibleModules;

static Map modules;

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

static void PopScope(Map* outMap)
{
    assert(currentScope != NULL);
    Scope* scope = currentScope;

    currentScope = currentScope->parent;

    if (outMap == NULL)
        FreeMap(&scope->declarations);
    else
        *outMap = scope->declarations;

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

static bool MemberAccessIsIdentifier(const MemberAccessExpr* memberAccess)
{
    return memberAccess != NULL &&
           memberAccess->value.type == Node_Literal &&
           ((LiteralExpr*)memberAccess->value.ptr)->type == Literal_Identifier;
}

static bool IsModuleAccessible(const Map* module)
{
    for (int i = 0; i < currentAccessibleModules.length; ++i)
    {
        const Map* m = *(Map**)currentAccessibleModules.array[i];
        if (module == m)
            return true;
    }
    return false;
}

static Result ResolveModuleAccess(MemberAccessExpr* current)
{
    if (current->next.ptr == NULL)
        return SUCCESS_RESULT;

    assert(current->next.type == Node_MemberAccess);
    const MemberAccessExpr* next = current->next.ptr;

    if (!MemberAccessIsIdentifier(current))
        return SUCCESS_RESULT;

    const LiteralExpr* currentLiteral = current->value.ptr;
    const Map* module = MapGet(&modules, currentLiteral->identifier.text);
    if (module == NULL || !IsModuleAccessible(module))
        return SUCCESS_RESULT;

    IdentifierReference* nextIdentifier = NULL;
    if (MemberAccessIsIdentifier(next))
        nextIdentifier = &((LiteralExpr*)next->value.ptr)->identifier;
    else if (next->value.type == Node_FunctionCall)
        nextIdentifier = &((FuncCallExpr*)next->value.ptr)->identifier;
    else if (next->value.type == Node_ArrayAccess)
        nextIdentifier = &((ArrayAccessExpr*)next->value.ptr)->identifier;
    else
        assert(0);

    const NodePtr* declaration = MapGet(module, nextIdentifier->text);
    if (declaration == NULL)
        return ERROR_RESULT(AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"",
                                nextIdentifier->text, currentLiteral->identifier.text),
                            currentLiteral->lineNumber);

    bool public;
    if (declaration->type == Node_StructDeclaration)
        public = ((StructDeclStmt*)declaration->ptr)->public;
    else if (declaration->type == Node_FunctionDeclaration)
        public = ((FuncDeclStmt*)declaration->ptr)->public;
    else if (declaration->type == Node_VariableDeclaration)
        public = ((VarDeclStmt*)declaration->ptr)->public;
    else
        assert(0);

    if (!public)
        return ERROR_RESULT(AllocateString2Str("Identifier \"%s\" in module \"%s\" cannot be accessed because it is not public",
                                nextIdentifier->text, currentLiteral->identifier.text),
                            currentLiteral->lineNumber);

    nextIdentifier->reference = *declaration;

    FreeASTNode(current->value);
    current->value = NULL_NODE;

    return SUCCESS_RESULT;
}

static Result VisitLiteral(LiteralExpr* literal)
{
    switch (literal->type)
    {
    case Literal_Identifier:
    {
        if (literal->identifier.reference.ptr == NULL)
            HANDLE_ERROR(GetDeclaration(
            literal->identifier.text,
            &literal->identifier.reference,
            literal->lineNumber));

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
        MemberAccessExpr* memberAccess = node->ptr;
        HANDLE_ERROR(ResolveModuleAccess(memberAccess));
        HANDLE_ERROR(VisitExpression(&memberAccess->value));
        HANDLE_ERROR(VisitExpression(&memberAccess->next));
        return SUCCESS_RESULT;
    }
    case Node_FunctionCall:
    {
        FuncCallExpr* funcCall = node->ptr;

        if (funcCall->identifier.reference.ptr == NULL)
            HANDLE_ERROR(GetDeclaration(
            funcCall->identifier.text,
            &funcCall->identifier.reference,
            funcCall->lineNumber));

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

        if (arrayAccess->identifier.reference.ptr == NULL)
            HANDLE_ERROR(GetDeclaration(
            arrayAccess->identifier.text,
            &arrayAccess->identifier.reference,
            arrayAccess->lineNumber));

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
    PopScope(NULL);

    return SUCCESS_RESULT;
}

static Result GetType(TypeReference* inout, const int lineNumber)
{
    if (inout->primitive)
        return SUCCESS_RESULT;

    HANDLE_ERROR(GetDeclaration(
        inout->text,
        &inout->value.reference,
        lineNumber));

    return SUCCESS_RESULT;
}

static void MakeImportAccessible(const ImportStmt* import, bool topLevel)
{
    Map* module = MapGet(&modules, import->moduleName);
    assert(module != NULL);

    ArrayAdd(&currentAccessibleModules, &module);

    if (import->public || topLevel)
    {
        for (MAP_ITERATE(i, module))
        {
            const NodePtr* node = i->value;
            if (node->type == Node_Import &&
                ((ImportStmt*)node->ptr)->public)
                MakeImportAccessible(node->ptr, false);
        }
    }
}

static Result VisitStatement(const NodePtr* node)
{
    switch (node->type)
    {
    case Node_VariableDeclaration:
    {
        VarDeclStmt* varDecl = node->ptr;
        HANDLE_ERROR(VisitExpression(&varDecl->initializer));
        HANDLE_ERROR(VisitExpression(&varDecl->arrayLength));
        HANDLE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
        HANDLE_ERROR(GetType(&varDecl->type, varDecl->lineNumber));
        return SUCCESS_RESULT;
    }
    case Node_FunctionDeclaration:
    {
        FuncDeclStmt* funcDecl = node->ptr;

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
        PopScope(NULL);

        HANDLE_ERROR(GetType(&funcDecl->type, funcDecl->lineNumber));
        HANDLE_ERROR(RegisterDeclaration(funcDecl->name, node, funcDecl->lineNumber));
        return SUCCESS_RESULT;
    }
    case Node_StructDeclaration:
    {
        const StructDeclStmt* structDecl = node->ptr;

        PushScope();
        for (int i = 0; i < structDecl->members.length; ++i)
            HANDLE_ERROR(VisitStatement(structDecl->members.array[i]));
        PopScope(NULL);

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
        PushScope();
        HANDLE_ERROR(VisitStatement(&ifStmt->trueStmt));
        PopScope(NULL);
        PushScope();
        HANDLE_ERROR(VisitStatement(&ifStmt->falseStmt));
        PopScope(NULL);
        return SUCCESS_RESULT;
    }
    case Node_While:
    {
        const WhileStmt* whileStmt = node->ptr;
        HANDLE_ERROR(VisitExpression(&whileStmt->expr));
        PushScope();
        HANDLE_ERROR(VisitStatement(&whileStmt->stmt));
        PopScope(NULL);
        return SUCCESS_RESULT;
    }
    case Node_For:
    {
        const ForStmt* forStmt = node->ptr;
        PushScope();
        HANDLE_ERROR(VisitStatement(&forStmt->initialization));
        HANDLE_ERROR(VisitExpression(&forStmt->condition));
        HANDLE_ERROR(VisitExpression(&forStmt->increment));
        HANDLE_ERROR(VisitStatement(&forStmt->stmt));
        PopScope(NULL);
        return SUCCESS_RESULT;
    }

    case Node_Import:
    {
        const ImportStmt* import = node->ptr;
        HANDLE_ERROR(RegisterDeclaration(import->moduleName, node, import->lineNumber));
        MakeImportAccessible(import, true);
        return SUCCESS_RESULT;
    }
    case Node_LoopControl:
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

    Map declarations;
    PopScope(&declarations);
    if (!MapAdd(&modules, module->moduleName, &declarations))
        assert(0);

    ArrayClear(&currentAccessibleModules);

    return SUCCESS_RESULT;
}

Result ResolverPass(const AST* ast)
{
    currentAccessibleModules = AllocateArray(sizeof(Map*));

    modules = AllocateMap(sizeof(Map));
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(VisitModule(node->ptr));
    }

    FreeArray(&currentAccessibleModules);

    for (MAP_ITERATE(i, &modules))
        FreeMap(i->value);
    FreeMap(&modules);

    return SUCCESS_RESULT;
}
