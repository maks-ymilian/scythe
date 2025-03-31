#include "MemberExpansionPass.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"

typedef struct
{
	StructDeclStmt* effectiveType;
	StructDeclStmt* pointerType;
	bool isPointer;
} TypeInfo;

static Array nodesToDelete;

static const char* currentFilePath = NULL;

static NodePtr AllocStructMember(const char* name, PrimitiveType primitiveType, int lineNumber)
{
	assert(name != NULL);
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = lineNumber,
			.name = AllocateString(name),
			.externalName = NULL,
			.initializer = NULL_NODE,
			.instantiatedVariables = AllocateArray(sizeof(NodePtr)),
			.public = false,
			.external = false,
			.uniqueName = -1,
			.type = (Type){
				.modifier = TypeModifier_None,
				.expr = AllocASTNode(
					&(LiteralExpr){
						.lineNumber = lineNumber,
						.type = Literal_PrimitiveType,
						.primitiveType = primitiveType,
					},
					sizeof(LiteralExpr), Node_Literal),
			},
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
}

static NodePtr AllocSetVariable(VarDeclStmt* varDecl, NodePtr right, int lineNumber)
{
	assert(varDecl != NULL);
	return AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = lineNumber,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.right = right,
					.left = AllocASTNode(
						&(MemberAccessExpr){
							.lineNumber = lineNumber,
							.next = NULL_NODE,
							.value = AllocASTNode(
								&(LiteralExpr){
									.lineNumber = lineNumber,
									.type = Literal_Identifier,
									.identifier = (IdentifierReference){
										.text = AllocateString(varDecl->name),
										.reference = (NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration},
									},
								},
								sizeof(LiteralExpr), Node_Literal),
						},
						sizeof(MemberAccessExpr), Node_MemberAccess),
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

static VarDeclStmt* FindInstantiated(const char* name, const VarDeclStmt* aggregateVarDecl)
{
	assert(aggregateVarDecl != NULL);
	for (size_t i = 0; i < aggregateVarDecl->instantiatedVariables.length; ++i)
	{
		VarDeclStmt** varDecl = aggregateVarDecl->instantiatedVariables.array[i];
		if (strcmp((*varDecl)->name, name) == 0)
			return *varDecl;
	}
	return NULL;
}

static TypeInfo GetTypeInfoFromType(const Type type)
{
	assert(type.modifier == TypeModifier_None ||
		   type.modifier == TypeModifier_Pointer);

	StructDeclStmt* structDecl = NULL;
	switch (type.expr.type)
	{
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = type.expr.ptr;
		while (memberAccess->next.ptr != NULL)
		{
			assert(memberAccess->next.type == Node_MemberAccess);
			memberAccess = memberAccess->next.ptr;
		}
		assert(memberAccess->value.type == Node_Literal);
		const LiteralExpr* literal = memberAccess->value.ptr;
		assert(literal->type == Literal_Identifier);
		assert(literal->identifier.reference.type == Node_StructDeclaration);
		structDecl = literal->identifier.reference.ptr;
		break;

	case Node_Literal:
	case Node_Null:
		break;

	default: INVALID_VALUE(type.expr.type);
	}

	return (TypeInfo){
		.effectiveType = type.modifier == TypeModifier_Pointer ? NULL : structDecl,
		.pointerType = structDecl,
		.isPointer = type.modifier == TypeModifier_Pointer,
	};
}

static VarDeclStmt* GetVarDeclFromMemberAccessValue(const NodePtr value)
{
	switch (value.type)
	{
	case Node_Literal:
		const LiteralExpr* literal = value.ptr;
		if (literal->type != Literal_Identifier)
			return NULL;
		assert(literal->identifier.reference.type == Node_VariableDeclaration);
		return literal->identifier.reference.ptr;

	case Node_FunctionCall:
		const FuncCallExpr* funcCall = value.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;
		return funcDecl->globalReturn;

	default: INVALID_VALUE(value.type);
	}
}

static IdentifierReference* GetIdentifier(const MemberAccessExpr* memberAccess)
{
	assert(memberAccess->value.type == Node_Literal);
	LiteralExpr* literal = memberAccess->value.ptr;
	if (literal->type != Literal_Identifier)
		return NULL;

	return &literal->identifier;
}

static void MakeMemberAccessesPointToInstantiated(const NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = memberAccessNode.ptr;
	if (memberAccess->value.type != Node_Literal)
		return;
	const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL || GetTypeInfoFromType(varDecl->type).effectiveType == NULL)
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
		if (GetTypeInfoFromType(nextVarDecl->type).effectiveType == NULL)
			break;
	}

	IdentifierReference* currentIdentifier = GetIdentifier(memberAccess);
	const IdentifierReference* nextIdentifier = GetIdentifier(next);
	assert(currentIdentifier != NULL);
	assert(nextIdentifier != NULL);

	VarDeclStmt* instantiated = FindInstantiated(nextIdentifier->text, varDecl);
	assert(instantiated != NULL);
	IdentifierReference newIdentifier =
		(IdentifierReference){
			.reference = (NodePtr){.ptr = instantiated, .type = Node_VariableDeclaration},
			.text = AllocateString(nextIdentifier->text),
		};

	assert(next->value.type == Node_Literal);

	free(currentIdentifier->text);
	*currentIdentifier = newIdentifier;

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

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data);
static size_t ForEachStructMember(
	StructDeclStmt* aggregateType,
	const StructMemberFunc func,
	void* data,
	size_t* currentIndex)
{
	assert(aggregateType != NULL);

	size_t index = 0;
	if (currentIndex == NULL)
		currentIndex = &index;

	for (size_t i = 0; i < aggregateType->members.length; i++)
	{
		const NodePtr* memberNode = aggregateType->members.array[i];
		assert(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		StructDeclStmt* memberType = GetTypeInfoFromType(varDecl->type).effectiveType;
		if (memberType == NULL)
		{
			if (func != NULL)
				func(varDecl, aggregateType, *currentIndex, data);
			(*currentIndex)++;
		}
		else
			ForEachStructMember(memberType, func, data, currentIndex);
	}

	return *currentIndex;
}

static NodePtr AllocLiteralIdentifier(VarDeclStmt* varDecl, int lineNumber)
{
	assert(varDecl != NULL);
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

static NodePtr AllocStructOffsetCalculation(NodePtr offset, size_t memberIndex, size_t memberCount, int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_Add,
			.left = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Multiply,
					.left = offset,
					.right = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Int,
							.intValue = memberCount,
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
			.right = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_Int,
					.intValue = memberIndex,
				},
				sizeof(LiteralExpr), Node_Literal),
		},
		sizeof(BinaryExpr), Node_Binary);
}

