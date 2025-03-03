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
	case Node_Null:
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
		const FuncCallExpr* funcCall = value.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;
		return funcDecl->globalReturn;

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
	if (memberAccess->value.type != Node_Literal)
		return;
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

static StructDeclStmt* GetStructDeclFromMemberAccess(const MemberAccessExpr* memberAccess)
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

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, size_t index, void* data);
static size_t ForEachStructMember(const StructDeclStmt* structDecl, const StructMemberFunc func, void* data, size_t* currentIndex)
{
	assert(structDecl != NULL);

	size_t index = 0;
	if (currentIndex == NULL)
		currentIndex = &index;

	for (size_t i = 0; i < structDecl->members.length; i++)
	{
		const NodePtr* memberNode = structDecl->members.array[i];
		assert(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		const StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
		if (structDecl == NULL)
		{
			func(varDecl, *currentIndex, data);
			(*currentIndex)++;
		}
		else
			ForEachStructMember(structDecl, func, data, currentIndex);
	}

	return *currentIndex;
}

static NodePtr AllocLiteralIdentifier(VarDeclStmt* varDecl, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_Identifier,
			.identifier = (IdentifierReference){
				.text = AllocateString(varDecl->name),
				.reference = (NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration},
			},
		},
		sizeof(LiteralExpr), Node_Literal);
}

static StructDeclStmt* GetStructDecl(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	{
		const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(node);
		if (varDecl != NULL)
			return GetStructDeclFromType(varDecl->type);
		return NULL;
	}

	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = node.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

		StructDeclStmt* structDecl = GetStructDeclFromType(funcDecl->oldType);
		if (structDecl != NULL)
			return structDecl;

		return GetStructDeclFromType(funcDecl->type);
	}

	case Node_MemberAccess:
	{
		const MemberAccessExpr* memberAccess = node.ptr;
		while (memberAccess->next.ptr != NULL)
		{
			assert(memberAccess->next.type == Node_MemberAccess);
			memberAccess = memberAccess->next.ptr;
		}
		const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
		if (varDecl != NULL)
			return GetStructDeclFromType(varDecl->type);
		return NULL;
	}

	default: return NULL;
	}
}

static Result CheckTypeConversion(const StructDeclStmt* from, const StructDeclStmt* to, const int lineNumber)
{
	if (from == NULL && to == NULL)
		assert(0);

	if (from == NULL && to != NULL)
		return ERROR_RESULT(
			"Cannot convert a non-struct type to a struct type",
			lineNumber, currentFilePath);

	if (from != NULL && to == NULL)
		return ERROR_RESULT(
			"Cannot convert a struct type to a non-struct type",
			lineNumber, currentFilePath);

	if (from != to)
		return ERROR_RESULT(
			AllocateString2Str(
				"Cannot convert struct type \"%s\" to struct type \"%s\"",
				from->name,
				to->name),
			lineNumber, currentFilePath);

	return SUCCESS_RESULT;
}

typedef struct
{
	NodePtr argumentNode;
	FuncCallExpr* funcCall;
	size_t argumentIndex;
} ExpandArgumentData;

static void ExpandArgument(VarDeclStmt* member, size_t index, void* data)
{
	const ExpandArgumentData* d = data;

	const NodePtr copy = CopyASTNode(d->argumentNode);
	ArrayInsert(&d->funcCall->arguments, &copy, d->argumentIndex + index);

	const NodePtr end = AllocASTNode(
		&(MemberAccessExpr){
			.next = NULL_NODE,
			.value = AllocLiteralIdentifier(member, -1),
		},
		sizeof(MemberAccessExpr), Node_MemberAccess);
	AddToEnd(copy, end);
}

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement);

