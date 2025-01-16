#include "SyntaxTree.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

NodePtr AllocASTNode(const void* node, const size_t size, const NodeType type)
{
	void* out = malloc(size);
	memcpy(out, node, size);
	return (NodePtr){.ptr = out, .type = type};
}

NodePtr CopyASTNode(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Binary:
	{
		BinaryExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(BinaryExpr), Node_Binary);
		assert(copy.type == Node_Binary);
		ptr = copy.ptr;

		ptr->left = CopyASTNode(ptr->left);
		ptr->right = CopyASTNode(ptr->right);

		return copy;
	}
	case Node_Unary:
	{
		UnaryExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(UnaryExpr), Node_Unary);
		assert(copy.type == Node_Unary);
		ptr = copy.ptr;

		ptr->expression = CopyASTNode(ptr->expression);

		return copy;
	}
	case Node_Literal:
	{
		LiteralExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(LiteralExpr), Node_Literal);
		assert(copy.type == Node_Literal);
		ptr = copy.ptr;

		if (ptr->type == Literal_Float) ptr->floatValue = strdup(ptr->floatValue);
		if (ptr->type == Literal_Identifier) ptr->identifier.text = strdup(ptr->identifier.text);
		if (ptr->type == Literal_String) ptr->string = strdup(ptr->string);

		return copy;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(FuncCallExpr), Node_FunctionCall);
		assert(copy.type == Node_FunctionCall);
		ptr = copy.ptr;

		ptr->identifier.text = strdup(ptr->identifier.text);

		Array arguments = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->arguments.length; ++i)
		{
			const NodePtr* node = ptr->arguments.array[i];
			const NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&arguments, &copy);
		}
		ptr->arguments = arguments;

		return copy;
	}
	case Node_ArrayAccess:
	{
		ArrayAccessExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(ArrayAccessExpr), Node_ArrayAccess);
		assert(copy.type == Node_ArrayAccess);
		ptr = copy.ptr;

		ptr->subscript = CopyASTNode(ptr->subscript);

		ptr->identifier.text = strdup(ptr->identifier.text);

		return copy;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(MemberAccessExpr), Node_MemberAccess);
		assert(copy.type == Node_Binary);
		ptr = copy.ptr;

		ptr->next = CopyASTNode(ptr->next);
		ptr->value = CopyASTNode(ptr->value);

		return copy;
	}

	case Node_ExpressionStatement:
	case Node_Import:
	case Node_Section:
	case Node_VariableDeclaration:
	case Node_FunctionDeclaration:
	case Node_StructDeclaration:
	case Node_Block:
	case Node_If:
	case Node_While:
	case Node_For:
	case Node_LoopControl:
	case Node_Return:
	default: unreachable();
	}
}

void FreeASTNode(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Null: return;

	case Node_Binary:
	{
		const BinaryExpr* ptr = node.ptr;
		FreeASTNode(ptr->left);
		FreeASTNode(ptr->right);
		break;
	}
	case Node_Unary:
	{
		const UnaryExpr* ptr = node.ptr;
		FreeASTNode(ptr->expression);
		break;
	}
	case Node_Literal:
	{
		const LiteralExpr* ptr = node.ptr;
		if (ptr->type == Literal_Identifier) free(ptr->identifier.text);
		if (ptr->type == Literal_String) free(ptr->string);
		if (ptr->type == Literal_Float) free(ptr->floatValue);
		break;
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* ptr = node.ptr;
		free(ptr->identifier.text);
		for (size_t i = 0; i < ptr->arguments.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->arguments.array[i]);
		FreeArray(&ptr->arguments);
		break;
	}
	case Node_ArrayAccess:
	{
		const ArrayAccessExpr* ptr = node.ptr;
		free(ptr->identifier.text);
		FreeASTNode(ptr->subscript);
		break;
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* ptr = node.ptr;
		FreeASTNode(ptr->value);
		FreeASTNode(ptr->next);
		break;
	}

	case Node_ExpressionStatement:
	{
		const ExpressionStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		break;
	}

	case Node_Import:
	{
		const ImportStmt* ptr = node.ptr;
		free(ptr->path);
		free(ptr->moduleName);
		break;
	}
	case Node_Section:
	{
		const SectionStmt* ptr = node.ptr;
		FreeASTNode(ptr->block);
		break;
	}
	case Node_VariableDeclaration:
	{
		const VarDeclStmt* ptr = node.ptr;
		free(ptr->name);
		free(ptr->externalName);
		FreeASTNode(ptr->type);
		FreeASTNode(ptr->initializer);
		FreeASTNode(ptr->arrayLength);
		break;
	}
	case Node_FunctionDeclaration:
	{
		const FuncDeclStmt* ptr = node.ptr;
		free(ptr->name);
		free(ptr->externalName);
		FreeASTNode(ptr->type);
		for (size_t i = 0; i < ptr->parameters.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->parameters.array[i]);
		FreeArray(&ptr->parameters);
		break;
	}
	case Node_StructDeclaration:
	{
		const StructDeclStmt* ptr = node.ptr;
		free(ptr->name);
		for (size_t i = 0; i < ptr->members.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->members.array[i]);
		FreeArray(&ptr->members);
		break;
	}

	case Node_Block:
	{
		const BlockStmt* ptr = node.ptr;
		for (size_t i = 0; i < ptr->statements.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->statements.array[i]);
		FreeArray(&ptr->statements);
		break;
	}

	case Node_If:
	{
		const IfStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		FreeASTNode(ptr->trueStmt);
		FreeASTNode(ptr->falseStmt);
		break;
	}
	case Node_While:
	{
		const WhileStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		FreeASTNode(ptr->stmt);
		break;
	}
	case Node_For:
	{
		const ForStmt* ptr = node.ptr;
		FreeASTNode(ptr->condition);
		FreeASTNode(ptr->initialization);
		FreeASTNode(ptr->increment);
		FreeASTNode(ptr->stmt);
		break;
	}
	case Node_LoopControl: break;

	case Node_Return:
	{
		const ReturnStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		break;
	}

	case Node_Module:
	{
		const ModuleNode* ptr = node.ptr;
		for (size_t i = 0; i < ptr->statements.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->statements.array[i]);
		free(ptr->path);
		free(ptr->moduleName);
		break;
	}

	default: unreachable();
	}

	free(node.ptr);
}

void FreeAST(const AST root)
{
	for (size_t i = 0; i < root.nodes.length; ++i)
	{
		const NodePtr* node = root.nodes.array[i];
		FreeASTNode(*node);
	}
}
