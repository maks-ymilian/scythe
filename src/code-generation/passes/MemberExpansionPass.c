#include "MemberExpansionPass.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"

typedef struct
{
	StructDeclStmt* effectiveType;
	StructDeclStmt* pointerType;
	bool isPointer;
} TypeInfo;

static Array nodesToDelete;

static const char* currentFilePath = NULL;

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement);

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
	assert(type.expr.ptr != NULL);
	assert(type.modifier == TypeModifier_None ||
		   type.modifier == TypeModifier_Pointer);

	StructDeclStmt* structDecl = NULL;
	switch (type.expr.type)
	{
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = type.expr.ptr;
		structDecl = memberAccess->typeReference;
		assert(structDecl != NULL);
		break;

	case Node_Literal:
		break;

	default: INVALID_VALUE(type.expr.type);
	}

	return (TypeInfo){
		.effectiveType = type.modifier == TypeModifier_Pointer ? NULL : structDecl,
		.pointerType = type.modifier == TypeModifier_Pointer ? structDecl : NULL,
		.isPointer = type.modifier == TypeModifier_Pointer,
	};
}

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data);
static size_t ForEachStructMember(
	StructDeclStmt* type,
	const StructMemberFunc func,
	void* data,
	size_t* currentIndex)
{
	assert(type != NULL);

	size_t index = 0;
	if (currentIndex == NULL)
		currentIndex = &index;

	for (size_t i = 0; i < type->members.length; i++)
	{
		const NodePtr* memberNode = type->members.array[i];
		assert(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		StructDeclStmt* memberType = GetTypeInfoFromType(varDecl->type).effectiveType;
		if (memberType == NULL)
		{
			if (func != NULL)
				func(varDecl, type, *currentIndex, data);
			(*currentIndex)++;
		}
		else
			ForEachStructMember(memberType, func, data, currentIndex);
	}

	return *currentIndex;
}

static TypeInfo GetTypeInfoFromExpression(const NodePtr node)
{
	const TypeInfo nullTypeInfo =
		(TypeInfo){
			.effectiveType = NULL,
			.pointerType = NULL,
			.isPointer = false,
		};

	switch (node.type)
	{
	case Node_Literal:
	{
		return nullTypeInfo;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		assert(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		FuncDeclStmt* funcDecl = memberAccess->funcReference;
		assert(funcDecl != NULL);

		if (funcDecl->oldType.expr.ptr == NULL)
			return nullTypeInfo;

		TypeInfo typeInfo = GetTypeInfoFromType(funcDecl->oldType);
		if (typeInfo.effectiveType != NULL)
			return typeInfo;

		return GetTypeInfoFromType(funcDecl->type);
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		VarDeclStmt* varDecl = memberAccess->varReference;
		if (varDecl == NULL)
			return nullTypeInfo;
		return GetTypeInfoFromType(varDecl->type);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = node.ptr;
		if (subscript->typeBeforeCollapse.expr.ptr)
			return GetTypeInfoFromType(subscript->typeBeforeCollapse);
		else
		{
			TypeInfo typeInfo = GetTypeInfoFromExpression(subscript->baseExpr);
			if (typeInfo.isPointer)
				return (TypeInfo){
					.effectiveType = typeInfo.pointerType,
					.pointerType = NULL,
					.isPointer = false,
				};

			return typeInfo;
		}
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;

		if (binary->operatorType == Binary_Assignment ||
			binary->operatorType == Binary_AddAssign ||
			binary->operatorType == Binary_SubtractAssign ||
			binary->operatorType == Binary_MultiplyAssign ||
			binary->operatorType == Binary_DivideAssign ||
			binary->operatorType == Binary_ModuloAssign ||
			binary->operatorType == Binary_ExponentAssign ||
			binary->operatorType == Binary_BitAndAssign ||
			binary->operatorType == Binary_BitOrAssign ||
			binary->operatorType == Binary_XORAssign)
			return GetTypeInfoFromExpression(binary->left);

		if (binary->operatorType == Binary_Add ||
			binary->operatorType == Binary_Subtract)
		{
			TypeInfo left = GetTypeInfoFromExpression(binary->left);
			TypeInfo right = GetTypeInfoFromExpression(binary->right);

			StructDeclStmt* type = NULL;
			if (left.isPointer && left.pointerType)
				type = left.pointerType;

			if (right.isPointer && right.pointerType)
				type = right.pointerType;

			// different pointer types are assignable to each other so
			// if both are struct pointers and theyre different types
			// then there is no type
			if (left.isPointer && left.pointerType &&
				right.isPointer && right.pointerType &&
				left.pointerType != right.pointerType)
				return nullTypeInfo;

			if (!type)
				return nullTypeInfo;

			return (TypeInfo){
				.effectiveType = NULL,
				.pointerType = type,
				.isPointer = true,
			};
		}

		return nullTypeInfo;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		return GetTypeInfoFromExpression(unary->expression);
	}

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

static NodePtr AllocMultiply(NodePtr left, NodePtr right, int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_Multiply,
			.left = left,
			.right = right,
		},
		sizeof(BinaryExpr), Node_Binary);
}

static NodePtr AllocInteger(uint64_t value, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_Int,
			.intValue = value,
		},
		sizeof(LiteralExpr), Node_Literal);
}