static Result VisitFunctionCallArguments(const NodePtr* memberAccessNode, NodePtr* containingStatement)
{
	assert(memberAccessNode->type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = memberAccessNode->ptr;
	if (memberAccess->value.type != Node_FunctionCall)
		return SUCCESS_RESULT;

	FuncCallExpr* funcCall = memberAccess->value.ptr;
	assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
	FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

	// todo add a pass so this doesnt get hit
	assert(funcDecl->oldParameters.length == funcCall->arguments.length);
	for (size_t paramIndex = 0, argIndex = 0; paramIndex < funcDecl->oldParameters.length; ++argIndex, ++paramIndex)
	{
		const NodePtr argument = *(NodePtr*)funcCall->arguments.array[argIndex];

		const NodePtr* paramNode = funcDecl->oldParameters.array[paramIndex];
		assert(paramNode->type == Node_VariableDeclaration);
		const VarDeclStmt* param = paramNode->ptr;

		const StructDeclStmt* argStruct = GetStructDecl(argument);
		const StructDeclStmt* paramStruct = GetStructDeclFromType(param->type);

		if (argStruct == NULL && paramStruct == NULL)
			continue;
		PROPAGATE_ERROR(CheckTypeConversion(argStruct, paramStruct, funcCall->lineNumber));

		assert(argument.type == Node_MemberAccess);

		ArrayRemove(&funcCall->arguments, argIndex);
		argIndex += ForEachStructMember(argStruct, ExpandArgument,
			&(ExpandArgumentData){
				.argumentNode = argument,
				.funcCall = funcCall,
				.argumentIndex = argIndex,
			},
			NULL);
		argIndex--;
		FreeASTNode(argument);
	}

	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		NodePtr* node = funcCall->arguments.array[i];
		PROPAGATE_ERROR(VisitExpression(node, containingStatement));
	}

	return SUCCESS_RESULT;
}

typedef struct
{
	NodePtr leftExpr;
	NodePtr rightExpr;
	Array* statements;
} GenerateStructMemberAssignmentData;

static NodePtr AllocAssignmentStatement(NodePtr left, NodePtr right, int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.left = left,
					.right = right,
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

static void GenerateStructMemberAssignment(VarDeclStmt* member, size_t index, void* data)
{
	const GenerateStructMemberAssignmentData* d = data;

	VarDeclStmt* leftVarDecl = GetVarDeclFromMemberAccessValue(d->leftExpr);
	assert(leftVarDecl != NULL);
	VarDeclStmt* rightVarDecl = GetVarDeclFromMemberAccessValue(d->rightExpr);
	assert(rightVarDecl != NULL);

	VarDeclStmt* leftInstance = FindInstantiated(member->name, leftVarDecl);
	assert(leftInstance != NULL);
	VarDeclStmt* rightInstance = FindInstantiated(member->name, rightVarDecl);
	assert(rightInstance != NULL);

	NodePtr statement = NULL_NODE;
	if (index == 0 && d->rightExpr.type == Node_FunctionCall)
	{
		statement = AllocAssignmentStatement(
			AllocLiteralIdentifier(leftInstance, -1),
			CopyASTNode(d->rightExpr),
			-1);
	}
	else
	{
		statement = AllocAssignmentStatement(
			AllocLiteralIdentifier(leftInstance, -1),
			AllocLiteralIdentifier(rightInstance, -1),
			-1);
	}
	ArrayAdd(d->statements, &statement);
}

static NodePtr AllocBlockStmt(const int lineNumber)
{
	return AllocASTNode(
		&(BlockStmt){
			.statements = AllocateArray(sizeof(NodePtr)),
			.lineNumber = lineNumber,
		},
		sizeof(BlockStmt), Node_BlockStatement);
}

static Result VisitBinaryExpression(NodePtr* node, NodePtr* containingStatement)
{
	assert(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;
	PROPAGATE_ERROR(VisitExpression(&binary->left, containingStatement));
	PROPAGATE_ERROR(VisitExpression(&binary->right, containingStatement));

	const StructDeclStmt* structDecl;
	{
		const StructDeclStmt* leftStruct = GetStructDecl(binary->left);
		const StructDeclStmt* rightStruct = GetStructDecl(binary->right);

		if (leftStruct == NULL && rightStruct == NULL)
			return SUCCESS_RESULT;
		PROPAGATE_ERROR(CheckTypeConversion(rightStruct, leftStruct, binary->lineNumber));

		structDecl = leftStruct;
	}

	switch (binary->operatorType)
	{
	case Binary_Assignment:
		NodePtr leftExpr = binary->left;
		NodePtr rightExpr = binary->right;
		if (binary->left.type == Node_MemberAccess)
		{
			const MemberAccessExpr* memberAccess = binary->left.ptr;
			leftExpr = memberAccess->value;
		}
		if (binary->right.type == Node_MemberAccess)
		{
			const MemberAccessExpr* memberAccess = binary->right.ptr;
			rightExpr = memberAccess->value;
		}

		if (leftExpr.type != Node_Literal)
			return ERROR_RESULT("Left operand of struct assignment must be a variable",
				binary->lineNumber,
				currentFilePath);

		BlockStmt* block = AllocBlockStmt(-1).ptr;

		ForEachStructMember(structDecl,
			GenerateStructMemberAssignment,
			&(GenerateStructMemberAssignmentData){
				.statements = &block->statements,
				.leftExpr = leftExpr,
				.rightExpr = rightExpr,
			},
			NULL);

		// the pass that breaks up chained assignment and equality will take care of this
		// all struct assignment expressions will be unchained in an expression statement
		// todo add the pass for this
		assert(containingStatement->type == Node_ExpressionStatement);

		FreeASTNode(*containingStatement);
		*containingStatement = (NodePtr){.ptr = block, .type = Node_BlockStatement};
		break;

		// todo should this be implemented
		/*case Binary_IsEqual:*/
		/*	assert(!"is equal");*/
		/*	break;*/
		/*case Binary_NotEqual:*/
		/*	assert(!"not equal");*/
		/*	break;*/

	default: return ERROR_RESULT(
		AllocateString1Str(
			"Cannot use operator \"%s\" on struct types",
			GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
		binary->lineNumber, currentFilePath);
	}

	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		RemoveModuleAccess(node);
		UpdateStructMemberAccess(*node);
		PROPAGATE_ERROR(VisitFunctionCallArguments(node, containingStatement));

		MemberAccessExpr* memberAccess = node->ptr;
		if (memberAccess->next.ptr != NULL)
			break;

		const NodePtr value = memberAccess->value;
		memberAccess->value = NULL_NODE;

		FreeASTNode(*node);
		*node = value;
		break;
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node, containingStatement));
		break;
	case Node_Unary:
		UnaryExpr* unary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&unary->expression, containingStatement));
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
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

