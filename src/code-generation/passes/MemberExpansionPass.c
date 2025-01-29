#include "MemberExpansionPass.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"

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

	default: INVALID_VALUE(type.type);
	}
}


static VarDeclStmt* GetStructVarDeclFromMemberAccessValue(const NodePtr value)
{
	switch (value.type)
	{
	case Node_Literal:
		const LiteralExpr* literal = value.ptr;
		if (literal->type != Literal_Identifier)
			return NULL;
		if (literal->identifier.reference.type != Node_VariableDeclaration)
			return NULL;
		VarDeclStmt* varDecl = literal->identifier.reference.ptr;
		if (GetStructDecl(varDecl->type) == NULL)
			return NULL;
		return varDecl;

	case Node_FunctionCall:
	case Node_ArrayAccess:
		return NULL;
	default: INVALID_VALUE(value.type);
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

static void UpdateStructMemberAccess(const NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = memberAccessNode.ptr;
	const VarDeclStmt* varDecl = GetStructVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL)
		return;

	IdentifierReference* currentIdentifier = GetIdentifier(memberAccessNode);
	const IdentifierReference* nextIdentifier = GetIdentifier(memberAccess->next);

	VarDeclStmt* instantiated = FindInstantiated(nextIdentifier->text, varDecl);
	assert(instantiated != NULL);
	currentIdentifier->reference = (NodePtr){.ptr = instantiated, .type = Node_VariableDeclaration};
	free(currentIdentifier->text);
	currentIdentifier->text = AllocateString(nextIdentifier->text);

	FreeASTNode(memberAccess->next);
	memberAccess->next = NULL_NODE;
}

static ImportStmt* GetImportStmtFromMemberAccessValue(const NodePtr value)
{
	switch (value.type)
	{
	case Node_Literal:
		const LiteralExpr* literal = value.ptr;
		if (literal->type != Literal_Identifier)
			return NULL;
		if (literal->identifier.reference.type != Node_Import)
			return NULL;
		return literal->identifier.reference.ptr;

	case Node_FunctionCall:
	case Node_ArrayAccess:
		return NULL;
	default: INVALID_VALUE(value.type);
	}
}

static void RemoveModuleAccess(NodePtr* memberAccessNode)
{
	assert(memberAccessNode->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = memberAccessNode->ptr;
	if (GetImportStmtFromMemberAccessValue(memberAccess->value) == NULL)
		return;

	const NodePtr next = memberAccess->next;
	memberAccess->next = NULL_NODE;
	FreeASTNode(*memberAccessNode);
	*memberAccessNode = next;
}

static void VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		RemoveModuleAccess(node);
		UpdateStructMemberAccess(*node);

		MemberAccessExpr* memberAccess = node->ptr;
		assert(memberAccess->next.ptr == NULL);

		const NodePtr value = memberAccess->value;
		memberAccess->value = NULL_NODE;

		FreeASTNode(*node);
		*node = value;
		break;
	case Node_Binary:
		BinaryExpr* binary = node->ptr;
		VisitExpression(&binary->left);
		VisitExpression(&binary->right);
		break;
	case Node_Unary:
		UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression);
		break;
	case Node_Literal:
		const LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_Int ||
			   literal->type == Literal_Float ||
			   literal->type == Literal_String ||
			   literal->type == Literal_Boolean ||
			   literal->type == Literal_PrimitiveType);
		break;
	default: INVALID_VALUE(node->type);
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

static void VisitBlock(BlockStmt* block)
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
			ExpressionStmt* exprStmt = node->ptr;
			VisitExpression(&exprStmt->expr);
			break;
		case Node_FunctionDeclaration:
			FuncDeclStmt* funcDecl = node->ptr;
			for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			{
				const NodePtr* node = funcDecl->parameters.array[i];
				assert(node->type == Node_VariableDeclaration);
				VarDeclStmt* varDecl = node->ptr;
				if (InstantiateStructMembers(varDecl, &funcDecl->parameters, i))
				{
					ArrayAdd(&nodesToDelete, node);
					ArrayRemove(&funcDecl->parameters, i);
					i--;
				}
			}
			break;
		default: INVALID_VALUE(node->type);
		}
	}
}

static void VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_Block);
	VisitBlock(section->block.ptr);
}

static void VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			VisitSection(stmt->ptr);
			break;
		case Node_Import:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}
}

void MemberExpansionPass(const AST* ast)
{
	nodesToDelete = AllocateArray(sizeof(NodePtr));

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		VisitModule(node->ptr);
	}

	for (size_t i = 0; i < nodesToDelete.length; ++i)
	{
		const NodePtr* node = nodesToDelete.array[i];
		FreeASTNode(*node);
	}
	FreeArray(&nodesToDelete);
}
