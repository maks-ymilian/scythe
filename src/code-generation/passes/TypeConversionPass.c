#include "TypeConversionPass.h"

#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"

static const char* currentFilePath = NULL;

static Result VisitExpression(NodePtr* node);
static Result ConvertExpression(NodePtr* expr, PrimitiveTypeInfo exprType, PrimitiveTypeInfo targetType, int lineNumber);
static Result VisitStatement(const NodePtr* node);

static PrimitiveTypeInfo NonPointerType(PrimitiveType type)
{
	return (PrimitiveTypeInfo){
		.effectiveType = type,
		.pointerType = type,
		.isPointer = false,
	};
}

static char* AllocTypeName(PrimitiveTypeInfo typeInfo)
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

static Result VisitFunctionCall(FuncCallExpr* funcCall)
{
	ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
	ASSERT(memberAccess->funcReference != NULL);
	const FuncDeclStmt* funcDecl = memberAccess->funcReference;

	ASSERT(funcDecl->variadic
			   ? funcCall->arguments.length >= funcDecl->parameters.length
			   : funcCall->arguments.length == funcDecl->parameters.length);

	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		NodePtr* arg = funcCall->arguments.array[i];

		PrimitiveTypeInfo paramType;
		if (i < funcDecl->parameters.length)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			paramType = GetPrimitiveTypeInfoFromType(varDecl->type);
		}
		else
		{
			ASSERT(funcDecl->variadic);
			paramType = (PrimitiveTypeInfo){
				.effectiveType = Primitive_Any,
				.pointerType = Primitive_Any,
				.isPointer = false,
			};
		}

		PrimitiveTypeInfo typeInfo = GetPrimitiveTypeInfoFromExpr(*arg);
		PROPAGATE_ERROR(VisitExpression(arg));
		PROPAGATE_ERROR(ConvertExpression(arg, typeInfo, paramType, funcCall->lineNumber));
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

static Result ConvertExpression(NodePtr* expr, PrimitiveTypeInfo exprType, PrimitiveTypeInfo targetType, int lineNumber)
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
	{
		if (exprType.effectiveType != Primitive_Int)
			goto convertError;
		return SUCCESS_RESULT;
	}
	case Primitive_Int:
	{
		if (exprType.isPointer && targetType.isPointer &&
			exprType.pointerType != targetType.pointerType)
			goto convertError;

		if (exprType.effectiveType != Primitive_Float &&
			exprType.effectiveType != Primitive_Int)
			goto convertError;

		*expr = AllocIntConversion(*expr, lineNumber);
		return SUCCESS_RESULT;
	}
	case Primitive_Bool:
	{
		if (exprType.effectiveType != Primitive_Float && exprType.effectiveType != Primitive_Int)
			goto convertError;
		*expr = AllocBoolConversion(*expr, lineNumber);
		return SUCCESS_RESULT;
	}
	default: INVALID_VALUE(targetType.effectiveType);
	}

convertError:
{
	char* exprName = AllocTypeName(exprType);
	char* targetName = AllocTypeName(targetType);
	char* message = AllocateString2Str("Cannot convert type \"%s\" to \"%s\"", exprName, targetName);
	free(exprName);
	free(targetName);
	return ERROR_RESULT(message, lineNumber, currentFilePath);
}
}

static Result VisitBinaryExpression(NodePtr* node)
{
	ASSERT(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	PrimitiveTypeInfo leftType = GetPrimitiveTypeInfoFromExpr(binary->left);
	PrimitiveTypeInfo rightType = GetPrimitiveTypeInfoFromExpr(binary->right);
	PROPAGATE_ERROR(VisitExpression(&binary->left));
	PROPAGATE_ERROR(VisitExpression(&binary->right));

	switch (binary->operatorType)
	{
	case Binary_IsEqual:
	case Binary_NotEqual:
	{
		if (leftType.effectiveType != Primitive_Any &&
			rightType.effectiveType != Primitive_Any)
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
		}
		break;
	}

	case Binary_BoolAnd:
	case Binary_BoolOr:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, NonPointerType(Primitive_Bool), binary->lineNumber));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, NonPointerType(Primitive_Bool), binary->lineNumber));
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

		PROPAGATE_ERROR(VisitBinaryExpression(node));
		break;
	}

	default: INVALID_VALUE(binary->operatorType);
	}

	return SUCCESS_RESULT;
}

