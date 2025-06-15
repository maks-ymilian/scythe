#include "MemberExpansionPass.h"

#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"

static Array nodesToDelete;

static const char* currentFilePath = NULL;

static Result VisitExpression(NodePtr* node, NodePtr* containingStatement);

static VarDeclStmt* FindInstantiated(const char* name, const VarDeclStmt* aggregateVarDecl)
{
	ASSERT(aggregateVarDecl != NULL);
	for (size_t i = 0; i < aggregateVarDecl->instantiatedVariables.length; ++i)
	{
		VarDeclStmt** varDecl = aggregateVarDecl->instantiatedVariables.array[i];
		if (strcmp((*varDecl)->name, name) == 0)
			return *varDecl;
	}
	return NULL;
}

static Result CheckTypeConversion(StructTypeInfo from, StructTypeInfo to, const int lineNumber)
{
	if (!from.effectiveType && to.effectiveType)
		return ERROR_RESULT(
			"Cannot convert a non-aggregate type to an aggregate type",
			lineNumber, currentFilePath);

	if (from.effectiveType && !to.effectiveType)
		return ERROR_RESULT(
			"Cannot convert an aggregate type to a non-aggregate type",
			lineNumber, currentFilePath);

	if (from.isPointer && to.isPointer &&
		from.pointerType != to.pointerType &&
		from.pointerType && to.pointerType)
		return ERROR_RESULT(
			"Different pointer types are only compatible with each other if at least one of the types is \"any\"",
			lineNumber, currentFilePath);

	if (from.effectiveType != to.effectiveType)
	{
		if (from.effectiveType->isArrayType && to.effectiveType->isArrayType)
		{
			VarDeclStmt* fromPtr = GetPtrMember(from.effectiveType);
			VarDeclStmt* toPtr = GetPtrMember(to.effectiveType);
			ASSERT(fromPtr && toPtr);

			bool allowed = false;

			if (!allowed && fromPtr->type.expr.type == Node_Literal)
			{
				LiteralExpr* literal = fromPtr->type.expr.ptr;
				allowed =
					literal->type == Literal_PrimitiveType &&
					literal->primitiveType == Primitive_Any;
			}
			if (!allowed && toPtr->type.expr.type == Node_Literal)
			{
				LiteralExpr* literal = toPtr->type.expr.ptr;
				allowed =
					literal->type == Literal_PrimitiveType &&
					literal->primitiveType == Primitive_Any;
			}

			if (!allowed)
				return ERROR_RESULT(
					"Different array types are only compatible with each other if at least one of the types is \"any[]\"",
					lineNumber, currentFilePath);
		}
		else
			return ERROR_RESULT(
				"Cannot convert between incompatible aggregate types",
				lineNumber, currentFilePath);
	}

	return SUCCESS_RESULT;
}

typedef void (*StructMemberFunc)(VarDeclStmt* varDecl, size_t index, void* data);
static size_t ForEachStructMember(
	StructDeclStmt* type,
	const StructMemberFunc func,
	void* data,
	size_t* currentIndex)
{
	ASSERT(type != NULL);

	size_t index = 0;
	if (currentIndex == NULL)
		currentIndex = &index;

	for (size_t i = 0; i < type->members.length; i++)
	{
		const NodePtr* memberNode = type->members.array[i];
		ASSERT(memberNode->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = memberNode->ptr;

		StructDeclStmt* memberType = GetStructTypeInfoFromType(varDecl->type).effectiveType;
		if (memberType == NULL)
		{
			if (func != NULL)
				func(varDecl, *currentIndex, data);
			(*currentIndex)++;
		}
		else
			ForEachStructMember(memberType, func, data, currentIndex);
	}

	return *currentIndex;
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

static NodePtr AllocStructOffsetCalculation(NodePtr offset, size_t memberIndex, size_t memberCount, int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_Add,
			.left = AllocMultiply(offset, AllocSizeInteger(memberCount, lineNumber), lineNumber),
			.right = AllocSizeInteger(memberIndex, lineNumber),
		},
		sizeof(BinaryExpr), Node_Binary);
}

typedef struct
{
	VarDeclStmt* member;
	size_t* returnValue;
} GetIndexOfMemberData;

