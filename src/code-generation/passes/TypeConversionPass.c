#include "TypeConversionPass.h"

#include "StringUtils.h"

typedef struct
{
	PrimitiveType effectiveType;
	PrimitiveType underlyingType;
	bool isPointer;
} TypeInfo;

static const char* currentFilePath = NULL;

static TypeInfo NonPointerType(PrimitiveType type)
{
	return (TypeInfo){
		.effectiveType = type,
		.underlyingType = type,
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
			.underlyingType = literal->primitiveType,
			.isPointer = isPointer,
		};
	}
	else
	{
		assert(isPointer);
		return (TypeInfo){
			.effectiveType = Primitive_Int,
			.underlyingType = Primitive_Void,
			.isPointer = true,
		};
	}
}

static void VisitLiteral(LiteralExpr* literal, TypeInfo* outType)
{
	if (outType == NULL)
		return;

	switch (literal->type)
	{
	case Literal_Float: *outType = NonPointerType(Primitive_Float); break;
	case Literal_Int: *outType = NonPointerType(Primitive_Int); break;
	case Literal_String: *outType = NonPointerType(Primitive_String); break;
	case Literal_Boolean:
		literal->type = Literal_Int;
		literal->intValue = literal->boolean ? 1 : 0;
		*outType = NonPointerType(Primitive_Bool);
		break;

	case Literal_Identifier:
		NodePtr reference = literal->identifier.reference;

		Type* type = NULL;
		if (reference.type == Node_VariableDeclaration)
			type = &((VarDeclStmt*)reference.ptr)->type;
		else if (reference.type == Node_FunctionDeclaration)
			type = &((FuncDeclStmt*)reference.ptr)->type;
		else
			assert(0);

		*outType = GetType(*type);
		break;

	default: INVALID_VALUE(literal->type);
	}
}

static Result VisitExpression(NodePtr* node, TypeInfo* outType);

static Result ConvertExpression(
	NodePtr* expr,
	PrimitiveType exprType,
	PrimitiveType targetType,
	int lineNumber,
	const char* errorMessage);

static Result VisitFunctionCall(FuncCallExpr* funcCall, TypeInfo* outType)
{
	assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
	const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

	if (outType != NULL)
		*outType = GetType(funcDecl->type);

	assert(funcDecl->parameters.length == funcCall->arguments.length);
	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];
		assert(node->type == Node_VariableDeclaration);
		const VarDeclStmt* varDecl = node->ptr;

		NodePtr* expr = funcCall->arguments.array[i];

		TypeInfo exprType;
		PROPAGATE_ERROR(VisitExpression(expr, &exprType));
		PROPAGATE_ERROR(ConvertExpression(expr, exprType.effectiveType, GetType(varDecl->type).effectiveType, funcCall->lineNumber, NULL));
	}

	return SUCCESS_RESULT;
}

static NodePtr AllocFloatToIntConversion(const NodePtr expr, const int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_BitOr,
			.right = expr,
			.left = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_Int,
					.intValue = 0,
				},
				sizeof(LiteralExpr), Node_Literal),
		},
		sizeof(BinaryExpr), Node_Binary);
}

static NodePtr AllocIntToBoolConversion(const NodePtr expr, const int lineNumber)
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

static TokenType PrimitiveTypeToTokenType(const PrimitiveType primitiveType)
{
	switch (primitiveType)
	{
	case Primitive_Void: return Token_Void;
	case Primitive_Float: return Token_Float;
	case Primitive_Int: return Token_Int;
	case Primitive_Bool: return Token_Bool;
	case Primitive_String: return Token_String;
	default: INVALID_VALUE(primitiveType);
	}
}

static Result ConvertExpression(
	NodePtr* expr,
	const PrimitiveType exprType,
	const PrimitiveType targetType,
	const int lineNumber,
	const char* errorMessage)
{
	if (exprType == Primitive_Void || targetType == Primitive_Void)
		return SUCCESS_RESULT;

	if (exprType == targetType)
		return SUCCESS_RESULT;

	switch (targetType)
	{
	case Primitive_Float:
		if (exprType != Primitive_Int)
			goto convertError;
		return SUCCESS_RESULT;

	case Primitive_Int:
		if (exprType != Primitive_Float)
			goto convertError;
		*expr = AllocFloatToIntConversion(*expr, lineNumber);
		return SUCCESS_RESULT;

	case Primitive_Bool:
		if (exprType != Primitive_Float && exprType != Primitive_Int)
			goto convertError;
		*expr = AllocIntToBoolConversion(*expr, lineNumber);
		return SUCCESS_RESULT;

	case Primitive_String:
		goto convertError;

	default: INVALID_VALUE(targetType);
	}

convertError:
	return ERROR_RESULT(
		errorMessage != NULL
			? errorMessage
			: AllocateString2Str(
				  "Cannot convert type \"%s\" to \"%s\"",
				  GetTokenTypeString(PrimitiveTypeToTokenType(exprType)),
				  GetTokenTypeString(PrimitiveTypeToTokenType(targetType))),
		lineNumber, currentFilePath);
}

