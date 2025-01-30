#include "TypeConversionPass.h"

#include "StringUtils.h"

static const char* currentFilePath = NULL;

static PrimitiveType GetIdentifierType(const IdentifierReference identifier)
{
	const NodePtr* type;
	switch (identifier.reference.type)
	{
	case Node_VariableDeclaration:
		const VarDeclStmt* varDecl = identifier.reference.ptr;
		type = &varDecl->type;
		break;
	case Node_FunctionDeclaration:
		const FuncDeclStmt* funcDecl = identifier.reference.ptr;
		type = &funcDecl->type;
		break;
	default: INVALID_VALUE(identifier.reference.type);
	}

	assert(type->type == Node_Literal);
	const LiteralExpr* literal = type->ptr;
	assert(literal->type == Literal_PrimitiveType);
	return literal->primitiveType;
}

static void VisitLiteral(LiteralExpr* literal, PrimitiveType* outType)
{
	if (outType == NULL)
		return;

	switch (literal->type)
	{
	case Literal_Identifier: *outType = GetIdentifierType(literal->identifier); break;
	case Literal_Float: *outType = Primitive_Float; break;
	case Literal_Int: *outType = Primitive_Int; break;
	case Literal_String: *outType = Primitive_String; break;
	case Literal_Boolean:
		literal->type = Literal_Int;
		literal->intValue = literal->boolean ? 1 : 0;
		*outType = Primitive_Bool;
		break;
	default: INVALID_VALUE(literal->type);
	}
}

static PrimitiveType GetType(const NodePtr type)
{
	assert(type.type == Node_Literal);
	const LiteralExpr* literal = type.ptr;
	assert(literal->type == Literal_PrimitiveType);
	return literal->primitiveType;
}

static size_t Max(const size_t a, const size_t b)
{
	return a > b ? a : b;
}

static Result VisitExpression(const NodePtr* node, PrimitiveType* outType);
static Result ConvertExpression(
	NodePtr* expr,
	PrimitiveType exprType,
	PrimitiveType targetType,
	int lineNumber,
	const char* errorMessage);

static Result VisitFunctionCall(FuncCallExpr* funcCall, PrimitiveType* outType)
{
	assert(funcCall->identifier.reference.type == Node_FunctionDeclaration);
	const FuncDeclStmt* funcDecl = funcCall->identifier.reference.ptr;

	if (outType != NULL) *outType = GetType(funcDecl->type);

	for (size_t i = 0; i < Max(funcDecl->parameters.length, funcCall->arguments.length); ++i)
	{
		if (i >= funcDecl->parameters.length)
			goto ArgumentCountError;

		const NodePtr* node = funcDecl->parameters.array[i];
		assert(node->type == Node_VariableDeclaration);
		const VarDeclStmt* varDecl = node->ptr;

		if (i >= funcCall->arguments.length)
		{
			if (varDecl->initializer.ptr == NULL)
				goto ArgumentCountError;

			const NodePtr node = CopyASTNode(varDecl->initializer);
			ArrayAdd(&funcCall->arguments, &node);
			assert(i < funcCall->arguments.length);
		}

		NodePtr* expr = funcCall->arguments.array[i];

		PrimitiveType exprType;
		PROPAGATE_ERROR(VisitExpression(expr, &exprType));
		PROPAGATE_ERROR(ConvertExpression(expr, exprType, GetType(varDecl->type), funcCall->lineNumber, NULL));
	}

	return SUCCESS_RESULT;

ArgumentCountError:
	return ERROR_RESULT("Function is called with wrong number of arguments",
		funcCall->lineNumber, currentFilePath);
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
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_NotEqual,
			.left = expr,
			.right = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_Int,
					.intValue = 0,
				},
				sizeof(LiteralExpr), Node_Literal),
		},
		sizeof(BinaryExpr), Node_Binary);
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

static Result VisitExpression(const NodePtr* node, PrimitiveType* outType);

static Result ConvertExpression(
	NodePtr* expr,
	const PrimitiveType exprType,
	const PrimitiveType targetType,
	const int lineNumber,
	const char* errorMessage)
{
	if (exprType == targetType)
		return SUCCESS_RESULT;

	if (errorMessage == NULL)
		errorMessage = AllocateString2Str(
			"Cannot convert type \"%s\" to \"%s\"",
			GetTokenTypeString(PrimitiveTypeToTokenType(exprType)),
			GetTokenTypeString(PrimitiveTypeToTokenType(targetType)));

	const Result error = ERROR_RESULT(errorMessage, lineNumber, currentFilePath);

	switch (targetType)
	{
	case Primitive_Float:
		if (exprType != Primitive_Int)
			return error;
		return SUCCESS_RESULT;

	case Primitive_Int:
		if (exprType != Primitive_Float)
			return error;
		*expr = AllocFloatToIntConversion(*expr, lineNumber);
		return SUCCESS_RESULT;

	case Primitive_Bool:
		if (exprType != Primitive_Float && exprType != Primitive_Int)
			return error;
		*expr = AllocIntToBoolConversion(*expr, lineNumber);
		return SUCCESS_RESULT;

	case Primitive_String:
	case Primitive_Void:
		return error;

	default: INVALID_VALUE(targetType);
	}
}

