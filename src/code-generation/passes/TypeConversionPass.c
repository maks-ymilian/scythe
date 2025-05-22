#include "TypeConversionPass.h"

#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"

typedef struct
{
	PrimitiveType effectiveType;
	PrimitiveType pointerType;
	bool pointerTypeIsStruct;
	bool isPointer;
} TypeInfo;

static const char* currentFilePath = NULL;

static Result VisitExpression(NodePtr* node, TypeInfo* outType);
static Result ConvertExpression(NodePtr* expr, TypeInfo exprType, TypeInfo targetType, int lineNumber);
static Result VisitStatement(const NodePtr* node);

static TypeInfo NonPointerType(PrimitiveType type)
{
	return (TypeInfo){
		.effectiveType = type,
		.pointerType = type,
		.isPointer = false,
	};
}

static TypeInfo GetType(const Type type)
{
	const bool isPointer = type.modifier == TypeModifier_Pointer;

	if (type.expr.type == Node_Literal)
	{
		const LiteralExpr* literal = type.expr.ptr;
		assert(literal->type == Literal_PrimitiveType);

		return (TypeInfo){
			.effectiveType = isPointer ? Primitive_Int : literal->primitiveType,
			.pointerType = literal->primitiveType,
			.isPointer = isPointer,
		};
	}
	else
	{
		assert(isPointer);
		return (TypeInfo){
			.effectiveType = Primitive_Int,
			.pointerType = Primitive_Any,
			.pointerTypeIsStruct = true,
			.isPointer = true,
		};
	}
}

static char* AllocTypeName(TypeInfo typeInfo)
{
	PrimitiveType type = typeInfo.isPointer ? typeInfo.pointerType : typeInfo.effectiveType;
	const char* name = GetTokenTypeString(primitiveTypeToTokenType[type]);

	const char structName[] = "(struct type)*";

	size_t length = strlen(name) + sizeof(structName);

	char* out = malloc(length);
	if (typeInfo.isPointer)
	{
		if (typeInfo.pointerTypeIsStruct)
			snprintf(out, length, "%s", structName);
		else
			snprintf(out, length, "%s*", name);
	}
	else
		snprintf(out, length, "%s", name);

	return out;
}

static void VisitLiteral(LiteralExpr* literal, TypeInfo* outType)
{
	if (outType == NULL)
		return;

	switch (literal->type)
	{
	case Literal_Float: *outType = NonPointerType(Primitive_Float); break;
	case Literal_Int: *outType = NonPointerType(Primitive_Int); break;
	case Literal_String: *outType = NonPointerType(Primitive_Int); break;
	case Literal_Boolean:
		literal->type = Literal_Int;
		literal->intValue = literal->boolean ? 1 : 0;
		*outType = NonPointerType(Primitive_Bool);
		break;
	default: INVALID_VALUE(literal->type);
	}
}

static void VisitMemberAccess(MemberAccessExpr* memberAccess, TypeInfo* outType)
{
	Type* type = NULL;
	if (memberAccess->funcReference != NULL)
		type = &memberAccess->funcReference->type;
	else if (memberAccess->varReference != NULL)
		type = &memberAccess->varReference->type;
	else
		assert(0);

	if (outType != NULL)
		*outType = GetType(*type);
}

static Result VisitFunctionCall(FuncCallExpr* funcCall, TypeInfo* outType)
{
	assert(funcCall->baseExpr.type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
	assert(memberAccess->funcReference != NULL);
	const FuncDeclStmt* funcDecl = memberAccess->funcReference;

	if (outType != NULL)
		*outType = GetType(funcDecl->type);

	assert(funcDecl->variadic
			   ? funcCall->arguments.length >= funcDecl->parameters.length
			   : funcCall->arguments.length == funcDecl->parameters.length);

	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		NodePtr* arg = funcCall->arguments.array[i];

		TypeInfo paramType;
		if (i < funcDecl->parameters.length)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			assert(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			paramType = GetType(varDecl->type);
		}
		else
		{
			assert(funcDecl->variadic);
			paramType = (TypeInfo){
				.effectiveType = Primitive_Any,
				.pointerType = Primitive_Any,
				.isPointer = false,
			};
		}

		TypeInfo exprType;
		PROPAGATE_ERROR(VisitExpression(arg, &exprType));
		PROPAGATE_ERROR(ConvertExpression(arg, exprType, paramType, funcCall->lineNumber));
	}

	return SUCCESS_RESULT;
}