static TypeInfo GetTypeInfoFromExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	{
		const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(node);
		if (varDecl != NULL)
			return GetTypeInfoFromType(varDecl->type);

		return (TypeInfo){
			.effectiveType = NULL,
			.pointerType = NULL,
			.isPointer = false,
		};
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = node.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

		TypeInfo typeInfo = GetTypeInfoFromType(funcDecl->oldType);
		if (typeInfo.pointerType != NULL)
			return typeInfo;

		return GetTypeInfoFromType(funcDecl->type);
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
			return GetTypeInfoFromType(varDecl->type);

		return (TypeInfo){
			.effectiveType = NULL,
			.pointerType = NULL,
			.isPointer = false,
		};
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = node.ptr;
		TypeInfo typeInfo = GetTypeInfoFromExpression(subscript->expr);
		if (typeInfo.isPointer)
			return (TypeInfo){
				.effectiveType = typeInfo.pointerType,
				.pointerType = typeInfo.pointerType,
				.isPointer = false,
			};

		return typeInfo;
	}
	case Node_Binary:
	case Node_Unary:
		return (TypeInfo){
			.effectiveType = NULL,
			.pointerType = NULL,
			.isPointer = false,
		};

	default: INVALID_VALUE(node.type);
	}
}

static Result CheckTypeConversion(StructDeclStmt* from, StructDeclStmt* to, const int lineNumber)
{
	if (from == NULL && to == NULL)
		assert(0);

	if (from == NULL && to != NULL)
		return ERROR_RESULT(
			"Cannot convert a non-aggregate type to an aggregate type",
			lineNumber, currentFilePath);

	if (from != NULL && to == NULL)
		return ERROR_RESULT(
			"Cannot convert an aggregate type to a non-aggregate type",
			lineNumber, currentFilePath);

	if (from != to)
		return ERROR_RESULT(
			"Cannot convert between incompatible aggregate types",
			lineNumber, currentFilePath);

	return SUCCESS_RESULT;
}