static void GetIndexOfMemberInternal(VarDeclStmt* varDecl, size_t index, void* data)
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
	ASSERT(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;
	ASSERT(memberAccess->start.type == Node_Subscript);
	SubscriptExpr* subscript = memberAccess->start.ptr;

	StructTypeInfo typeInfo = GetStructTypeInfoFromExpr(subscript->baseExpr);
	ASSERT(typeInfo.isPointer);
	StructDeclStmt* type = typeInfo.pointerType;
	ASSERT(type);

	subscript->indexExpr = AllocStructOffsetCalculation(
		AllocIntConversion(subscript->indexExpr, subscript->lineNumber),
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
	size_t memberCount)
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
				ASSERT(new.type == Node_MemberAccess);
				MemberAccessExpr* memberAccess = new.ptr;
				memberAccess->varReference = member;
				CollapseSubscriptMemberAccess(&new);
				return new;
			}
			else
				UNREACHABLE();
		}
		else // its an identifier
		{
			VarDeclStmt* varDecl = memberAccess->parentReference != NULL
									   ? memberAccess->parentReference
									   : memberAccess->varReference;
			VarDeclStmt* instance = FindInstantiated(member->name, varDecl);
			ASSERT(instance != NULL);
			return AllocIdentifier(instance, -1);
		}
	}
	case Node_FunctionCall:
	{
		if (index == 0)
			return CopyASTNode(node);
		else
		{
			FuncCallExpr* funcCall = node.ptr;
			ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
			MemberAccessExpr* identifier = funcCall->baseExpr.ptr;
			FuncDeclStmt* funcDecl = identifier->funcReference;
			ASSERT(funcDecl != NULL);
			ASSERT(funcDecl->globalReturn != NULL);

			VarDeclStmt* instance = FindInstantiated(member->name, funcDecl->globalReturn);
			ASSERT(instance != NULL);
			return AllocIdentifier(instance, -1);
		}
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		NodePtr copy = CopyASTNode(node);
		SubscriptExpr* new = copy.ptr;
		new->indexExpr = AllocStructOffsetCalculation(
			AllocIntConversion(new->indexExpr, subscript->lineNumber),
			index,
			memberCount,
			subscript->lineNumber);
		return copy;
	}
	default: INVALID_VALUE(node.type);
	}
}

static void ExpandArgument(VarDeclStmt* member, size_t index, void* data)
{
	const ExpandArgumentData* d = data;

	NodePtr expr = AllocStructMemberAssignmentExpr(d->argumentNode, member, index, d->memberCount);
	ArrayInsert(&d->funcCall->arguments, &expr, d->argumentIndex + index);
}

static size_t Max(size_t a, size_t b)
{
	return a > b ? a : b;
}

