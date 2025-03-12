#include "MemberExpansionPass.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"

typedef struct
{
	union
	{
		StructDeclStmt* structDecl;
		PrimitiveType primitiveType;
	};

	enum
	{
		AggregateType_None,
		AggregateType_Struct,
		AggregateType_StructArray,
		AggregateType_PrimitiveArray,
	} type;
} AggregateType;

static Array arrayMembers;

static Array nodesToDelete;

static const char* currentFilePath = NULL;

static NodePtr AllocStructMember(const char* name, PrimitiveType primitiveType, int lineNumber)
{
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
			.arrayType = (Type){.array = false, .expr = NULL_NODE},
			.type = (Type){
				.array = false,
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
	for (size_t i = 0; i < aggregateVarDecl->instantiatedVariables.length; ++i)
	{
		VarDeclStmt** varDecl = aggregateVarDecl->instantiatedVariables.array[i];
		if (strcmp((*varDecl)->name, name) == 0)
			return *varDecl;
	}
	return NULL;
}

static AggregateType GetAggregateFromType(const Type type)
{
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
		return (AggregateType){
			.type = type.array ? AggregateType_StructArray : AggregateType_Struct,
			.structDecl = literal->identifier.reference.ptr,
		};

	case Node_Literal:
		if (!type.array)
			return (AggregateType){.type = AggregateType_None};

		const LiteralExpr* literalExpr = type.expr.ptr;
		assert(literalExpr->type == Literal_PrimitiveType);
		return (AggregateType){
			.type = AggregateType_PrimitiveArray,
			.primitiveType = literalExpr->primitiveType,
		};

	case Node_Null:
		return (AggregateType){.type = AggregateType_None};

	default: INVALID_VALUE(type.expr.type);
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

	case Node_Subscript:
		const SubscriptExpr* subscript = value.ptr;
		assert(subscript->identifier.reference.type == Node_VariableDeclaration ||
			   subscript->identifier.reference.type == Node_Null);
		return subscript->identifier.reference.ptr;
	default: INVALID_VALUE(value.type);
	}
}

static IdentifierReference* GetIdentifier(const MemberAccessExpr* memberAccess)
{
	switch (memberAccess->value.type)
	{
	case Node_Literal:
		LiteralExpr* literal = memberAccess->value.ptr;
		if (literal->type != Literal_Identifier)
			return NULL;
		return &literal->identifier;

	case Node_Subscript:
		SubscriptExpr* subscript = memberAccess->value.ptr;
		return &subscript->identifier;

	default: INVALID_VALUE(memberAccess->value.type);
	}
}