typedef struct
{
	NodePtr argumentNode;
	FuncCallExpr* funcCall;
	size_t argumentIndex;
} ExpandArgumentData;

static void ExpandArgument(VarDeclStmt* member, StructDeclStmt* parentType, size_t index, void* data)
{
	const ExpandArgumentData* d = data;

	if (index == 0 && d->argumentNode.type == Node_FunctionCall)
		return;

	VarDeclStmt* aggregateVarDecl = GetVarDeclFromMemberAccessValue(d->argumentNode);
	assert(aggregateVarDecl != NULL);

	VarDeclStmt* currentInstance = FindInstantiated(member->name, aggregateVarDecl);
	assert(currentInstance != NULL);

	NodePtr expr = AllocLiteralIdentifier(currentInstance, -1);
	ArrayInsert(&d->funcCall->arguments, &expr, d->argumentIndex + index);
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

	assert(funcDecl->oldParameters.length == funcCall->arguments.length);
	for (size_t paramIndex = 0, argIndex = 0; paramIndex < funcDecl->oldParameters.length; ++argIndex, ++paramIndex)
	{
		PROPAGATE_ERROR(VisitExpression(funcCall->arguments.array[argIndex], containingStatement));

		const NodePtr argument = *(NodePtr*)funcCall->arguments.array[argIndex];

		const NodePtr* paramNode = funcDecl->oldParameters.array[paramIndex];
		assert(paramNode->type == Node_VariableDeclaration);
		const VarDeclStmt* param = paramNode->ptr;

		StructDeclStmt* argAggregate = GetTypeInfoFromExpression(argument).effectiveType;
		StructDeclStmt* paramAggregate = GetTypeInfoFromType(param->type).effectiveType;

		if (argAggregate == NULL && paramAggregate == NULL)
			continue;
		PROPAGATE_ERROR(CheckTypeConversion(argAggregate, paramAggregate, funcCall->lineNumber));

		if (argument.type == Node_Literal)
			ArrayRemove(&funcCall->arguments, argIndex);

		argIndex += ForEachStructMember(argAggregate, ExpandArgument,
						&(ExpandArgumentData){
							.argumentNode = argument,
							.funcCall = funcCall,
							.argumentIndex = argIndex,
						},
						NULL) -
					1;
	}

	return SUCCESS_RESULT;
}

