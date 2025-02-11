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

static StructDeclStmt* GetStructDeclFromType(const NodePtr type)
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
	if (varDecl == NULL || GetStructDeclFromType(varDecl->type) == NULL)
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
		if (GetStructDeclFromType(nextVarDecl->type) == NULL)
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
	if (memberAccess == NULL)
		return NULL;

	while (memberAccess->next.ptr != NULL)
	{
		assert(memberAccess->next.type == Node_MemberAccess);
		memberAccess = memberAccess->next.ptr;
	}

	const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL)
		return NULL;

	StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
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

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, void* data);
static size_t ForEachStructMember(const StructDeclStmt* structDecl, const StructMemberFunc func, void* data)
{
	assert(structDecl != NULL);

	size_t totalMembers = 0;
	for (size_t i = structDecl->members.length - 1; i + 1 >= 1; --i)
	{
		const NodePtr* memberNode = structDecl->members.array[i];
		assert(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		const StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
		if (structDecl == NULL)
		{
			func(varDecl, data);
			totalMembers++;
		}
		else
			totalMembers += ForEachStructMember(structDecl, func, data);
	}
	return totalMembers;
}

typedef struct
{
	NodePtr argumentNode;
	FuncCallExpr* funcCall;
	size_t argumentIndex;
} ExpandArgumentData;

static void ExpandArgument(VarDeclStmt* member, void* data)
{
	const ExpandArgumentData* d = data;

	const NodePtr copy = CopyASTNode(d->argumentNode);
	ArrayInsert(&d->funcCall->arguments, &copy, d->argumentIndex);

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

static Result VisitExpression(NodePtr* node);

static Result ExpandFunctionCallArguments(const NodePtr* memberAccessNode)
{
	assert(memberAccessNode->type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = memberAccessNode->ptr;
	if (memberAccess->value.type != Node_FunctionCall)
		return SUCCESS_RESULT;

	FuncCallExpr* funcCall = memberAccess->value.ptr;
	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		const NodePtr argument = *(NodePtr*)funcCall->arguments.array[i];
		if (argument.type == Node_Literal)
			continue;
		assert(argument.type == Node_MemberAccess); // todo a function call or array access could still return a struct

		const MemberAccessExpr* memberAccess = argument.ptr;
		const StructDeclStmt* structDecl = GetStructVariableFromMemberAccess(memberAccess);
		if (structDecl == NULL)
			continue;

		ArrayRemove(&funcCall->arguments, i);
		i += ForEachStructMember(structDecl, ExpandArgument,
			&(ExpandArgumentData){
				.argumentNode = argument,
				.funcCall = funcCall,
				.argumentIndex = i,
			});
		i--;
		FreeASTNode(argument);
	}

	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		NodePtr* node = funcCall->arguments.array[i];
		PROPAGATE_ERROR(VisitExpression(node));
	}

	return SUCCESS_RESULT;
}

static StructDeclStmt* GetStructDecl(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
		const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(node);
		if (varDecl != NULL)
			return GetStructDeclFromType(varDecl->type);
		return NULL;

	case Node_FunctionCall:
		const FuncCallExpr* funcCall = node.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;
		return GetStructDeclFromType(funcDecl->type);

	default: return NULL;
	}
}

typedef struct
{
	VarDeclStmt* leftVarDecl;
	VarDeclStmt* rightVarDecl;
	Array* statements;
} AssignmentThingData;

static void AssignmentThing(VarDeclStmt* member, void* data)
{
	const AssignmentThingData* d = data;
	VarDeclStmt* leftInstance = FindInstantiated(member->name, d->leftVarDecl);
	assert(leftInstance != NULL);
	VarDeclStmt* rightInstance = FindInstantiated(member->name, d->rightVarDecl);
	assert(rightInstance != NULL);

	const NodePtr statement = AllocASTNode(
		&(ExpressionStmt){
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = -1,
					.operatorType = Binary_Assignment,
					.left = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = -1,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(leftInstance->name),
								.reference = (NodePtr){.ptr = leftInstance, .type = Node_VariableDeclaration},
							},
						},
						sizeof(LiteralExpr), Node_Literal),
					.right = AllocASTNode( // clang-format is bad
						&(LiteralExpr){
							.lineNumber = -1,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(rightInstance->name),
								.reference = (NodePtr){.ptr = rightInstance, .type = Node_VariableDeclaration},
							},
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
	ArrayAdd(d->statements, &statement);
}