static NodePtr AllocBoolConversion(const NodePtr expr, const int lineNumber)
{
	return AllocASTNode(
		&(UnaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Unary_Negate,
			.expression = AllocASTNode(
				&(UnaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Unary_Negate,
					.expression = expr,
				},
				sizeof(UnaryExpr), Node_Unary),
		},
		sizeof(UnaryExpr), Node_Unary);
}

static Result ConvertExpression(NodePtr* expr, TypeInfo exprType, TypeInfo targetType, int lineNumber)
{
	if (exprType.effectiveType == Primitive_Void || targetType.effectiveType == Primitive_Void)
		goto convertError;

	if (exprType.effectiveType == Primitive_Any || targetType.effectiveType == Primitive_Any ||
		(!exprType.pointerTypeIsStruct && exprType.pointerType == Primitive_Any) ||
		(!targetType.pointerTypeIsStruct && targetType.pointerType == Primitive_Any))
		return SUCCESS_RESULT;

	if (exprType.effectiveType == targetType.effectiveType &&
		exprType.pointerType == targetType.pointerType &&
		exprType.isPointer == targetType.isPointer)
		return SUCCESS_RESULT;

	switch (targetType.effectiveType)
	{
	case Primitive_Float:
		if (exprType.effectiveType != Primitive_Int)
			goto convertError;
		return SUCCESS_RESULT;

	case Primitive_Int:
		if (exprType.isPointer && targetType.isPointer &&
			exprType.pointerType != targetType.pointerType)
			goto convertError;

		if (exprType.effectiveType != Primitive_Float &&
			exprType.effectiveType != Primitive_Int)
			goto convertError;

		*expr = AllocIntConversion(*expr, lineNumber);
		return SUCCESS_RESULT;

	case Primitive_Bool:
		if (exprType.effectiveType != Primitive_Float && exprType.effectiveType != Primitive_Int)
			goto convertError;
		*expr = AllocBoolConversion(*expr, lineNumber);
		return SUCCESS_RESULT;
	default: INVALID_VALUE(targetType.effectiveType);
	}

convertError:
	char* exprName = AllocTypeName(exprType);
	char* targetName = AllocTypeName(targetType);
	char* message = AllocateString2Str("Cannot convert type \"%s\" to \"%s\"", exprName, targetName);
	free(exprName);
	free(targetName);
	return ERROR_RESULT(message, lineNumber, currentFilePath);
}