static NodePtr AllocStructOffsetCalculation(NodePtr offset, size_t memberIndex, size_t memberCount, int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_Add,
			.left = AllocMultiply(offset, AllocInteger(memberCount, lineNumber), lineNumber),
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

typedef struct
{
	VarDeclStmt* member;
	size_t* returnValue;
} GetIndexOfMemberData;

static void GetIndexOfMemberInternal(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data)
{
	const GetIndexOfMemberData* d = data;
	if (d->member == varDecl)
		*d->returnValue = index;
}

static size_t GetIndexOfMember(StructDeclStmt* type, VarDeclStmt* member)
{
	size_t index = 0;

	ForEachStructMember(type,
		GetIndexOfMemberInternal,
		&(GetIndexOfMemberData){
			.member = member,
			.returnValue = &index,
		},
		NULL);

	return index;
}

static size_t CountStructMembers(StructDeclStmt* type)
{
	return ForEachStructMember(type, NULL, NULL, NULL);
}

typedef struct
{
	NodePtr argumentNode;
	FuncCallExpr* funcCall;
	size_t argumentIndex;
	size_t memberCount;
} ExpandArgumentData;

static void CollapseSubscriptMemberAccess(NodePtr* node)
{
	assert(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;
	assert(memberAccess->start.type == Node_Subscript);
	SubscriptExpr* subscript = memberAccess->start.ptr;

	TypeInfo typeInfo = GetTypeInfoFromExpression(subscript->baseExpr);
	assert(typeInfo.isPointer);
	StructDeclStmt* type = typeInfo.pointerType;
	assert(type);

	subscript->indexExpr = AllocStructOffsetCalculation(
		subscript->indexExpr,
		GetIndexOfMember(type, memberAccess->varReference),
		CountStructMembers(type),
		subscript->lineNumber);

	subscript->typeBeforeCollapse = memberAccess->varReference->type;
	subscript->typeBeforeCollapse.expr = CopyASTNode(subscript->typeBeforeCollapse.expr);

	memberAccess->start = NULL_NODE;
	FreeASTNode(*node);
	*node = (NodePtr){.ptr = subscript, .type = Node_Subscript};
}

static NodePtr AllocStructMemberAssignmentExpr(
	NodePtr node,
	VarDeclStmt* member,
	size_t index,
	size_t memberCount,
	bool isLeft)
{
	switch (node.type)
	{
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;

		if (memberAccess->start.ptr)
		{
			if (memberAccess->start.type == Node_Subscript)
			{
				NodePtr new = CopyASTNode(node);
				assert(new.type == Node_MemberAccess);
				MemberAccessExpr* memberAccess = new.ptr;
				memberAccess->varReference = member;
				CollapseSubscriptMemberAccess(&new);
				return new;
			}
			else if (memberAccess->start.type == Node_FunctionCall)
			{
				if (isLeft)
					assert(0);

				assert(!"todo");
			}
			else
				assert(0);
		}
		else // its an identifier
		{
			VarDeclStmt* varDecl = memberAccess->parentReference != NULL
									   ? memberAccess->parentReference
									   : memberAccess->varReference;
			VarDeclStmt* instance = FindInstantiated(member->name, varDecl);
			assert(instance != NULL);
			return AllocIdentifier(instance, -1);
		}
	}
	case Node_FunctionCall:
	{
		if (isLeft)
			assert(0);

		if (index == 0)
			return CopyASTNode(node);
		else
		{
			FuncCallExpr* funcCall = node.ptr;
			assert(funcCall->baseExpr.type == Node_MemberAccess);
			MemberAccessExpr* identifier = funcCall->baseExpr.ptr;
			FuncDeclStmt* funcDecl = identifier->funcReference;
			assert(funcDecl != NULL);
			assert(funcDecl->globalReturn != NULL);

			VarDeclStmt* instance = FindInstantiated(member->name, funcDecl->globalReturn);
			assert(instance != NULL);
			return AllocIdentifier(instance, -1);
		}
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		NodePtr copy = CopyASTNode(node);
		SubscriptExpr* new = copy.ptr;
		new->indexExpr = AllocStructOffsetCalculation(new->indexExpr, index, memberCount, subscript->lineNumber);
		return copy;
	}
	default: INVALID_VALUE(node.type);
	}
}

static void ExpandArgument(VarDeclStmt* member, StructDeclStmt* parentType, size_t index, void* data)
{
	const ExpandArgumentData* d = data;

	NodePtr expr = AllocStructMemberAssignmentExpr(d->argumentNode, member, index, d->memberCount, false);
	ArrayInsert(&d->funcCall->arguments, &expr, d->argumentIndex + index);
}

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement);