static Result VisitBinaryExpression(NodePtr* node)
{
	// todo add pass to break up chained assignment and equality

	assert(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;
	PROPAGATE_ERROR(VisitExpression(&binary->left));
	PROPAGATE_ERROR(VisitExpression(&binary->right));

	// it could be a member access with more than 1 values all structs
	// if a member access is found then assert that the corresponding struct decl is also found
	assert(binary->left.type != Node_MemberAccess);
	assert(binary->right.type != Node_MemberAccess);

	const StructDeclStmt* structDecl;
	{
		const StructDeclStmt* leftStruct = GetStructDecl(binary->left);
		const StructDeclStmt* rightStruct = GetStructDecl(binary->right);

		if (leftStruct == NULL && rightStruct == NULL)
			return SUCCESS_RESULT;

		if (leftStruct == NULL || rightStruct == NULL)
			return ERROR_RESULT(
				AllocateString1Str(
					"Cannot use operator \"%s\" on a struct type and a non-struct type",
					GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
				binary->lineNumber, currentFilePath);

		if (leftStruct != rightStruct)
			return ERROR_RESULT(
				AllocateString1Str(
					"Cannot use operator \"%s\" on two different struct types",
					GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
				binary->lineNumber, currentFilePath);

		structDecl = leftStruct;
	}

	switch (binary->operatorType)
	{
	case Binary_Assignment:
		if (binary->left.type != Node_Literal)
			return ERROR_RESULT("Left operand of struct assignment must be a variable", binary->lineNumber, currentFilePath);

		assert(binary->right.type == Node_Literal); // todo function assignment

		Array statements = AllocateArray(sizeof(NodePtr));

		VarDeclStmt* leftVarDecl = GetVarDeclFromMemberAccessValue(binary->left);
		assert(leftVarDecl != NULL);
		VarDeclStmt* rightVarDecl = GetVarDeclFromMemberAccessValue(binary->right);
		assert(rightVarDecl != NULL);

		ForEachStructMember(structDecl,
			AssignmentThing,
			&(AssignmentThingData){
				.statements = &statements,
				.leftVarDecl = leftVarDecl,
				.rightVarDecl = rightVarDecl,
			});

		const NodePtr block = AllocASTNode(
			&(BlockExpr){
				.type = AllocASTNode(
					&(LiteralExpr){
						.lineNumber = -1,
						.type = Literal_PrimitiveType,
						.primitiveType = Primitive_Void,
					},
					sizeof(LiteralExpr), Node_Literal),
				.block = AllocASTNode(
					&(BlockStmt){
						.statements = statements,
						.lineNumber = -1,
					},
					sizeof(BlockStmt), Node_BlockStatement)},
			sizeof(BlockExpr), Node_BlockExpression);

		FreeASTNode(*node);
		*node = block;
		break;
	case Binary_IsEqual:
		assert(!"is equal");
		break;
	case Binary_NotEqual:
		assert(!"not equal");
		break;
	default: return ERROR_RESULT(
		AllocateString1Str(
			"Cannot use operator \"%s\" on struct types",
			GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
		binary->lineNumber, currentFilePath);
	}

	return SUCCESS_RESULT;

	// check if the expressions are struct types
	// check the operator type
	// only let it get through if member access or struct type was failed to get rid of
	// so if the types are incompatible or the wrong operator was used then return an error
}

static Result VisitStatement(NodePtr* node);

static Result VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		RemoveModuleAccess(node);
		UpdateStructMemberAccess(*node);
		PROPAGATE_ERROR(ExpandFunctionCallArguments(node));

		MemberAccessExpr* memberAccess = node->ptr;
		if (memberAccess->next.ptr != NULL)
			break;

		const NodePtr value = memberAccess->value;
		memberAccess->value = NULL_NODE;

		FreeASTNode(*node);
		*node = value;
		break;
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node));
		break;
	case Node_Unary:
		UnaryExpr* unary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&unary->expression));
		assert(!"unary"); // todo implement
		break;
	case Node_Literal:
		const LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_Int ||
			   literal->type == Literal_Float ||
			   literal->type == Literal_String ||
			   literal->type == Literal_Boolean ||
			   literal->type == Literal_PrimitiveType);
		break;
	case Node_BlockExpression:
		BlockExpr* block = node->ptr;
		assert(block->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitStatement(&block->block));
		break;
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

typedef struct
{
	Array* array;
	size_t index;
	VarDeclStmt* topLevel;
} InstantiateMemberData;

static void InstantiateMember(VarDeclStmt* varDecl, void* data)
{
	const InstantiateMemberData* d = data;

	const NodePtr copy = CopyASTNode((NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration});
	ArrayInsert(d->array, &copy, d->index);
	ArrayAdd(&d->topLevel->instantiatedVariables, &copy.ptr);
}

static bool InstantiateStructMembers(VarDeclStmt* varDecl, Array* array, const size_t index)
{
	const StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
	if (structDecl == NULL)
		return false;

	ForEachStructMember(structDecl,
		InstantiateMember,
		&(InstantiateMemberData){
			.array = array,
			.index = index,
			.topLevel = varDecl,
		});

	return true;
}

static Result VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
		VarDeclStmt* varDecl = node->ptr;

		if (varDecl->initializer.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&varDecl->initializer));

		Array statements = AllocateArray(sizeof(NodePtr));
		if (InstantiateStructMembers(varDecl, &statements, 0))
		{
			ArrayAdd(&nodesToDelete, node);
			*node = AllocASTNode(
				&(BlockStmt){
					.statements = statements,
					.lineNumber = varDecl->lineNumber,
				},
				sizeof(BlockStmt), Node_BlockStatement);
		}
		else
			FreeArray(&statements);

		break;
	case Node_StructDeclaration:
		ArrayAdd(&nodesToDelete, node);
		*node = NULL_NODE;
		break;
	case Node_ExpressionStatement:
		ExpressionStmt* exprStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&exprStmt->expr));
		break;
	case Node_FunctionDeclaration:
		FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			const NodePtr* node = funcDecl->parameters.array[i];
			assert(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			assert(varDecl->initializer.ptr == NULL);
			if (InstantiateStructMembers(varDecl, &funcDecl->parameters, i + 1))
			{
				ArrayAdd(&nodesToDelete, node);
				ArrayRemove(&funcDecl->parameters, i);
				i--;
			}
		}
		assert(funcDecl->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
		break;
	case Node_BlockStatement:
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		break;
	case Node_If:
		IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr));
		break;
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}

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
			SectionStmt* section = stmt->ptr;
			PROPAGATE_ERROR(VisitStatement(&section->block));
			break;
		case Node_Import:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}

	return SUCCESS_RESULT;
}

Result MemberExpansionPass(const AST* ast)
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