static Result VisitBinaryExpression(const NodePtr* node, TypeInfo* outType)
{
	assert(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	TypeInfo leftTypeInfo;
	TypeInfo rightTypeInfo;
	PROPAGATE_ERROR(VisitExpression(&binary->left, &leftTypeInfo));
	PROPAGATE_ERROR(VisitExpression(&binary->right, &rightTypeInfo));
	PrimitiveType leftType = leftTypeInfo.effectiveType;
	PrimitiveType rightType = rightTypeInfo.effectiveType;

	const char* errorMessage = AllocateString3Str(
		"Cannot use operator \"%s\" on type \"%s\" and \"%s\"",
		GetTokenTypeString(binaryOperatorToTokenType[binary->operatorType]),
		GetTokenTypeString(PrimitiveTypeToTokenType(leftType)),
		GetTokenTypeString(PrimitiveTypeToTokenType(rightType)));

	switch (binary->operatorType)
	{
	case Binary_IsEqual:
	case Binary_NotEqual:
	{
		if (leftType != rightType &&
			(leftType != Primitive_Int || rightType != Primitive_Float) &&
			(leftType != Primitive_Float || rightType != Primitive_Int))
			return ERROR_RESULT(errorMessage, binary->lineNumber, currentFilePath);

		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

	case Binary_BoolAnd:
	case Binary_BoolOr:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, Primitive_Bool, binary->lineNumber, errorMessage));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, Primitive_Bool, binary->lineNumber, errorMessage));
		if (outType != NULL) *outType = NonPointerType(Primitive_Bool);
		break;
	}

		// relational
	case Binary_GreaterThan:
	case Binary_GreaterOrEqual:
	case Binary_LessThan:
	case Binary_LessOrEqual:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, Primitive_Float, binary->lineNumber, errorMessage));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, Primitive_Float, binary->lineNumber, errorMessage));
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
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, Primitive_Float, binary->lineNumber, errorMessage));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, Primitive_Float, binary->lineNumber, errorMessage));

		PrimitiveType type;
		if (leftType == Primitive_Int && rightType == Primitive_Int &&
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
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, Primitive_Float, binary->lineNumber, errorMessage));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, Primitive_Float, binary->lineNumber, errorMessage));
		if (outType != NULL) *outType = NonPointerType(Primitive_Int);
		break;
	}

		// assignment
	case Binary_Assignment:
	{
		if (binary->left.type == Node_Literal)
		{
			LiteralExpr* literal = binary->left.ptr;
			if (literal->type != Literal_Identifier)
				goto assignmentError;
		}
		else if (binary->left.type != Node_Subscript)
			goto assignmentError;

		PROPAGATE_ERROR(ConvertExpression(
			&binary->right,
			rightType,
			leftType,
			binary->lineNumber,
			NULL));

		if (outType != NULL) *outType = NonPointerType(leftType);
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

		if (outType != NULL) *outType = NonPointerType(leftType);
		break;
	}

	assignmentError:
		return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

	default: INVALID_VALUE(binary->operatorType);
	}

	return SUCCESS_RESULT;
}

static Result VisitUnaryExpression(NodePtr* node, TypeInfo* outType)
{
	assert(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	TypeInfo exprTypeInfo;
	PROPAGATE_ERROR(VisitExpression(&unary->expression, &exprTypeInfo));
	PrimitiveType exprType = exprTypeInfo.effectiveType;

	const char* errorMessage = AllocateString2Str(
		"Cannot use operator \"%s\" on type \"%s\"",
		GetTokenTypeString(unaryOperatorToTokenType[unary->operatorType]),
		GetTokenTypeString(PrimitiveTypeToTokenType(exprType)));

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
			Primitive_Float,
			unary->lineNumber,
			errorMessage));

		if (outType != NULL) *outType = NonPointerType(exprType);
		break;
	}
	case Unary_Negate:
	{
		PROPAGATE_ERROR(ConvertExpression(
			&unary->expression,
			exprType,
			Primitive_Bool,
			unary->lineNumber,
			errorMessage));

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
	PROPAGATE_ERROR(VisitExpression(&subscript->addressExpr, &addressType));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->addressExpr,
		addressType.effectiveType,
		Primitive_Int,
		subscript->lineNumber, NULL));

	if (addressType.isPointer)
		*outType = NonPointerType(addressType.underlyingType);
	else
		*outType = NonPointerType(Primitive_Void);

	TypeInfo indexType;
	PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, &indexType));
	PROPAGATE_ERROR(ConvertExpression(
		&subscript->indexExpr,
		indexType.effectiveType,
		Primitive_Int,
		subscript->lineNumber, NULL));

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
	case Primitive_Void:
		return ERROR_RESULT("\"void\" is not allowed here", varDecl->lineNumber, currentFilePath);

	case Primitive_Float:
	case Primitive_Int:
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Int,
				.intValue = 0,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;

	case Primitive_Bool:
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Boolean,
				.boolean = false,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;

	case Primitive_String:
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_String,
				.string = AllocateString(""),
			},
			sizeof(LiteralExpr), Node_Literal);
		break;

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
			type.effectiveType,
			GetType(varDecl->type).effectiveType,
			varDecl->lineNumber,
			NULL));
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node);

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
		PROPAGATE_ERROR(ConvertExpression(&ifStmt->expr, type.effectiveType, Primitive_Bool, ifStmt->lineNumber, NULL));
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
		PROPAGATE_ERROR(ConvertExpression(&whileStmt->expr, type.effectiveType, Primitive_Bool, whileStmt->lineNumber, NULL));
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