static Result VisitBinaryExpression(const NodePtr* node, PrimitiveType* outType)
{
	assert(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	PrimitiveType leftType;
	PrimitiveType rightType;
	PROPAGATE_ERROR(VisitExpression(&binary->left, &leftType));
	PROPAGATE_ERROR(VisitExpression(&binary->right, &rightType));

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

		if (outType != NULL) *outType = Primitive_Bool;
		break;
	}

	case Binary_BoolAnd:
	case Binary_BoolOr:
	{
		PROPAGATE_ERROR(ConvertExpression(&binary->left, leftType, Primitive_Bool, binary->lineNumber, errorMessage));
		PROPAGATE_ERROR(ConvertExpression(&binary->right, rightType, Primitive_Bool, binary->lineNumber, errorMessage));
		if (outType != NULL) *outType = Primitive_Bool;
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
		if (outType != NULL) *outType = Primitive_Bool;
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

		if (outType != NULL) *outType = type;
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
		if (outType != NULL) *outType = Primitive_Int;
		break;
	}

		// assignment
	case Binary_Assignment:
	{
		LiteralExpr* literal = NULL;
		if (binary->left.type == Node_Literal)
			literal = binary->left.ptr;

		if (literal == NULL || literal->type != Literal_Identifier)
			return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

		PrimitiveType variableType;
		VisitLiteral(literal, &variableType);
		PROPAGATE_ERROR(ConvertExpression(
			&binary->right,
			rightType,
			variableType,
			binary->lineNumber,
			NULL));

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
		LiteralExpr* literal = NULL;
		if (binary->left.type == Node_Literal)
			literal = binary->left.ptr;

		if (literal == NULL || literal->type != Literal_Identifier)
			return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

		binary->right = AllocASTNode(
			&(BinaryExpr){
				.lineNumber = binary->lineNumber,
				.operatorType = getCompoundAssignmentOperator[binary->operatorType],
				.right = binary->right,
				.left = CopyASTNode((NodePtr){.ptr = literal, .type = Node_Literal}),
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

static Result VisitExpression(const NodePtr* node, PrimitiveType* outType)
{
	switch (node->type)
	{
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node, outType));
		break;
	case Node_Literal:
		VisitLiteral(node->ptr, outType);
		break;
	case Node_FunctionCall:
		PROPAGATE_ERROR(VisitFunctionCall(node->ptr, outType));
		break;
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

static Result AddVariableInitializer(VarDeclStmt* varDecl)
{
	if (varDecl->array)
		return SUCCESS_RESULT;

	if (varDecl->initializer.ptr != NULL)
		return SUCCESS_RESULT;

	switch (GetType(varDecl->type))
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

	default: INVALID_VALUE(GetType(varDecl->type));
	}

	return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(VarDeclStmt* varDecl, const bool addInitializer)
{
	if (addInitializer)
		PROPAGATE_ERROR(AddVariableInitializer(varDecl));

	if (varDecl->initializer.ptr != NULL)
	{
		PrimitiveType type;
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, &type));
		PROPAGATE_ERROR(ConvertExpression(
			&varDecl->initializer,
			type,
			GetType(varDecl->type),
			varDecl->lineNumber,
			NULL));
	}

	if (varDecl->arrayLength.ptr != NULL)
	{
		PrimitiveType type;
		PROPAGATE_ERROR(VisitExpression(&varDecl->arrayLength, &type));
		PROPAGATE_ERROR(ConvertExpression(&varDecl->arrayLength, type, Primitive_Int, varDecl->lineNumber, NULL));
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node);

static Result VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
	bool encounteredOptional = false;
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];
		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = node->ptr;

		PROPAGATE_ERROR(VisitVariableDeclaration(varDecl, false));

		if (varDecl->initializer.ptr == NULL)
		{
			if (encounteredOptional)
				return ERROR_RESULT(
					"Optional parameters must be at the end of the parameter list",
					varDecl->lineNumber,
					currentFilePath);
		}
		else
			encounteredOptional = true;
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
		const ExpressionStmt* expressionStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&expressionStmt->expr, NULL));
		break;
	}
	case Node_Block:
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
		PrimitiveType type;
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, &type));
		PROPAGATE_ERROR(ConvertExpression(&ifStmt->expr, type, Primitive_Bool, ifStmt->lineNumber, NULL));
		break;
	}
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_Block);
	const BlockStmt* block = section->block.ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));

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
			PROPAGATE_ERROR(VisitSection(stmt->ptr));
			break;
		case Node_Import:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}
	return SUCCESS_RESULT;
}

Result TypeConversionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	return SUCCESS_RESULT;
}