static Result VisitFunctionCallArguments(FuncCallExpr* funcCall, NodePtr* containingStatement)
{
	ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
	MemberAccessExpr* identifier = funcCall->baseExpr.ptr;
	FuncDeclStmt* funcDecl = identifier->funcReference;
	ASSERT(funcDecl != NULL);

	ASSERT(funcDecl->variadic
			   ? funcCall->arguments.length >= funcDecl->oldParameters.length
			   : funcCall->arguments.length == funcDecl->oldParameters.length);

	size_t length = Max(funcCall->arguments.length, funcDecl->oldParameters.length);
	for (size_t paramIndex = 0, argIndex = 0; paramIndex < length; ++argIndex, ++paramIndex)
	{
		ASSERT(argIndex < funcCall->arguments.length);
		PROPAGATE_ERROR(VisitExpression(funcCall->arguments.array[argIndex], containingStatement));
		NodePtr argument = *(NodePtr*)funcCall->arguments.array[argIndex];

		StructTypeInfo argType = GetStructTypeInfoFromExpr(argument);
		StructTypeInfo paramType;
		if (paramIndex < funcDecl->oldParameters.length)
		{
			NodePtr* paramNode = funcDecl->oldParameters.array[paramIndex];
			ASSERT(paramNode->type == Node_VariableDeclaration);
			VarDeclStmt* param = paramNode->ptr;
			paramType = GetStructTypeInfoFromType(param->type);
		}
		else
		{
			ASSERT(funcDecl->variadic);
			paramType = (StructTypeInfo){
				.effectiveType = NULL,
				.pointerType = NULL,
				.isPointer = false,
			};
		}

		if (argType.effectiveType == NULL && paramType.effectiveType == NULL)
			continue;
		PROPAGATE_ERROR(CheckTypeConversion(argType, paramType, funcCall->lineNumber));

		ArrayRemove(&funcCall->arguments, argIndex);

		argIndex += ForEachStructMember(argType.effectiveType, ExpandArgument,
			&(ExpandArgumentData){
				.argumentNode = argument,
				.funcCall = funcCall,
				.argumentIndex = argIndex,
				.memberCount = ForEachStructMember(argType.effectiveType, NULL, NULL, NULL),
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

static void GenerateStructMemberAssignment(VarDeclStmt* member, size_t index, void* data)
{
	const GenerateStructMemberAssignmentData* d = data;
	NodePtr statement = AllocAssignmentStatement(
		AllocStructMemberAssignmentExpr(d->leftExpr, member, index, d->memberCount),
		AllocStructMemberAssignmentExpr(d->rightExpr, member, index, d->memberCount),
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
	ASSERT(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	if (binary->operatorType == Binary_Assignment &&
		binary->left.type != Node_MemberAccess &&
		binary->left.type != Node_Subscript &&
		binary->left.type != Node_FunctionCall)
		return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

	PROPAGATE_ERROR(VisitExpression(&binary->left, containingStatement));
	PROPAGATE_ERROR(VisitExpression(&binary->right, containingStatement));

	StructTypeInfo leftType = GetStructTypeInfoFromExpr(binary->left);
	StructTypeInfo rightType = GetStructTypeInfoFromExpr(binary->right);
	PROPAGATE_ERROR(CheckTypeConversion(rightType, leftType, binary->lineNumber));

	// special case for adding to pointer variables
	if (binary->operatorType == Binary_Add ||
		binary->operatorType == Binary_Subtract)
	{
		// only if one of them is a pointer
		if (leftType.isPointer && leftType.pointerType && !rightType.isPointer)
			binary->right = AllocMultiply(
				binary->right,
				AllocSizeInteger(CountStructMembers(leftType.pointerType), binary->lineNumber),
				binary->lineNumber);

		if (rightType.isPointer && rightType.pointerType && !leftType.isPointer)
			binary->left = AllocMultiply(
				binary->left,
				AllocSizeInteger(CountStructMembers(rightType.pointerType), binary->lineNumber),
				binary->lineNumber);
	}
	else if ((binary->operatorType == Binary_AddAssign ||
				 binary->operatorType == Binary_SubtractAssign) &&
			 leftType.isPointer && leftType.pointerType && !rightType.isPointer)
		binary->right = AllocMultiply(
			binary->right,
			AllocSizeInteger(CountStructMembers(leftType.pointerType), binary->lineNumber),
			binary->lineNumber);

	if (leftType.effectiveType == NULL && rightType.effectiveType == NULL)
		return SUCCESS_RESULT;

	StructDeclStmt* type = leftType.effectiveType;

	if (binary->operatorType != Binary_Assignment)
		return ERROR_RESULT(
			AllocateString1Str(
				"Cannot use operator \"%s\" on aggregate types",
				GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType])),
			binary->lineNumber, currentFilePath);

	BlockStmt* block = AllocBlockStmt(-1).ptr;

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
	ASSERT(containingStatement->type == Node_ExpressionStatement);

	ArrayAdd(&nodesToDelete, containingStatement);
	*containingStatement = (NodePtr){.ptr = block, .type = Node_BlockStatement};
	return SUCCESS_RESULT;
}

static Result VisitSubscriptExpression(SubscriptExpr* subscript, NodePtr* containingStatement)
{
	PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, containingStatement));

	StructTypeInfo type = GetStructTypeInfoFromExpr(subscript->baseExpr);
	if (!type.effectiveType)
		return VisitExpression(&subscript->baseExpr, containingStatement);

	ASSERT(type.effectiveType->isArrayType);

	PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr, containingStatement));
	return SUCCESS_RESULT;
}