typedef struct
{
	Array* destination;
	Array* instantiatedVariables;
	size_t index;
} InstantiateMemberData;

static void InstantiateMember(VarDeclStmt* varDecl, size_t index, void* data)
{
	const InstantiateMemberData* d = data;

	const NodePtr copy = CopyASTNode((NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration});
	ArrayInsert(d->destination, &copy, d->index + index);
	ArrayAdd(d->instantiatedVariables, &copy.ptr);
}

static void InstantiateStructMembers(
	StructDeclStmt* structDecl,
	Array* instantiatedVariables,
	Array* destination,
	const size_t index)
{
	ForEachStructMember(structDecl,
		InstantiateMember,
		&(InstantiateMemberData){
			.destination = destination,
			.instantiatedVariables = instantiatedVariables,
			.index = index,
		},
		NULL);
}

typedef struct
{
	size_t index;
	VarDeclStmt** returnValue;
} GetStructMemberAtIndexData;

static void CheckStructMemberAtIndex(VarDeclStmt* varDecl, size_t index, void* data)
{
	const GetStructMemberAtIndexData* d = data;
	if (index == d->index)
		*d->returnValue = varDecl;
}

static VarDeclStmt* GetStructMemberAtIndex(const StructDeclStmt* structDecl, size_t index)
{
	VarDeclStmt* varDecl = NULL;

	ForEachStructMember(structDecl,
		CheckStructMemberAtIndex,
		&(GetStructMemberAtIndexData){
			.index = index,
			.returnValue = &varDecl,
		},
		NULL);

	return varDecl;
}

static Result VisitExpressionStatement(NodePtr* node)
{
	assert(node->type == Node_ExpressionStatement);
	ExpressionStmt* exprStmt = node->ptr;

	// remove the expression statement if it doesnt have any side effects
	bool remove = false;
	if (exprStmt->expr.type == Node_MemberAccess)
	{
		const MemberAccessExpr* memberAccess = exprStmt->expr.ptr;
		while (true)
		{
			if (GetImportStmtFromMemberAccessValue(memberAccess->value) == NULL &&
				GetVarDeclFromMemberAccessValue(memberAccess->value) == NULL)
			{
				remove = false;
				break;
			}

			if (memberAccess->next.ptr == NULL)
			{
				remove = true;
				break;
			}

			assert(memberAccess->next.type == Node_MemberAccess);
			memberAccess = memberAccess->next.ptr;
		}
	}

	if (remove)
	{
		FreeASTNode(*node);
		*node = NULL_NODE;
	}
	else
		PROPAGATE_ERROR(VisitExpression(&exprStmt->expr, node));

	return SUCCESS_RESULT;
}

static Result VisitStatement(NodePtr* node);

