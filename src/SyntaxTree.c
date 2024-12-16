#include "SyntaxTree.h"

#include <assert.h>
#include "stdlib.h"
#include <string.h>

NodePtr AllocASTNode(const void* node, const size_t size, const NodeType type)
{
    void* out = malloc(size);
    memcpy(out, node, size);
    return (NodePtr){.ptr = out, .type = type};
}

static void FreeNode(const NodePtr node)
{
    switch (node.type)
    {
    case Node_Null: return;

    case Node_Binary:
    {
        const BinaryExpr* ptr = node.ptr;
        FreeNode(ptr->left);
        FreeNode(ptr->right);
        break;
    }
    case Node_Unary:
    {
        const UnaryExpr* ptr = node.ptr;
        FreeNode(ptr->expression);
        break;
    }
    case Node_Literal:
    {
        const LiteralExpr* ptr = node.ptr;
        if (ptr->type == Literal_Identifier) free(ptr->identifier.text);
        if (ptr->type == Literal_String) free(ptr->string);
        break;
    }
    case Node_FunctionCall:
    {
        const FuncCallExpr* ptr = node.ptr;
        free(ptr->identifier.text);
        for (int i = 0; i < ptr->parameters.length; ++i)
            FreeNode(*(NodePtr*)ptr->parameters.array[i]);
        FreeArray(&ptr->parameters);
        break;
    }
    case Node_ArrayAccess:
    {
        const ArrayAccessExpr* ptr = node.ptr;
        free(ptr->identifier.text);
        FreeNode(ptr->subscript);
        break;
    }
    case Node_MemberAccess:
    {
        const MemberAccessExpr* ptr = node.ptr;
        FreeNode(ptr->value);
        FreeNode(ptr->next);
        break;
    }

    case Node_ExpressionStatement:
    {
        const ExpressionStmt* ptr = node.ptr;
        FreeNode(ptr->expr);
        break;
    }

    case Node_Import:
    {
        const ImportStmt* ptr = node.ptr;
        free(ptr->file);
        break;
    }
    case Node_Section:
    {
        const SectionStmt* ptr = node.ptr;
        FreeNode(ptr->block);
        break;
    }
    case Node_VariableDeclaration:
    {
        const VarDeclStmt* ptr = node.ptr;
        free(ptr->typeName);
        free(ptr->name);
        free(ptr->externalName);
        FreeNode(ptr->initializer);
        FreeNode(ptr->arrayLength);
        break;
    }
    case Node_FunctionDeclaration:
    {
        const FuncDeclStmt* ptr = node.ptr;
        free(ptr->typeName);
        free(ptr->name);
        free(ptr->externalName);
        for (int i = 0; i < ptr->parameters.length; ++i)
            FreeNode(*(NodePtr*)ptr->parameters.array[i]);
        FreeArray(&ptr->parameters);
        break;
    }
    case Node_StructDeclaration:
    {
        const StructDeclStmt* ptr = node.ptr;
        free(ptr->name);
        for (int i = 0; i < ptr->members.length; ++i)
            FreeNode(*(NodePtr*)ptr->members.array[i]);
        FreeArray(&ptr->members);
        break;
    }

    case Node_Block:
    {
        const BlockStmt* ptr = node.ptr;
        for (int i = 0; i < ptr->statements.length; ++i)
            FreeNode(*(NodePtr*)ptr->statements.array[i]);
        FreeArray(&ptr->statements);
        break;
    }

    case Node_If:
    {
        const IfStmt* ptr = node.ptr;
        FreeNode(ptr->expr);
        FreeNode(ptr->trueStmt);
        FreeNode(ptr->falseStmt);
        break;
    }
    case Node_While:
    {
        const WhileStmt* ptr = node.ptr;
        FreeNode(ptr->expr);
        FreeNode(ptr->stmt);
        break;
    }
    case Node_For:
    {
        const ForStmt* ptr = node.ptr;
        FreeNode(ptr->condition);
        FreeNode(ptr->initialization);
        FreeNode(ptr->increment);
        FreeNode(ptr->stmt);
        break;
    }
    case Node_LoopControl: break;

    case Node_Return:
    {
        const ReturnStmt* ptr = node.ptr;
        FreeNode(ptr->expr);
        break;
    }

    case Node_Module:
    {
        const ModuleNode* ptr = node.ptr;
        for (int i = 0; i < ptr->statements.length; ++i)
            FreeNode(*(NodePtr*)ptr->statements.array[i]);
        free(ptr->path);
        free(ptr->name);
        break;
    }

    default: assert(0);
    }

    free(node.ptr);
}

void FreeAST(const AST root)
{
    for (int i = 0; i < root.nodes.length; ++i)
    {
        const NodePtr* node = root.nodes.array[i];
        FreeNode(*node);
    }
}