typedef struct
{
	NodePtr leftExpr;
	NodePtr rightExpr;
	Array* statements;
	size_t memberCount;
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

static void GenerateStructMemberAssignment(VarDeclStmt* member, StructDeclStmt* parentType, size_t index, void* data)
{
	const GenerateStructMemberAssignmentData* d = data;

	NodePtr left = NULL_NODE;
	if (d->leftExpr.type == Node_Subscript)
	{
		assert(0); // todo
	}
	else
	{
		VarDeclStmt* leftInstance = FindInstantiated(member->name, GetVarDeclFromMemberAccessValue(d->leftExpr));
		left = AllocLiteralIdentifier(leftInstance, -1);
	}

	NodePtr right = NULL_NODE;
	if (d->rightExpr.type == Node_Subscript)
	{
		assert(0); // todo
	}
	else
	{
		VarDeclStmt* rightInstance = FindInstantiated(member->name, GetVarDeclFromMemberAccessValue(d->rightExpr));

		if (index == 0 && d->rightExpr.type == Node_FunctionCall)
			right = CopyASTNode(d->rightExpr);
		else
			right = AllocLiteralIdentifier(rightInstance, -1);
	}

	NodePtr statement = AllocAssignmentStatement(left, right, -1);
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

	StructDeclStmt* aggregateType;
	{
		StructDeclStmt* leftAggregateType = GetTypeInfoFromExpression(binary->left).effectiveType;
		StructDeclStmt* rightAggregateType = GetTypeInfoFromExpression(binary->right).effectiveType;

		if (leftAggregateType == NULL && rightAggregateType == NULL)
			return SUCCESS_RESULT;
		PROPAGATE_ERROR(CheckTypeConversion(leftAggregateType, rightAggregateType, binary->lineNumber));

		aggregateType = leftAggregateType;
	}

	if (binary->operatorType != Binary_Assignment)
		return ERROR_RESULT(
			AllocateString1Str(
				"Cannot use operator \"%s\" on aggregate types",
				GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
			binary->lineNumber, currentFilePath);

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

	BlockStmt* block = AllocBlockStmt(-1).ptr;

	if (leftExpr.type != Node_Literal &&
		leftExpr.type != Node_Subscript)
		return ERROR_RESULT("Left operand of aggregate type assignment must be a variable",
			binary->lineNumber,
			currentFilePath);

	ForEachStructMember(aggregateType,
		GenerateStructMemberAssignment,
		&(GenerateStructMemberAssignmentData){
			.statements = &block->statements,
			.leftExpr = leftExpr,
			.rightExpr = rightExpr,
			.memberCount = ForEachStructMember(aggregateType, NULL, NULL, NULL),
		},
		NULL);

	// the pass that breaks up chained assignment and equality will take care of this
	// all struct assignment expressions will be unchained in an expression statement
	// todo add the pass for this
	assert(containingStatement->type == Node_ExpressionStatement);

	FreeASTNode(*containingStatement);
	*containingStatement = (NodePtr){.ptr = block, .type = Node_BlockStatement};
	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		RemoveModuleAccess(node);
		MakeMemberAccessesPointToInstantiated(*node);
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
		break;
	case Node_Literal:
		const LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_Int ||
			   literal->type == Literal_Float ||
			   literal->type == Literal_String ||
			   literal->type == Literal_Boolean ||
			   literal->type == Literal_PrimitiveType);
		break;
	case Node_Subscript:
		SubscriptExpr* subscript = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&subscript->expr, containingStatement));
		PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, containingStatement));
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

static void InstantiateMember(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data)
{
	const InstantiateMemberData* d = data;

	NodePtr copy = CopyASTNode((NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration});

	ArrayInsert(d->destination, &copy, d->index + index);
	ArrayAdd(d->instantiatedVariables, &copy.ptr);
}

