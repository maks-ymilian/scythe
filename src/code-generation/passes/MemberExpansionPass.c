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

static VarDeclStmt* GetVarDeclFromMemberAccessValue(const NodePtr value)
{
	switch (value.type)
	{
	case Node_Literal:
		const LiteralExpr* literal = value.ptr;
		if (literal->type != Literal_Identifier)
			return NULL;
		if (literal->identifier.reference.type != Node_VariableDeclaration)
			return NULL;
		return literal->identifier.reference.ptr;

	case Node_FunctionCall:
	case Node_ArrayAccess:
		return NULL;
	default: INVALID_VALUE(value.type);
	}
}

static IdentifierReference* GetIdentifier(const MemberAccessExpr* memberAccess)
{
	if (memberAccess->value.type != Node_Literal)
		return NULL;

	LiteralExpr* literal = memberAccess->value.ptr;
	if (literal->type != Literal_Identifier)
		return NULL;

	return &literal->identifier;
}

static void UpdateStructMemberAccess(const NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = memberAccessNode.ptr;
	const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL || GetStructDecl(varDecl->type) == NULL)
		return;

	const MemberAccessExpr* next = memberAccess;
	while (true)
	{
		if (next->next.ptr == NULL)
			return;

		assert(next->next.type == Node_MemberAccess);
		next = next->next.ptr;

		const VarDeclStmt* nextVarDecl = GetVarDeclFromMemberAccessValue(next->value);
		assert(nextVarDecl != NULL);
		if (GetStructDecl(nextVarDecl->type) == NULL)
			break;
	}

	IdentifierReference* currentIdentifier = GetIdentifier(memberAccess);
	const IdentifierReference* nextIdentifier = GetIdentifier(next);
	assert(currentIdentifier != NULL);
	assert(nextIdentifier != NULL);

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

static StructDeclStmt* GetStructVariableFromMemberAccess(const MemberAccessExpr* memberAccess)
{
	while (memberAccess->next.ptr != NULL)
	{
		assert(memberAccess->next.type == Node_MemberAccess);
		memberAccess = memberAccess->next.ptr;
	}

	const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL)
		return NULL;

	StructDeclStmt* structDecl = GetStructDecl(varDecl->type);
	if (structDecl == NULL)
		return NULL;

	return structDecl;
}

static void AddToEnd(const NodePtr startMemberAccessNode, const NodePtr endMemberAccessNode)
{
	assert(startMemberAccessNode.type == Node_MemberAccess);
	assert(endMemberAccessNode.type == Node_MemberAccess);
	MemberAccessExpr* end = startMemberAccessNode.ptr;
	while (end->next.ptr != NULL)
	{
		assert(end->next.type == Node_MemberAccess);
		end = end->next.ptr;
	}
	end->next = endMemberAccessNode;
}

static void VisitExpression(NodePtr* node);

static void ExpandFunctionCallArguments(const NodePtr* memberAccessNode)
{
	assert(memberAccessNode->type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = memberAccessNode->ptr;
	if (memberAccess->value.type != Node_FunctionCall)
		return;

	FuncCallExpr* funcCall = memberAccess->value.ptr;
	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		const NodePtr argument = *(NodePtr*)funcCall->arguments.array[i];
		if (argument.type != Node_MemberAccess)
			continue;

		const MemberAccessExpr* memberAccess = argument.ptr;
		const StructDeclStmt* structDecl = GetStructVariableFromMemberAccess(memberAccess);
		if (structDecl == NULL)
			continue;

		ArrayRemove(&funcCall->arguments, i);
		for (size_t j = 0; j < structDecl->members.length; ++j)
		{
			NodePtr copy = CopyASTNode(argument);
			ArrayInsert(&funcCall->arguments, &copy, i);

			const NodePtr* memberNode = structDecl->members.array[j];
			assert(memberNode->type == Node_VariableDeclaration);
			VarDeclStmt* member = memberNode->ptr;

			const NodePtr end = AllocASTNode(
				&(MemberAccessExpr){
					.next = NULL_NODE,
					.value = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = -1,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(member->name),
								.reference = (NodePtr){.ptr = member, .type = Node_VariableDeclaration},
							},
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(MemberAccessExpr), Node_MemberAccess);
			AddToEnd(copy, end);
		}
		i += structDecl->members.length;

		FreeASTNode(argument);
	}

	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		NodePtr* node = funcCall->arguments.array[i];
		VisitExpression(node);
	}
}

static void VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		RemoveModuleAccess(node);
		UpdateStructMemberAccess(*node);
		ExpandFunctionCallArguments(node);

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

static bool InstantiateStructMembers(VarDeclStmt* varDecl, Array* array, const size_t index, VarDeclStmt* topLevel)
{
	const StructDeclStmt* structDecl = GetStructDecl(varDecl->type);
	if (structDecl == NULL)
		return false;

	if (topLevel == NULL)
		topLevel = varDecl;

	for (size_t i = 0; i < structDecl->members.length; ++i)
	{
		const NodePtr* node = structDecl->members.array[i];
		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* member = node->ptr;
		if (!InstantiateStructMembers(member, array, index, varDecl))
		{
			const NodePtr copy = CopyASTNode((NodePtr){.ptr = member, .type = Node_VariableDeclaration});
			ArrayInsert(array, &copy, index + 1);
			ArrayAdd(&topLevel->instantiatedVariables, &copy.ptr);
		}
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
			if (InstantiateStructMembers(varDecl, &block->statements, i, NULL))
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
				if (InstantiateStructMembers(varDecl, &funcDecl->parameters, i, NULL))
				{
					ArrayAdd(&nodesToDelete, node);
					ArrayRemove(&funcDecl->parameters, i);
					i--;
				}
			}
			assert(funcDecl->block.type == Node_Block);
			VisitBlock(funcDecl->block.ptr);
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