static void MakeMemberAccessesPointToInstantiated(NodePtr* node)
{
	ASSERT(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	if (memberAccess->parentReference == NULL)
		return;

	// if its primitive type
	if (GetStructTypeInfoFromExpr(*node).effectiveType == NULL)
	{
		VarDeclStmt* instantiated = FindInstantiated(memberAccess->varReference->name, memberAccess->parentReference);
		ASSERT(instantiated != NULL);
		NodePtr new = AllocIdentifier(instantiated, memberAccess->lineNumber);
		FreeASTNode(*node);
		*node = new;
	}
}

static Result VisitMemberAccess(NodePtr* node, NodePtr* containingStatement)
{
	ASSERT(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	if (memberAccess->start.ptr != NULL)
	{
		PROPAGATE_ERROR(VisitExpression(&memberAccess->start, containingStatement));

		if (memberAccess->start.type == Node_Subscript)
		{
			// only if its a primitive type
			if (!GetStructTypeInfoFromType(memberAccess->varReference->type).effectiveType)
				CollapseSubscriptMemberAccess(node);
			return SUCCESS_RESULT;
		}
		else
			UNREACHABLE();

		ASSERT(memberAccess->start.ptr == NULL);
	}

	MakeMemberAccessesPointToInstantiated(node);
	return SUCCESS_RESULT;
}

static Result VisitUnaryExpression(NodePtr* node, NodePtr* containingStatement)
{
	ASSERT(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	PROPAGATE_ERROR(VisitExpression(&unary->expression, containingStatement));

	if (unary->operatorType == Unary_Increment ||
		unary->operatorType == Unary_Decrement)
	{
		StructTypeInfo typeInfo = GetStructTypeInfoFromExpr(unary->expression);
		if (typeInfo.isPointer && typeInfo.pointerType)
		{
			NodePtr old = *node;
			*node = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = unary->lineNumber,
					.operatorType = unary->operatorType == Unary_Increment ? Binary_AddAssign : Binary_SubtractAssign,
					.left = unary->expression,
					.right = AllocSizeInteger(CountStructMembers(typeInfo.pointerType), unary->lineNumber),
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
	ASSERT(node->type == Node_SizeOf);
	SizeOfExpr* sizeOf = node->ptr;

	StructTypeInfo typeInfo = (StructTypeInfo){.effectiveType = NULL};
	if (sizeOf->expr.ptr)
	{
		PROPAGATE_ERROR(VisitExpression(&sizeOf->expr, containingStatement));
		typeInfo = GetStructTypeInfoFromExpr(sizeOf->expr);
	}
	else
		typeInfo = GetStructTypeInfoFromType(sizeOf->type);

	uint64_t value = typeInfo.effectiveType ? CountStructMembers(typeInfo.effectiveType) : 1;
	int lineNumber = sizeOf->lineNumber;
	FreeASTNode(*node);
	*node = AllocUInt64Integer(value, lineNumber);
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

static void InstantiateMember(VarDeclStmt* varDecl, size_t index, void* data)
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

static void GetMemberAtIndexInternal(VarDeclStmt* varDecl, size_t index, void* data)
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

	ASSERT(node->type == Node_ExpressionStatement);
	ExpressionStmt* exprStmt = node->ptr;

	if (exprStmt->expr.type == Node_MemberAccess)
	{
		MemberAccessExpr* memberAccess = exprStmt->expr.ptr;
		if (memberAccess->start.ptr == NULL ||
			(memberAccess->start.type == Node_Subscript &&
				GetStructTypeInfoFromExpr(exprStmt->expr).effectiveType))
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
	ASSERT(node->type == Node_FunctionDeclaration);
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

		ASSERT(node->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = node->ptr;

		StructTypeInfo type = GetStructTypeInfoFromType(varDecl->type);
		if (type.effectiveType == NULL)
			continue;

		if (funcDecl->modifiers.externalValue)
			return ERROR_RESULT("External functions cannot have any parameters of an aggregate type",
				funcDecl->lineNumber,
				currentFilePath);

		if (varDecl->initializer.ptr != NULL)
			PROPAGATE_ERROR(CheckTypeConversion(
				GetStructTypeInfoFromExpr(varDecl->initializer),
				type,
				varDecl->lineNumber));

		InstantiateMembers(type.effectiveType, &varDecl->instantiatedVariables, &funcDecl->parameters, i + 1);
		ArrayAdd(&nodesToDelete, node);
		ArrayRemove(&funcDecl->parameters, i);
		i--;
	}

	StructDeclStmt* type = GetStructTypeInfoFromType(funcDecl->type).effectiveType;
	if (type != NULL)
	{
		if (funcDecl->modifiers.externalValue)
			return ERROR_RESULT("External functions cannot return an aggregate type", funcDecl->lineNumber, currentFilePath);

		BlockStmt* block = AllocBlockStmt(funcDecl->lineNumber).ptr;

		NodePtr globalReturn = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = funcDecl->lineNumber,
				.type.expr = CopyASTNode(funcDecl->type.expr),
				.type.modifier = funcDecl->type.modifier,
				.name = AllocateString("return"),
				.initializer = NULL_NODE,
				.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
				.uniqueName = -1,
			},
			sizeof(VarDeclStmt), Node_VariableDeclaration);
		funcDecl->globalReturn = globalReturn.ptr;
		PROPAGATE_ERROR(VisitStatement(&globalReturn));
		ArrayAdd(&block->statements, &globalReturn);

		ArrayAdd(&block->statements, node);
		*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

		VarDeclStmt* first = GetMemberAtIndex(type, 0);
		ASSERT(first != NULL);
		ASSERT(funcDecl->oldType.expr.ptr == NULL);
		funcDecl->oldType = funcDecl->type;
		funcDecl->type.expr = CopyASTNode(first->type.expr);
		funcDecl->type.modifier = first->type.modifier;
	}

	if (!funcDecl->modifiers.externalValue)
	{
		ASSERT(funcDecl->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
	}

	return SUCCESS_RESULT;
}

static Result VisitReturnStatement(NodePtr* node)
{
	ASSERT(node->type == Node_Return);
	ReturnStmt* returnStmt = node->ptr;

	StructTypeInfo exprType = (StructTypeInfo){.effectiveType = NULL};
	if (returnStmt->expr.ptr)
		exprType = GetStructTypeInfoFromExpr(returnStmt->expr);

	StructTypeInfo returnType = (StructTypeInfo){.effectiveType = NULL};
	FuncDeclStmt* function = NULL;
	if (returnStmt->function.type == Node_FunctionDeclaration)
	{
		function = returnStmt->function.ptr;
		if (function->oldType.expr.ptr)
			returnType = GetStructTypeInfoFromType(function->oldType);
	}
	else
		ASSERT(returnStmt->function.type == Node_Section); // you can return from a section

	if (returnType.effectiveType == NULL && exprType.effectiveType == NULL)
	{
		if (returnStmt->expr.ptr != NULL)
			PROPAGATE_ERROR(VisitExpression(&returnStmt->expr, node));

		return SUCCESS_RESULT;
	}

	PROPAGATE_ERROR(CheckTypeConversion(exprType, returnType, returnStmt->lineNumber));

	BlockStmt* block = AllocBlockStmt(returnStmt->lineNumber).ptr;

	VarDeclStmt* globalReturn = function->globalReturn;
	ASSERT(globalReturn != NULL);
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
	ASSERT(node->type == Node_VariableDeclaration);
	VarDeclStmt* varDecl = node->ptr;

	StructDeclStmt* type = GetStructTypeInfoFromType(varDecl->type).effectiveType;
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

	if (varDecl->modifiers.externalValue)
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
	case Node_LoopControl:
	case Node_Import:
	case Node_Input:
	case Node_Null:
		break;
	case Node_VariableDeclaration:
	{
		PROPAGATE_ERROR(VisitVariableDeclaration(node));
		break;
	}
	case Node_StructDeclaration:
	{
		ArrayAdd(&nodesToDelete, node);
		*node = NULL_NODE;
		break;
	}
	case Node_ExpressionStatement:
	{
		PROPAGATE_ERROR(VisitExpressionStatement(node));
		break;
	}
	case Node_FunctionDeclaration:
	{
		PROPAGATE_ERROR(VisitFunctionDeclaration(node));
		break;
	}
	case Node_BlockStatement:
	{
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, node));
		PROPAGATE_ERROR(CheckTypeConversion(GetStructTypeInfoFromExpr(ifStmt->expr), (StructTypeInfo){.effectiveType = NULL}, ifStmt->lineNumber));
		break;
	}
	case Node_Return:
	{
		PROPAGATE_ERROR(VisitReturnStatement(node));
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&section->block));
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr, node));
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		PROPAGATE_ERROR(CheckTypeConversion(GetStructTypeInfoFromExpr(whileStmt->expr), (StructTypeInfo){.effectiveType = NULL}, whileStmt->lineNumber));
		break;
	}
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

		ASSERT(node->type == Node_Module);
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