static void InstantiateMembers(
	StructDeclStmt* aggregateType,
	Array* instantiatedVariables,
	Array* destination,
	const size_t index)
{
	ForEachStructMember(aggregateType,
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

static void CheckMemberAtIndex(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data)
{
	const GetStructMemberAtIndexData* d = data;
	if (index == d->index)
		*d->returnValue = varDecl;
}

static VarDeclStmt* GetMemberAtIndex(StructDeclStmt* aggregateType, size_t index)
{
	VarDeclStmt* varDecl = NULL;

	ForEachStructMember(aggregateType,
		CheckMemberAtIndex,
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

	bool remove = false;
	if (exprStmt->expr.type == Node_MemberAccess)
	{
		remove = true;
		const MemberAccessExpr* memberAccess = exprStmt->expr.ptr;
		while (true)
		{
			if (memberAccess->value.type != Node_Literal)
			{
				remove = false;
				break;
			}

			if (memberAccess->next.ptr == NULL)
				break;

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

		StructDeclStmt* aggregateType = GetTypeInfoFromType(varDecl->type).effectiveType;
		if (aggregateType != NULL)
		{
			if (funcDecl->external)
				return ERROR_RESULT("External functions cannot have any struct parameters",
					funcDecl->lineNumber,
					currentFilePath);

			if (varDecl->initializer.ptr != NULL)
				PROPAGATE_ERROR(CheckTypeConversion(
					GetTypeInfoFromExpression(varDecl->initializer).effectiveType,
					aggregateType,
					varDecl->lineNumber));

			InstantiateMembers(aggregateType, &varDecl->instantiatedVariables, &funcDecl->parameters, i + 1);
			ArrayAdd(&nodesToDelete, node);
			ArrayRemove(&funcDecl->parameters, i);
			i--;
		}
	}

	StructDeclStmt* aggregateType = GetTypeInfoFromType(funcDecl->type).effectiveType;
	if (aggregateType != NULL)
	{
		if (funcDecl->external)
			return ERROR_RESULT("External functions cannot return a struct", funcDecl->lineNumber, currentFilePath);

		BlockStmt* block = AllocBlockStmt(funcDecl->lineNumber).ptr;

		NodePtr globalReturn = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = funcDecl->lineNumber,
				.type.expr = CopyASTNode(funcDecl->type.expr),
				.type.modifier = funcDecl->type.modifier,
				.name = AllocateString("return"),
				.externalName = NULL,
				.initializer = NULL_NODE,
				.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
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

		VarDeclStmt* first = GetMemberAtIndex(aggregateType, 0);
		assert(first != NULL);
		assert(funcDecl->oldType.expr.ptr == NULL);
		funcDecl->oldType = funcDecl->type;
		funcDecl->type.expr = CopyASTNode(first->type.expr);
		funcDecl->type.modifier = first->type.modifier;
	}

	if (!funcDecl->external)
	{
		assert(funcDecl->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
	}

	return SUCCESS_RESULT;
}

static Result VisitReturnStatement(NodePtr* node)
{
	assert(node->type == Node_Return);
	ReturnStmt* returnStmt = node->ptr;

	assert(returnStmt->function != NULL);
	StructDeclStmt* exprAggregateType = GetTypeInfoFromExpression(returnStmt->expr).effectiveType;
	StructDeclStmt* returnAggregateType = GetTypeInfoFromType(returnStmt->function->oldType).effectiveType;

	if (returnAggregateType == NULL && exprAggregateType == NULL)
	{
		if (returnStmt->expr.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&returnStmt->expr, node));

		return SUCCESS_RESULT;
	}

	PROPAGATE_ERROR(CheckTypeConversion(exprAggregateType, returnAggregateType, returnStmt->lineNumber));

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
							.lineNumber = returnStmt->lineNumber,
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

static Result VisitVariableDeclaration(NodePtr* node)
{
	assert(node->type == Node_VariableDeclaration);
	VarDeclStmt* varDecl = node->ptr;

	StructDeclStmt* aggregateType = GetTypeInfoFromType(varDecl->type).effectiveType;
	if (aggregateType == NULL)
	{
		if (varDecl->initializer.ptr != NULL)
		{
			NodePtr setVariable = AllocSetVariable(varDecl, CopyASTNode(varDecl->initializer), varDecl->lineNumber);
			PROPAGATE_ERROR(VisitStatement(&setVariable));
			FreeASTNode(setVariable);

			PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, node));
		}
		return SUCCESS_RESULT;
	}

	if (varDecl->external)
		return ERROR_RESULT(
			"External variable declarations cannot be of an aggregate type",
			varDecl->lineNumber,
			currentFilePath);

	BlockStmt* block = AllocBlockStmt(varDecl->lineNumber).ptr;
	InstantiateMembers(aggregateType, &varDecl->instantiatedVariables, &block->statements, block->statements.length);

	if (varDecl->initializer.ptr != NULL)
	{
		NodePtr setVariable = AllocSetVariable(varDecl, varDecl->initializer, varDecl->lineNumber);
		varDecl->initializer = NULL_NODE;
		PROPAGATE_ERROR(VisitStatement(&setVariable));
		ArrayAdd(&block->statements, &setVariable);
	}

	ArrayAdd(&nodesToDelete, node);
	*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

	return SUCCESS_RESULT;
}

static Result VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
		PROPAGATE_ERROR(VisitVariableDeclaration(node));
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
	case Node_While:
		WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr, node));
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		break;
	case Node_LoopControl:
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
		FreeASTNode(*(NodePtr*)nodesToDelete.array[i]);
	FreeArray(&nodesToDelete);

	return SUCCESS_RESULT;
}