static void UpdateStructMemberAccess(const NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = memberAccessNode.ptr;
	if (memberAccess->value.type != Node_Literal)
		return;
	const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(memberAccess->value);
	if (varDecl == NULL || GetAggregateFromType(varDecl->type).type == AggregateType_None)
		return;

	const MemberAccessExpr* next = memberAccess;
	while (true)
	{
		if (next->next.ptr == NULL)
			return;

		assert(next->next.type == Node_MemberAccess);
		next = next->next.ptr;

		const VarDeclStmt* nextVarDecl = GetVarDeclFromMemberAccessValue(next->value);
		if (nextVarDecl == NULL) // if its accessing array
			break;
		if (GetAggregateFromType(nextVarDecl->type).type == AggregateType_None)
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

	if (next->value.type == Node_Subscript)
	{
		SubscriptExpr* subscript = next->value.ptr;

		NodePtr new = AllocASTNode(
			&(SubscriptExpr){
				.lineNumber = memberAccess->lineNumber,
				.identifier = newIdentifier,
				.expr = subscript->expr,
			},
			sizeof(SubscriptExpr), Node_Subscript);
		subscript->expr = NULL_NODE;

		FreeASTNode(memberAccess->value);
		memberAccess->value = new;
	}
	else
	{
		assert(next->value.type == Node_Literal);

		free(currentIdentifier->text);
		*currentIdentifier = newIdentifier;
	}

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
	case Node_Subscript:
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

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, AggregateType parentType, size_t index, void* data);
static size_t ForEachStructMember(
	const AggregateType aggregateType,
	const StructMemberFunc func,
	void* data,
	size_t* currentIndex)
{
	assert(aggregateType.type != AggregateType_None);

	size_t index = 0;
	if (currentIndex == NULL)
		currentIndex = &index;

	Array* members = NULL;
	if (aggregateType.type == AggregateType_StructArray ||
		aggregateType.type == AggregateType_PrimitiveArray)
		members = &arrayMembers;
	else if (aggregateType.type == AggregateType_Struct)
		members = &aggregateType.structDecl->members;
	else
		assert(0);

	for (size_t i = 0; i < members->length; i++)
	{
		const NodePtr* memberNode = members->array[i];
		assert(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		const AggregateType memberType = GetAggregateFromType(varDecl->type);
		if (memberType.type == AggregateType_None)
		{
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

static AggregateType GetAggregateFromExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	{
		const VarDeclStmt* varDecl = GetVarDeclFromMemberAccessValue(node);
		if (varDecl != NULL)
			return GetAggregateFromType(varDecl->type);
		return (AggregateType){.type = AggregateType_None};
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = node.ptr;
		assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
		const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

		AggregateType aggregateType = GetAggregateFromType(funcDecl->oldType);
		if (aggregateType.type != AggregateType_None)
			return aggregateType;

		return GetAggregateFromType(funcDecl->type);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = node.ptr;
		assert(subscript->identifier.reference.type == Node_VariableDeclaration);
		const VarDeclStmt* varDecl = subscript->identifier.reference.ptr;
		return GetAggregateFromType(varDecl->type);
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
			return GetAggregateFromType(varDecl->type);
		return (AggregateType){.type = AggregateType_None};
	}
	case Node_Binary:
	case Node_Unary:
		return (AggregateType){.type = AggregateType_None};
	default: INVALID_VALUE(node.type);
	}
}

static bool AreAggregateTypesEqual(AggregateType a, AggregateType b)
{
	assert(a.type != AggregateType_None && b.type != AggregateType_None);

	if (a.type != b.type)
		return false;

	if (a.type == AggregateType_Struct)
		return a.structDecl == b.structDecl;
	else if (a.type == AggregateType_StructArray)
		return a.structDecl == b.structDecl;
	else if (a.type == AggregateType_PrimitiveArray)
		return a.primitiveType == b.primitiveType;
	else
		assert(0);
}

static Result CheckTypeConversion(AggregateType from, AggregateType to, const int lineNumber)
{
	if (from.type == AggregateType_None && to.type == AggregateType_None)
		assert(0);

	if (from.type == AggregateType_None && to.type != AggregateType_None)
		return ERROR_RESULT(
			"Cannot convert a non-aggregate type to an aggregate type",
			lineNumber, currentFilePath);

	if (from.type != AggregateType_None && to.type == AggregateType_None)
		return ERROR_RESULT(
			"Cannot convert an aggregate type to a non-aggregate type",
			lineNumber, currentFilePath);

	if (!AreAggregateTypesEqual(from, to))
		assert(!"Cannot convert aggregate type \"%s\" to aggregate type \"%s\""); // todo
	/*return ERROR_RESULT(*/
	/*	AllocateString2Str(*/
	/*		"Cannot convert struct type \"%s\" to struct type \"%s\"",*/
	/*		from->name,*/
	/*		to->name),*/
	/*	lineNumber, currentFilePath);*/

	return SUCCESS_RESULT;
}

typedef struct
{
	NodePtr argumentNode;
	FuncCallExpr* funcCall;
	size_t argumentIndex;
} ExpandArgumentData;

static void ExpandArgument(VarDeclStmt* member, AggregateType parentType, size_t index, void* data)
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

		const AggregateType argAggregate = GetAggregateFromExpression(argument);
		const AggregateType paramAggregate = GetAggregateFromType(param->type);

		if (argAggregate.type == AggregateType_None && paramAggregate.type == AggregateType_None)
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

static Result TransformSubscriptMemberAccess(NodePtr memberAccessNode)
{
	assert(memberAccessNode.type == Node_MemberAccess);
	for (MemberAccessExpr* memberAccess = memberAccessNode.ptr; memberAccess != NULL; memberAccess = memberAccess->next.ptr)
	{
		if (memberAccess->value.type != Node_Subscript)
			continue;

		SubscriptExpr* subscript = memberAccess->value.ptr;
		if (subscript->identifier.reference.type != Node_VariableDeclaration)
			continue;

		VarDeclStmt* varDecl = subscript->identifier.reference.ptr;
		AggregateType aggregateType = GetAggregateFromType(varDecl->type);
		if (aggregateType.type == AggregateType_Struct)
			return ERROR_RESULT("Cannot use subscript on a struct type",
				subscript->lineNumber,
				currentFilePath);

		if (aggregateType.type != AggregateType_StructArray &&
			aggregateType.type != AggregateType_PrimitiveArray)
			continue;

		NodePtr newMemberAccess = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = subscript->lineNumber,
				.next = memberAccess->next,
				.value = AllocASTNode(
					&(SubscriptExpr){
						.lineNumber = subscript->lineNumber,
						.identifier = (IdentifierReference){
							.text = AllocateString("offset"),
							.reference = NULL_NODE,
						},
						.expr = subscript->expr,
					},
					sizeof(SubscriptExpr), Node_Subscript),
			},
			sizeof(MemberAccessExpr), Node_MemberAccess);

		memberAccess->value = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = subscript->lineNumber,
				.type = Literal_Identifier,
				.identifier = (IdentifierReference){
					.text = AllocateString(subscript->identifier.text),
					.reference = subscript->identifier.reference,
				},
			},
			sizeof(LiteralExpr), Node_Literal);
		subscript->expr = NULL_NODE;
		FreeASTNode((NodePtr){.ptr = subscript, .type = Node_Subscript});

		memberAccess->next = newMemberAccess;
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

static void GenerateStructMemberAssignment(VarDeclStmt* member, AggregateType parentType, size_t index, void* data)
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

	AggregateType aggregateType;
	{
		const AggregateType leftAggregateType = GetAggregateFromExpression(binary->left);
		const AggregateType rightAggregateType = GetAggregateFromExpression(binary->right);

		if (leftAggregateType.type == AggregateType_None && rightAggregateType.type == AggregateType_None)
			return SUCCESS_RESULT;
		PROPAGATE_ERROR(CheckTypeConversion(leftAggregateType, rightAggregateType, binary->lineNumber));

		aggregateType = leftAggregateType;
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

		ForEachStructMember(aggregateType,
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
		PROPAGATE_ERROR(TransformSubscriptMemberAccess(*node));
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

static void SetArrayType(VarDeclStmt* varDecl, AggregateType aggregateType)
{
	if (aggregateType.type == AggregateType_Struct ||
		aggregateType.type == AggregateType_StructArray)
		return;

	varDecl->arrayType.array = aggregateType.type == AggregateType_PrimitiveArray;

	assert(aggregateType.type == AggregateType_PrimitiveArray);
	varDecl->arrayType.expr = AllocASTNode(
		&(LiteralExpr){
			.lineNumber = varDecl->lineNumber,
			.type = Literal_PrimitiveType,
			.primitiveType = aggregateType.primitiveType,
		},
		sizeof(LiteralExpr), Node_Literal);
}

static void InstantiateMember(VarDeclStmt* varDecl, AggregateType parentType, size_t index, void* data)
{
	const InstantiateMemberData* d = data;

	NodePtr copy = CopyASTNode((NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration});

	if (strcmp(varDecl->name, "offset") == 0)
	{
		VarDeclStmt* varDecl = copy.ptr;
		SetArrayType(varDecl, parentType);
	}

	ArrayInsert(d->destination, &copy, d->index + index);
	ArrayAdd(d->instantiatedVariables, &copy.ptr);
}

static void InstantiateMembers(
	AggregateType aggregateType,
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

static void CheckMemberAtIndex(VarDeclStmt* varDecl, AggregateType parentType, size_t index, void* data)
{
	const GetStructMemberAtIndexData* d = data;
	if (index == d->index)
		*d->returnValue = varDecl;
}

static VarDeclStmt* GetMemberAtIndex(AggregateType aggregateType, size_t index)
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

		AggregateType aggregateType = GetAggregateFromType(varDecl->type);
		if (aggregateType.type != AggregateType_None)
		{
			if (funcDecl->external)
				return ERROR_RESULT("External functions cannot have any struct parameters",
					funcDecl->lineNumber,
					currentFilePath);

			if (varDecl->initializer.ptr != NULL)
				PROPAGATE_ERROR(CheckTypeConversion(
					GetAggregateFromExpression(varDecl->initializer),
					aggregateType,
					varDecl->lineNumber));

			InstantiateMembers(aggregateType, &varDecl->instantiatedVariables, &funcDecl->parameters, i + 1);
			ArrayAdd(&nodesToDelete, node);
			ArrayRemove(&funcDecl->parameters, i);
			i--;
		}
	}

	AggregateType aggregateType = GetAggregateFromType(funcDecl->type);
	if (aggregateType.type != AggregateType_None)
	{
		if (funcDecl->external)
			return ERROR_RESULT("External functions cannot return a struct", funcDecl->lineNumber, currentFilePath);

		BlockStmt* block = AllocBlockStmt(funcDecl->lineNumber).ptr;

		NodePtr globalReturn = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = funcDecl->lineNumber,
				.type.expr = CopyASTNode(funcDecl->type.expr),
				.type.array = funcDecl->type.array,
				.arrayType = (Type){.array = false, .expr = NULL_NODE},
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
		funcDecl->type.array = first->type.array;
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
	AggregateType exprAggregateType = GetAggregateFromExpression(returnStmt->expr);
	AggregateType returnAggregateType = GetAggregateFromType(returnStmt->function->oldType);

	if (returnAggregateType.type == AggregateType_None && exprAggregateType.type == AggregateType_None)
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

	AggregateType aggregateType = GetAggregateFromType(varDecl->type);
	if (aggregateType.type == AggregateType_None)
	{
		if (varDecl->initializer.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, node));
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
		NodePtr node = AllocSetVariable(varDecl, varDecl->initializer, varDecl->lineNumber);
		varDecl->initializer = NULL_NODE;
		PROPAGATE_ERROR(VisitStatement(&node));
		ArrayAdd(&block->statements, &node);
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
	arrayMembers = AllocateArray(sizeof(NodePtr));
	NodePtr node = AllocStructMember("offset", Primitive_Int, -1);
	ArrayAdd(&arrayMembers, &node);
	node = AllocStructMember("length", Primitive_Int, -1);
	ArrayAdd(&arrayMembers, &node);

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

	for (size_t i = 0; i < arrayMembers.length; ++i)
		FreeASTNode(*(NodePtr*)arrayMembers.array[i]);
	FreeArray(&arrayMembers);

	return SUCCESS_RESULT;
}