static Result VisitUnaryExpression(NodePtr* node)
{
	ASSERT(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	PrimitiveTypeInfo exprType = GetPrimitiveTypeInfoFromExpr(unary->expression);
	PROPAGATE_ERROR(VisitExpression(&unary->expression));

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

		PROPAGATE_ERROR(VisitBinaryExpression(node));
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
		break;
	}
	case Unary_Negate:
	{
		PROPAGATE_ERROR(ConvertExpression(
			&unary->expression,
			exprType,
			NonPointerType(Primitive_Bool),
			unary->lineNumber));
		break;
	}

	default: INVALID_VALUE(unary->operatorType);
	}

	return SUCCESS_RESULT;
}

static Result VisitSubscriptExpression(SubscriptExpr* subscript)
{
	PrimitiveTypeInfo baseType = GetPrimitiveTypeInfoFromExpr(subscript->baseExpr);
	PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->baseExpr,
		baseType,
		NonPointerType(Primitive_Int),
		subscript->lineNumber));

	PrimitiveTypeInfo indexType = GetPrimitiveTypeInfoFromExpr(subscript->indexExpr);
	PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->indexExpr,
		indexType,
		NonPointerType(Primitive_Int),
		subscript->lineNumber));

	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess:
		break;
	case Node_Binary:
	{
		PROPAGATE_ERROR(VisitBinaryExpression(node));
		break;
	}
	case Node_Unary:
	{
		PROPAGATE_ERROR(VisitUnaryExpression(node));
		break;
	}
	case Node_Literal:
	{
		LiteralExpr* literal = node->ptr;
		if (literal->type == Literal_Boolean)
		{
			literal->type = Literal_Int;
			literal->intValue = literal->boolean ? 1 : 0;
		}
		break;
	}
	case Node_FunctionCall:
	{
		PROPAGATE_ERROR(VisitFunctionCall(node->ptr));
		break;
	}
	case Node_Subscript:
	{
		PROPAGATE_ERROR(VisitSubscriptExpression(node->ptr));
		break;
	}
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

static Result AddVariableInitializer(VarDeclStmt* varDecl)
{
	ASSERT(varDecl != NULL);

	if (varDecl->initializer.ptr != NULL)
		return SUCCESS_RESULT;

	switch (GetPrimitiveTypeInfoFromType(varDecl->type).effectiveType)
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
	default: INVALID_VALUE(GetPrimitiveTypeInfoFromType(varDecl->type).effectiveType);
	}

	return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(VarDeclStmt* varDecl, const bool addInitializer)
{
	if (addInitializer)
		PROPAGATE_ERROR(AddVariableInitializer(varDecl));

	if (varDecl->initializer.ptr != NULL)
	{
		PrimitiveTypeInfo initializerType = GetPrimitiveTypeInfoFromExpr(varDecl->initializer);
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer));
		PROPAGATE_ERROR(ConvertExpression(
			&varDecl->initializer,
			initializerType,
			GetPrimitiveTypeInfoFromType(varDecl->type),
			varDecl->lineNumber));
	}

	return SUCCESS_RESULT;
}

static Result VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];
		ASSERT(node->type == Node_VariableDeclaration);
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
		PROPAGATE_ERROR(VisitExpression(&expressionStmt->expr));
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
		PrimitiveTypeInfo exprType = GetPrimitiveTypeInfoFromExpr(ifStmt->expr);
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr));
		PROPAGATE_ERROR(ConvertExpression(&ifStmt->expr, exprType, NonPointerType(Primitive_Bool), ifStmt->lineNumber));
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
		PrimitiveTypeInfo exprType = GetPrimitiveTypeInfoFromExpr(whileStmt->expr);
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr));
		PROPAGATE_ERROR(ConvertExpression(&whileStmt->expr, exprType, NonPointerType(Primitive_Bool), whileStmt->lineNumber));
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

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