static Result VisitFunctionDeclaration(NodePtr* node)
{
	assert(node->type == Node_FunctionDeclaration);
	FuncDeclStmt* funcDecl = node->ptr;

	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];
		NodePtr copy = CopyASTNode(*node);
		ArrayAdd(&funcDecl->oldParameters, &copy);
	}

	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];

		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = node->ptr;
		assert(varDecl->initializer.ptr == NULL);

		StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
		if (structDecl != NULL)
		{
			InstantiateStructMembers(structDecl, &varDecl->instantiatedVariables, &funcDecl->parameters, i + 1);
			FreeASTNode(*node);
			ArrayRemove(&funcDecl->parameters, i);
			i--;
		}
	}
	assert(funcDecl->block.type == Node_BlockStatement);

	StructDeclStmt* structDecl = GetStructDeclFromType(funcDecl->type);
	if (structDecl != NULL)
	{
		BlockStmt* block = AllocBlockStmt(funcDecl->lineNumber).ptr;

		NodePtr globalReturn = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = funcDecl->lineNumber,
				.type = CopyASTNode(funcDecl->type),
				.name = AllocateString("return"),
				.externalName = NULL,
				.initializer = NULL_NODE,
				.arrayLength = NULL_NODE,
				.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
				.array = false,
				.public = false,
				.external = false,
				.uniqueName = -1,
			},
			sizeof(VarDeclStmt), Node_VariableDeclaration);
		funcDecl->globalReturn = globalReturn.ptr;
		PROPAGATE_ERROR(VisitStatement(&globalReturn));
		ArrayAdd(&block->statements, &globalReturn);

		ArrayAdd(&block->statements, node);
		*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

		VarDeclStmt* first = GetStructMemberAtIndex(structDecl, 0);
		assert(first != NULL);
		assert(funcDecl->oldType.ptr == NULL);
		funcDecl->oldType = funcDecl->type;
		funcDecl->type = CopyASTNode(first->type);
	}

	PROPAGATE_ERROR(VisitStatement(&funcDecl->block));

	return SUCCESS_RESULT;
}

static Result VisitReturnStatement(NodePtr* node)
{
	assert(node->type == Node_Return);
	ReturnStmt* returnStmt = node->ptr;

	assert(returnStmt->function != NULL);
	StructDeclStmt* exprStruct = GetStructDecl(returnStmt->expr);
	StructDeclStmt* returnStruct = GetStructDeclFromType(returnStmt->function->oldType);

	if (returnStruct == NULL && exprStruct == NULL)
	{
		if (returnStmt->expr.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&returnStmt->expr, node));

		return SUCCESS_RESULT;
	}

	PROPAGATE_ERROR(CheckTypeConversion(exprStruct, returnStruct, returnStmt->lineNumber));

	BlockStmt* block = AllocBlockStmt(returnStmt->lineNumber).ptr;

	VarDeclStmt* globalReturn = returnStmt->function->globalReturn;
	NodePtr statement = AllocASTNode(
		&(ExpressionStmt){
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = returnStmt->lineNumber,
					.operatorType = Binary_Assignment,
					.right = returnStmt->expr,
					.left = AllocASTNode(
						&(MemberAccessExpr){
							.next = NULL_NODE,
							.value = AllocLiteralIdentifier(globalReturn, returnStmt->lineNumber),
						},
						sizeof(MemberAccessExpr), Node_MemberAccess),

				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
	PROPAGATE_ERROR(VisitStatement(&statement));
	ArrayAdd(&block->statements, &statement);

	ArrayAdd(&block->statements, node);
	*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

	VarDeclStmt* firstReturnVariable = *(VarDeclStmt**)globalReturn->instantiatedVariables.array[0];
	returnStmt->expr = AllocLiteralIdentifier(firstReturnVariable, returnStmt->lineNumber);
	return SUCCESS_RESULT;
}

static Result VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
		VarDeclStmt* varDecl = node->ptr;

		if (varDecl->initializer.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, node));

		StructDeclStmt* structDecl = GetStructDeclFromType(varDecl->type);
		if (structDecl != NULL)
		{
			BlockStmt* block = AllocBlockStmt(varDecl->lineNumber).ptr;
			InstantiateStructMembers(structDecl, &varDecl->instantiatedVariables, &block->statements, 0);
			ArrayAdd(&nodesToDelete, node);
			*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};
		}
		break;
	case Node_StructDeclaration:
		ArrayAdd(&nodesToDelete, node);
		*node = NULL_NODE;
		break;
	case Node_ExpressionStatement:
		PROPAGATE_ERROR(VisitExpressionStatement(node));
		break;
	case Node_FunctionDeclaration:
		PROPAGATE_ERROR(VisitFunctionDeclaration(node));
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
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, node));
		break;
	case Node_Return:
		PROPAGATE_ERROR(VisitReturnStatement(node));
		break;
	case Node_Section:
		SectionStmt* section = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&section->block));
		break;
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
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
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));
	}

	for (size_t i = 0; i < nodesToDelete.length; ++i)
	{
		const NodePtr* node = nodesToDelete.array[i];
		FreeASTNode(*node);
	}
	FreeArray(&nodesToDelete);

	return SUCCESS_RESULT;
}