static Result VisitFunctionCallArguments(FuncCallExpr* funcCall, NodePtr* containingStatement)
{
	assert(funcCall->baseExpr.type == Node_MemberAccess);
	MemberAccessExpr* identifier = funcCall->baseExpr.ptr;
	FuncDeclStmt* funcDecl = identifier->funcReference;
	assert(funcDecl != NULL);

	assert(funcDecl->oldParameters.length == funcCall->arguments.length);
	for (size_t paramIndex = 0, argIndex = 0; paramIndex < funcDecl->oldParameters.length; ++argIndex, ++paramIndex)
	{
		assert(funcCall->arguments.length > argIndex);
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

		ArrayRemove(&funcCall->arguments, argIndex);

		argIndex += ForEachStructMember(argAggregate, ExpandArgument,
			&(ExpandArgumentData){
				.argumentNode = argument,
				.funcCall = funcCall,
				.argumentIndex = argIndex,
				.memberCount = ForEachStructMember(argAggregate, NULL, NULL, NULL),
			},
			NULL);

		// account for for loop increment
		--argIndex;
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

static void GenerateStructMemberAssignment(VarDeclStmt* member, StructDeclStmt* parentType, size_t index, void* data)
{
	const GenerateStructMemberAssignmentData* d = data;
	NodePtr statement = AllocAssignmentStatement(
		AllocStructMemberAssignmentExpr(d->leftExpr, member, index, d->memberCount, true),
		AllocStructMemberAssignmentExpr(d->rightExpr, member, index, d->memberCount, false),
		-1);
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

	TypeInfo leftType = GetTypeInfoFromExpression(binary->left);
	TypeInfo rightType = GetTypeInfoFromExpression(binary->right);

	// special case for adding to pointer variables
	if (binary->operatorType == Binary_Add ||
		binary->operatorType == Binary_Subtract)
	{
		// only if one of them is a pointer
		if (leftType.isPointer && leftType.pointerType && !rightType.isPointer)
			binary->right = AllocMultiply(
				binary->right,
				AllocInteger(CountStructMembers(leftType.pointerType), binary->lineNumber),
				binary->lineNumber);

		if (rightType.isPointer && rightType.pointerType && !leftType.isPointer)
			binary->left = AllocMultiply(
				binary->left,
				AllocInteger(CountStructMembers(rightType.pointerType), binary->lineNumber),
				binary->lineNumber);
	}
	else if ((binary->operatorType == Binary_AddAssign ||
				 binary->operatorType == Binary_SubtractAssign) &&
			 leftType.isPointer && leftType.pointerType && !rightType.isPointer)
		binary->right = AllocMultiply(
			binary->right,
			AllocInteger(CountStructMembers(leftType.pointerType), binary->lineNumber),
			binary->lineNumber);

	if (leftType.effectiveType == NULL && rightType.effectiveType == NULL)
		return SUCCESS_RESULT;
	PROPAGATE_ERROR(CheckTypeConversion(leftType.effectiveType, rightType.effectiveType, binary->lineNumber));

	StructDeclStmt* type = leftType.effectiveType;

	if (binary->operatorType != Binary_Assignment)
		return ERROR_RESULT(
			AllocateString1Str(
				"Cannot use operator \"%s\" on aggregate types",
				GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
			binary->lineNumber, currentFilePath);

	BlockStmt* block = AllocBlockStmt(-1).ptr;

	if (binary->left.type != Node_MemberAccess &&
		binary->left.type != Node_Subscript)
		return ERROR_RESULT("Left operand of aggregate type assignment must be a variable",
			binary->lineNumber,
			currentFilePath);

	ForEachStructMember(type,
		GenerateStructMemberAssignment,
		&(GenerateStructMemberAssignmentData){
			.statements = &block->statements,
			.leftExpr = binary->left,
			.rightExpr = binary->right,
			.memberCount = CountStructMembers(type),
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

static Result VisitSubscriptExpression(SubscriptExpr* subscript, NodePtr* containingStatement)
{
	PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, containingStatement));

	TypeInfo type = GetTypeInfoFromExpression(subscript->baseExpr);
	if (!type.effectiveType)
		return VisitExpression(&subscript->baseExpr, containingStatement);

	if (!type.effectiveType->isArrayType)
		return ERROR_RESULT("Cannot index into non-array type",
			subscript->lineNumber,
			currentFilePath);

	// test_accessing_from_function.scy
	assert(subscript->baseExpr.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = subscript->baseExpr.ptr;

	// make the member access point to the ptr member inside the array type
	assert(memberAccess->varReference != NULL);
	if (!memberAccess->parentReference)
		memberAccess->parentReference = memberAccess->varReference;

	memberAccess->varReference = GetPtrMember(type.effectiveType);

	PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr, containingStatement));
	return SUCCESS_RESULT;
}

static void MakeMemberAccessesPointToInstantiated(NodePtr* node)
{
	assert(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	if (memberAccess->parentReference == NULL)
		return;

	// if its primitive type
	if (GetTypeInfoFromExpression(*node).effectiveType == NULL)
	{
		VarDeclStmt* instantiated = FindInstantiated(memberAccess->varReference->name, memberAccess->parentReference);
		assert(instantiated != NULL);
		NodePtr new = AllocIdentifier(instantiated, memberAccess->lineNumber);
		FreeASTNode(*node);
		*node = new;
	}
}

static Result VisitMemberAccess(NodePtr* node, NodePtr* containingStatement)
{
	assert(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	if (memberAccess->start.ptr != NULL)
	{
		PROPAGATE_ERROR(VisitExpression(&memberAccess->start, containingStatement));

		if (memberAccess->start.type == Node_Subscript)
		{
			// only if its a primitive type
			if (!GetTypeInfoFromType(memberAccess->varReference->type).effectiveType)
				CollapseSubscriptMemberAccess(node);
			return SUCCESS_RESULT;
		}
		else if (memberAccess->start.type == Node_FunctionCall)
		{
			// test_accessing_from_function.scy
			assert(!"todo");
		}
		else
			assert(0);

		assert(memberAccess->start.ptr == NULL);
	}

	MakeMemberAccessesPointToInstantiated(node);
	return SUCCESS_RESULT;
}

static Result VisitUnaryExpression(NodePtr* node, NodePtr* containingStatement)
{
	assert(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	PROPAGATE_ERROR(VisitExpression(&unary->expression, containingStatement));

	if (unary->operatorType == Unary_Increment ||
		unary->operatorType == Unary_Decrement)
	{
		TypeInfo typeInfo = GetTypeInfoFromExpression(unary->expression);
		if (typeInfo.isPointer && typeInfo.pointerType)
		{
			NodePtr old = *node;
			*node = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = unary->lineNumber,
					.operatorType = unary->operatorType == Unary_Increment ? Binary_AddAssign : Binary_SubtractAssign,
					.left = unary->expression,
					.right = AllocInteger(CountStructMembers(typeInfo.pointerType), unary->lineNumber),
				},
				sizeof(BinaryExpr), Node_Binary);

			unary->expression = NULL_NODE;
			FreeASTNode(old);
		}
	}

	return SUCCESS_RESULT;
}

static Result VisitSizeOfExpression(NodePtr* node, NodePtr* containingStatement)
{
	assert(node->type == Node_SizeOf);
	SizeOfExpr* sizeOf = node->ptr;

	PROPAGATE_ERROR(VisitExpression(&sizeOf->expr, containingStatement));

	uint64_t value = 1;

	TypeInfo typeInfo = GetTypeInfoFromExpression(sizeOf->expr);
	if (typeInfo.effectiveType)
		value = CountStructMembers(typeInfo.effectiveType);

	int lineNumber = sizeOf->lineNumber;
	FreeASTNode(*node);
	*node = AllocInteger(value, lineNumber);
	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		PROPAGATE_ERROR(VisitMemberAccess(node, containingStatement));
		break;
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node, containingStatement));
		break;
	case Node_Unary:
		PROPAGATE_ERROR(VisitUnaryExpression(node, containingStatement));
		break;
	case Node_FunctionCall:
		PROPAGATE_ERROR(VisitFunctionCallArguments(node->ptr, containingStatement));
		break;
	case Node_Subscript:
		PROPAGATE_ERROR(VisitSubscriptExpression(node->ptr, containingStatement));
		break;
	case Node_SizeOf:
		PROPAGATE_ERROR(VisitSizeOfExpression(node, containingStatement));
		break;
	case Node_Literal:
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
	StructDeclStmt* type,
	Array* instantiatedVariables,
	Array* destination,
	const size_t index)
{
	ForEachStructMember(type,
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

static void GetMemberAtIndexInternal(VarDeclStmt* varDecl, StructDeclStmt* parentType, size_t index, void* data)
{
	const GetStructMemberAtIndexData* d = data;
	if (index == d->index)
		*d->returnValue = varDecl;
}

static VarDeclStmt* GetMemberAtIndex(StructDeclStmt* type, size_t index)
{
	VarDeclStmt* varDecl = NULL;

	ForEachStructMember(type,
		GetMemberAtIndexInternal,
		&(GetStructMemberAtIndexData){
			.index = index,
			.returnValue = &varDecl,
		},
		NULL);

	return varDecl;
}

static Result VisitExpressionStatement(NodePtr* node)
{
	// todo a statement that does something could actually be removed e.g a[funcCall()].n;

	assert(node->type == Node_ExpressionStatement);
	ExpressionStmt* exprStmt = node->ptr;

	if (exprStmt->expr.type == Node_MemberAccess)
	{
		MemberAccessExpr* memberAccess = exprStmt->expr.ptr;
		if (memberAccess->start.ptr == NULL ||
			(memberAccess->start.type == Node_Subscript &&
				GetTypeInfoFromExpression(exprStmt->expr).effectiveType))
		{
			FreeASTNode(*node);
			*node = NULL_NODE;
			return SUCCESS_RESULT;
		}
	}

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

		StructDeclStmt* type = GetTypeInfoFromType(varDecl->type).effectiveType;
		if (type == NULL)
			continue;

		if (funcDecl->external)
			return ERROR_RESULT("External functions cannot have any parameters of an aggregate type",
				funcDecl->lineNumber,
				currentFilePath);

		if (varDecl->initializer.ptr != NULL)
			PROPAGATE_ERROR(CheckTypeConversion(
				GetTypeInfoFromExpression(varDecl->initializer).effectiveType,
				type,
				varDecl->lineNumber));

		InstantiateMembers(type, &varDecl->instantiatedVariables, &funcDecl->parameters, i + 1);
		ArrayAdd(&nodesToDelete, node);
		ArrayRemove(&funcDecl->parameters, i);
		i--;
	}

	StructDeclStmt* type = GetTypeInfoFromType(funcDecl->type).effectiveType;
	if (type != NULL)
	{
		if (funcDecl->external)
			return ERROR_RESULT("External functions cannot return an aggregate type", funcDecl->lineNumber, currentFilePath);

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

		VarDeclStmt* first = GetMemberAtIndex(type, 0);
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
	StructDeclStmt* exprType = GetTypeInfoFromExpression(returnStmt->expr).effectiveType;
	StructDeclStmt* returnType =
		returnStmt->function->oldType.expr.ptr != NULL
			? GetTypeInfoFromType(returnStmt->function->oldType).effectiveType
			: NULL;

	if (returnType == NULL && exprType == NULL)
	{
		if (returnStmt->expr.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&returnStmt->expr, node));

		return SUCCESS_RESULT;
	}

	PROPAGATE_ERROR(CheckTypeConversion(exprType, returnType, returnStmt->lineNumber));

	BlockStmt* block = AllocBlockStmt(returnStmt->lineNumber).ptr;

	VarDeclStmt* globalReturn = returnStmt->function->globalReturn;
	assert(globalReturn != NULL);
	NodePtr statement = AllocAssignmentStatement(
		AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = returnStmt->lineNumber,
				.start = NULL_NODE,
				.identifiers = (Array){.array = NULL},
				.funcReference = NULL,
				.typeReference = NULL,
				.varReference = globalReturn,
				.parentReference = NULL,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess),
		returnStmt->expr,
		returnStmt->lineNumber);
	PROPAGATE_ERROR(VisitStatement(&statement));
	ArrayAdd(&block->statements, &statement);

	ArrayAdd(&block->statements, node);
	*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

	VarDeclStmt* firstReturnVariable = *(VarDeclStmt**)globalReturn->instantiatedVariables.array[0];
	returnStmt->expr = AllocIdentifier(firstReturnVariable, returnStmt->lineNumber);
	return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(NodePtr* node)
{
	assert(node->type == Node_VariableDeclaration);
	VarDeclStmt* varDecl = node->ptr;

	StructDeclStmt* type = GetTypeInfoFromType(varDecl->type).effectiveType;
	if (type == NULL)
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
	InstantiateMembers(type, &varDecl->instantiatedVariables, &block->statements, block->statements.length);

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