static Result VisitBinaryExpression(NodePtr* node, TypeInfo* outType)
{
	assert(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	TypeInfo leftType;
	TypeInfo rightType;
	PROPAGATE_ERROR(VisitExpression(&binary->left, &leftType));
	PROPAGATE_ERROR(VisitExpression(&binary->right, &rightType));

	switch (binary->operatorType)
	{
	case Binary_IsEqual:
	case Binary_NotEqual:
	{
		if (leftType.effectiveType != rightType.effectiveType &&
			(leftType.effectiveType != Primitive_Int || rightType.effectiveType != Primitive_Float) &&
			(leftType.effectiveType != Primitive_Float || rightType.effectiveType != Primitive_Int))
			return ERROR_RESULT(
				AllocateString3Str(
					"Cannot use operator \"%s\" on type \"%s\" and \"%s\"",
					GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType]),
					GetTokenTypeString(primitiveTypeToTokenType[leftType.effectiveType]),
					GetTokenTypeString(primitiveTypeToTokenType[rightType.effectiveType])),
				binary->lineNumber,
				currentFilePath);

		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

	case Binary_BoolAnd:
	case Binary_BoolOr:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, NonPointerType(Primitive_Bool), binary->lineNumber));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, NonPointerType(Primitive_Bool), binary->lineNumber));
		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

		// relational
	case Binary_GreaterThan:
	case Binary_GreaterOrEqual:
	case Binary_LessThan:
	case Binary_LessOrEqual:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, NonPointerType(Primitive_Float), binary->lineNumber));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, NonPointerType(Primitive_Float), binary->lineNumber));
		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

		// arithmetic
	case Binary_Add:
	case Binary_Subtract:
	case Binary_Multiply:
	case Binary_Divide:
	case Binary_Exponentiation:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, NonPointerType(Primitive_Float), binary->lineNumber));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, NonPointerType(Primitive_Float), binary->lineNumber));

		if (leftType.effectiveType == Primitive_Int && rightType.effectiveType == Primitive_Int &&
			(binary->operatorType == Binary_Divide ||
				binary->operatorType == Binary_Exponentiation))
			*node = AllocIntConversion(*node, binary->lineNumber);

		PrimitiveType type;
		if (leftType.effectiveType == Primitive_Int && rightType.effectiveType == Primitive_Int &&
			binary->operatorType != Binary_Divide &&
			binary->operatorType != Binary_Exponentiation)
			type = Primitive_Int;
		else
			type = Primitive_Float;

		if (outType != NULL) *outType = NonPointerType(type);
		break;
	}

		// integer arithmetic
	case Binary_Modulo:
	case Binary_LeftShift:
	case Binary_RightShift:
	case Binary_BitAnd:
	case Binary_BitOr:
	case Binary_XOR:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, NonPointerType(Primitive_Float), binary->lineNumber));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, NonPointerType(Primitive_Float), binary->lineNumber));
		if (outType != NULL) *outType = NonPointerType(Primitive_Int);
		break;
	}

		// assignment
	case Binary_Assignment:
	{
		if (binary->left.type != Node_MemberAccess &&
			binary->left.type != Node_Subscript &&
			binary->left.type != Node_FunctionCall) // slider()
			return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

		PROPAGATE_ERROR(ConvertExpression(
			&binary->right,
			rightType,
			leftType,
			binary->lineNumber));

		if (outType != NULL) *outType = leftType;
		break;
	}

		// compound assignment
	case Binary_AddAssign:
	case Binary_SubtractAssign:
	case Binary_MultiplyAssign:
	case Binary_DivideAssign:
	case Binary_ModuloAssign:
	case Binary_ExponentAssign:
	case Binary_BitAndAssign:
	case Binary_BitOrAssign:
	case Binary_XORAssign:
	{
		binary->right = AllocASTNode(
			&(BinaryExpr){
				.lineNumber = binary->lineNumber,
				.operatorType = getCompoundAssignmentOperator[binary->operatorType],
				.right = binary->right,
				.left = CopyASTNode(binary->left),
			},
			sizeof(BinaryExpr), Node_Binary);
		binary->operatorType = Binary_Assignment;

		PROPAGATE_ERROR(VisitBinaryExpression(node, NULL));

		if (outType != NULL) *outType = leftType;
		break;
	}

	default: INVALID_VALUE(binary->operatorType);
	}

	return SUCCESS_RESULT;
}

