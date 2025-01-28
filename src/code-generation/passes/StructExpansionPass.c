#include "StructExpansionPass.h"

#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"
#include "data-structures/Map.h"

static Array nodesToDelete;

static const char* currentFilePath = NULL;

static VarDeclStmt* FindInstantiated(const char* name, const VarDeclStmt* structVarDecl)
{
	for (size_t i = 0; i < structVarDecl->instantiatedVariables.length; ++i)
	{
		VarDeclStmt** varDecl = structVarDecl->instantiatedVariables.array[i];
		if (strcmp((*varDecl)->name, name) == 0)
			return *varDecl;
	}
	return NULL;
}

static StructDeclStmt* GetStructDecl(const NodePtr type)
{
	switch (type.type)
	{
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = type.ptr;
		while (memberAccess->next.ptr != NULL)
		{
			assert(memberAccess->next.type == Node_MemberAccess);
			memberAccess = memberAccess->next.ptr;
		}
		assert(memberAccess->value.type == Node_Literal);
		const LiteralExpr* literal = memberAccess->value.ptr;
		assert(literal->type == Literal_Identifier);
		assert(literal->identifier.reference.type == Node_StructDeclaration);
		return literal->identifier.reference.ptr;

	case Node_Literal:
		return NULL;

	default: unreachable();
	}
}


static bool GetStructVarDeclFromMemberAccessValue(const NodePtr value, VarDeclStmt** out)
{
	switch (value.type)
	{
	case Node_Literal:
		const LiteralExpr* literal = value.ptr;
		if (literal->type != Literal_Identifier)
			return false;
		if (literal->identifier.reference.type != Node_VariableDeclaration)
			return false;
		VarDeclStmt* varDecl = literal->identifier.reference.ptr;
		if (GetStructDecl(varDecl->type) == NULL)
			return false;
		*out = varDecl;
		return true;

	case Node_FunctionCall:
	case Node_ArrayAccess:
		return false;
	default: unreachable();
	}
}

static IdentifierReference* GetIdentifier(const NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = memberAccessNode.ptr;
	assert(memberAccess->value.type == Node_Literal);
	LiteralExpr* literal = memberAccess->value.ptr;
	assert(literal->type == Literal_Identifier);
	return &literal->identifier;
}

static Result VisitExpression(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		MemberAccessExpr* memberAccess = node->ptr;
		VarDeclStmt* varDecl = NULL;
		while (!GetStructVarDeclFromMemberAccessValue(memberAccess->value, &varDecl))
		{
			if (memberAccess->next.ptr == NULL)
				break;

			assert(memberAccess->next.type == Node_MemberAccess);
			memberAccess = memberAccess->next.ptr;
		}
		if (varDecl == NULL)
			break;

		IdentifierReference* currentIdentifier = GetIdentifier(*node);
		IdentifierReference* nextIdentifier = GetIdentifier(memberAccess->next);

		VarDeclStmt* instantiated = FindInstantiated(nextIdentifier->text, varDecl);
		assert(instantiated != NULL);
		currentIdentifier->reference = (NodePtr){.ptr = instantiated, .type = Node_VariableDeclaration};
		free(currentIdentifier->text);
		currentIdentifier->text = AllocateString(nextIdentifier->text);

		FreeASTNode(memberAccess->next);
		memberAccess->next = NULL_NODE;
		break;

	case Node_Binary:
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&binary->left));
		PROPAGATE_ERROR(VisitExpression(&binary->right));
		break;
	case Node_Unary:
		const UnaryExpr* unary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&unary->expression));
		break;
	case Node_Literal:
		// const LiteralExpr* literal = node->ptr;
		break;
	case Node_FunctionCall:
		const FuncCallExpr* funcCall = node->ptr;
		break;
	case Node_ArrayAccess:
		const ArrayAccessExpr* arrayAccess = node->ptr;
		break;

	default: unreachable();
	}
	return SUCCESS_RESULT;
}

static VarDeclStmt* GetStructMember(const StructDeclStmt* structDecl, const char* name)
{
	for (size_t i = 0; i < structDecl->members.length; ++i)
	{
		const NodePtr* node = structDecl->members.array[i];
		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = node->ptr;
		if (strcmp(name, varDecl->name) == 0)
			return varDecl;
	}
	unreachable();
}

static char* GetText(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal: return ((LiteralExpr*)node.ptr)->identifier.text;
	case Node_FunctionCall: return ((FuncCallExpr*)node.ptr)->identifier.text;
	case Node_ArrayAccess: return ((ArrayAccessExpr*)node.ptr)->identifier.text;

	default: unreachable();
	}
}

static void InstantiateStructMember(VarDeclStmt* structVarDecl, VarDeclStmt* memberDecl, Array* array, const size_t index)
{
	const NodePtr copy = CopyASTNode((NodePtr){.ptr = memberDecl, .type = Node_VariableDeclaration});
	ArrayInsert(array, &copy, index + 1);
	ArrayAdd(&structVarDecl->instantiatedVariables, &copy.ptr);
}

static bool InstantiateStructMembers(VarDeclStmt* varDecl, Array* array, const size_t index)
{
	const StructDeclStmt* structDecl = GetStructDecl(varDecl->type);
	if (structDecl == NULL)
		return false;

	for (size_t i = 0; i < structDecl->members.length; ++i)
	{
		const NodePtr* node = structDecl->members.array[i];
		assert(node->type == Node_VariableDeclaration);
		InstantiateStructMember(varDecl, node->ptr, array, index);
	}

	return true;
}

static Result VisitBlock(BlockStmt* block)
{
	for (size_t i = 0; i < block->statements.length; ++i)
	{
		const NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_VariableDeclaration:
			VarDeclStmt* varDecl = node->ptr;
			if (InstantiateStructMembers(varDecl, &block->statements, i))
			{
				ArrayAdd(&nodesToDelete, node);
				ArrayRemove(&block->statements, i);
				i--;
			}
			break;
		case Node_StructDeclaration:
			ArrayAdd(&nodesToDelete, node);
			ArrayRemove(&block->statements, i);
			i--;
			break;
		case Node_ExpressionStatement:
			const ExpressionStmt* exprStmt = node->ptr;
			PROPAGATE_ERROR(VisitExpression(&exprStmt->expr));
			break;

			// case Node_FunctionDeclaration:
			// 	const FuncDeclStmt* funcDecl = node->ptr;
			// 	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			// 	{
			// 	}
			// 	break;
			// case Node_Block:
			// 	break;
			//
			// case Node_If:
			// 	break;
			// case Node_While:
			// 	break;
			// case Node_For:
			// 	break;
			// case Node_LoopControl:
			// 	break;
			//
			// case Node_Return:

			// todo
		default: break; unreachable();
		}
	}

	return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_Block);
	PROPAGATE_ERROR(VisitBlock(section->block.ptr));
	return SUCCESS_RESULT;
}

static Result VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			PROPAGATE_ERROR(VisitSection(stmt->ptr));
			break;
		case Node_Import:
			break;
		default: unreachable();
		}
	}
	return SUCCESS_RESULT;
}

Result StructExpansionPass(const AST* ast)
{
	nodesToDelete = AllocateArray(sizeof(NodePtr));

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	for (size_t i = 0; i < nodesToDelete.length; ++i)
	{
		const NodePtr* node = nodesToDelete.array[i];
		FreeASTNode(*node);
	}
	FreeArray(&nodesToDelete);

	return SUCCESS_RESULT;
}