static Result VisitUnaryExpression(NodePtr* node, TypeInfo* outType)
{
	assert(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	TypeInfo exprType;
	PROPAGATE_ERROR(VisitExpression(&unary->expression, &exprType));

	switch (unary->operatorType)
	{
	case Unary_Decrement:
	case Unary_Increment:
	{
		*node = AllocASTNode(
			&(BinaryExpr){
				.lineNumber = unary->lineNumber,
				.operatorType =
					unary->operatorType == Unary_Increment
						? Binary_AddAssign
						: Binary_SubtractAssign,
				.left = unary->expression,
				.right = AllocASTNode(
					&(LiteralExpr){
						.lineNumber = unary->lineNumber,
						.type = Literal_Int,
						.intValue = 1,
					},
					sizeof(LiteralExpr), Node_Literal),
			},
			sizeof(BinaryExpr), Node_Binary);
		unary->expression = NULL_NODE;
		FreeASTNode((NodePtr){.ptr = unary, .type = Node_Unary});

		PROPAGATE_ERROR(VisitBinaryExpression(node, outType));
		break;
	}
	case Unary_Minus:
	case Unary_Plus:
	{
		PROPAGATE_ERROR(ConvertExpression(
			&unary->expression,
			exprType,
			NonPointerType(Primitive_Float),
			unary->lineNumber));

		if (outType != NULL) *outType = exprType;
		break;
	}
	case Unary_Negate:
	{
		PROPAGATE_ERROR(ConvertExpression(
			&unary->expression,
			exprType,
			NonPointerType(Primitive_Bool),
			unary->lineNumber));

		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

	default: INVALID_VALUE(unary->operatorType);
	}

	return SUCCESS_RESULT;
}

static Result VisitSubscriptExpression(SubscriptExpr* subscript, TypeInfo* outType)
{
	TypeInfo addressType;
	PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr, &addressType));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->baseExpr,
		addressType,
		NonPointerType(Primitive_Int),
		subscript->lineNumber));

	TypeInfo indexType;
	PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, &indexType));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->indexExpr,
		indexType,
		NonPointerType(Primitive_Int),
		subscript->lineNumber));

	if (outType != NULL)
	{
		if (subscript->typeBeforeCollapse.expr.ptr)
			*outType = GetType(subscript->typeBeforeCollapse);
		else
		{
			if (addressType.isPointer)
				*outType = NonPointerType(addressType.pointerType);
			else
				*outType = NonPointerType(Primitive_Any);
		}
	}
	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node, TypeInfo* outType)
{
	switch (node->type)
	{
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node, outType));
		break;
	case Node_Unary:
		PROPAGATE_ERROR(VisitUnaryExpression(node, outType));
		break;
	case Node_MemberAccess:
		VisitMemberAccess(node->ptr, outType);
		break;
	case Node_Literal:
		VisitLiteral(node->ptr, outType);
		break;
	case Node_FunctionCall:
		PROPAGATE_ERROR(VisitFunctionCall(node->ptr, outType));
		break;
	case Node_Subscript:
		PROPAGATE_ERROR(VisitSubscriptExpression(node->ptr, outType));
		break;
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

static Result AddVariableInitializer(VarDeclStmt* varDecl)
{
	assert(varDecl != NULL);

	if (varDecl->initializer.ptr != NULL)
		return SUCCESS_RESULT;

	switch (GetType(varDecl->type).effectiveType)
	{
	case Primitive_Any:
	case Primitive_Float:
	case Primitive_Int:
	{
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Int,
				.intValue = 0,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;
	}
	case Primitive_Bool:
	{
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Boolean,
				.boolean = false,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;
	}
	default: INVALID_VALUE(GetType(varDecl->type).effectiveType);
	}

	return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(VarDeclStmt* varDecl, const bool addInitializer)
{
	if (addInitializer)
		PROPAGATE_ERROR(AddVariableInitializer(varDecl));

	if (varDecl->initializer.ptr != NULL)
	{
		TypeInfo type;
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, &type));
		PROPAGATE_ERROR(ConvertExpression(
			&varDecl->initializer,
			type,
			GetType(varDecl->type),
			varDecl->lineNumber));
	}

	return SUCCESS_RESULT;
}

static Result VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];
		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = node->ptr;

		PROPAGATE_ERROR(VisitVariableDeclaration(varDecl, false));
	}

	PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Null:
		break;
	case Node_FunctionDeclaration:
	{
		PROPAGATE_ERROR(VisitFunctionDeclaration(node->ptr));
		break;
	}
	case Node_VariableDeclaration:
	{
		PROPAGATE_ERROR(VisitVariableDeclaration(node->ptr, true));
		break;
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* expressionStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&expressionStmt->expr, NULL));
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
		TypeInfo type;
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, &type));
		PROPAGATE_ERROR(ConvertExpression(&ifStmt->expr, type, NonPointerType(Primitive_Bool), ifStmt->lineNumber));
		break;
	}
	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&section->block));
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		TypeInfo type;
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr, &type));
		PROPAGATE_ERROR(ConvertExpression(&whileStmt->expr, type, NonPointerType(Primitive_Bool), whileStmt->lineNumber));
		break;
	}
	case Node_Import:
		break;
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

Result TypeConversionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
