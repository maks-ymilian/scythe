#include "ChainedAssignmentPass.h"

#include "Common.h"

static const char* currentFilePath = NULL;

static Result VisitExpression(NodePtr* node, bool parentIsExprStmt);
static Result VisitStatement(NodePtr* node);

static void ConvertIncrement(NodePtr* node)
{
	ASSERT(node->type == Node_Unary);
	UnaryExpr* unary = node->ptr;

	if (unary->operatorType != Unary_Decrement &&
		unary->operatorType != Unary_Increment)
		return;

	*node = AllocASTNode(
		&(BinaryExpr){
			.lineNumber = unary->lineNumber,
			.operatorType =
				unary->operatorType == Unary_Increment
					? Binary_AddAssign
					: Binary_SubtractAssign,
			.left = unary->expression,
			.right = AllocUInt64Integer(1, unary->lineNumber),
		},
		sizeof(BinaryExpr), Node_Binary);
	unary->expression = NULL_NODE;
	FreeASTNode((NodePtr){.ptr = unary, .type = Node_Unary});
}

static Result ConvertCompoundAssignment(NodePtr* node)
{
	ASSERT(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	if (binary->operatorType != Binary_AddAssign &&
		binary->operatorType != Binary_SubtractAssign &&
		binary->operatorType != Binary_MultiplyAssign &&
		binary->operatorType != Binary_DivideAssign &&
		binary->operatorType != Binary_ModuloAssign &&
		binary->operatorType != Binary_ExponentAssign &&
		binary->operatorType != Binary_BitAndAssign &&
		binary->operatorType != Binary_BitOrAssign &&
		binary->operatorType != Binary_XORAssign)
		return SUCCESS_RESULT;

	if (binary->left.type == Node_BlockExpression)
		return ERROR_RESULT("Left operand of assignment must be a variable", binary->lineNumber, currentFilePath);

	binary->right = AllocASTNode(
		&(BinaryExpr){
			.lineNumber = binary->lineNumber,
			.operatorType = getCompoundAssignmentOperator[binary->operatorType],
			.right = binary->right,
			.left = CopyASTNode(binary->left),
		},
		sizeof(BinaryExpr), Node_Binary);
	binary->operatorType = Binary_Assignment;

	return SUCCESS_RESULT;
}

static Result VisitBinaryExpression(NodePtr* node, bool parentIsExprStmt)
{
	ASSERT(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	PROPAGATE_ERROR(VisitExpression(&binary->left, false));
	PROPAGATE_ERROR(VisitExpression(&binary->right, false));

	if (binary->left.type == Node_Binary &&
		((BinaryExpr*)binary->left.ptr)->operatorType == Binary_Assignment)
		return SUCCESS_RESULT;

	NodePtr* nested = NULL;
	if (parentIsExprStmt)
	{
		if (binary->operatorType != Binary_Assignment)
			return SUCCESS_RESULT;

		if (binary->right.type != Node_Binary)
			return SUCCESS_RESULT;

		nested = &binary->right;
	}
	else
		nested = node;

	ASSERT(nested->type == Node_Binary);
	BinaryExpr* nestedBinary = nested->ptr;
	if (nestedBinary->operatorType != Binary_Assignment)
		return SUCCESS_RESULT;

	BlockStmt* blockStmt = AllocASTNode(
		&(BlockStmt){
			.lineNumber = nestedBinary->lineNumber,
			.statements = AllocateArray(sizeof(NodePtr)),
		},
		sizeof(BlockStmt), Node_BlockStatement)
							   .ptr;
	NodePtr blockExpr = AllocASTNode(
		&(BlockExpr){
			.type = AllocTypeFromExpr(*nested, nestedBinary->lineNumber),
			.block = (NodePtr){.ptr = blockStmt, .type = Node_BlockStatement},
		},
		sizeof(BlockExpr), Node_BlockExpression);

	NodePtr exprStmt = AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = nestedBinary->lineNumber,
			.expr = *nested,
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
	ArrayAdd(&blockStmt->statements, &exprStmt);

	NodePtr returnStmt = AllocASTNode(
		&(ReturnStmt){
			.lineNumber = nestedBinary->lineNumber,
			.expr = CopyASTNode(nestedBinary->left),
		},
		sizeof(ReturnStmt), Node_Return);
	ArrayAdd(&blockStmt->statements, &returnStmt);

	*nested = blockExpr;

	return SUCCESS_RESULT;
}

static Result VisitExpression(NodePtr* node, bool parentIsExprStmt)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_Binary:
	{
		PROPAGATE_ERROR(ConvertCompoundAssignment(node));
		PROPAGATE_ERROR(VisitBinaryExpression(node, parentIsExprStmt));
		break;
	}
	case Node_Unary:
	{
		ConvertIncrement(node);
		if (node->type == Node_Unary)
		{
			UnaryExpr* unary = node->ptr;
			PROPAGATE_ERROR(VisitExpression(&unary->expression, false));
		}
		else
			PROPAGATE_ERROR(VisitExpression(node, false));
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		for (size_t i = 0; i < funcCall->arguments.length; i++)
			PROPAGATE_ERROR(VisitExpression(funcCall->arguments.array[i], false));
		break;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&memberAccess->start, false));
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr, false));
		PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr, false));
		break;
	}
	case Node_SizeOf:
	{
		SizeOfExpr* sizeOf = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&sizeOf->expr, false));
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&block->block));
		break;
	}
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_StructDeclaration:
	case Node_LoopControl:
	case Node_Import:
	case Node_Input:
	case Node_Null:
		break;
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, false));
		break;
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&exprStmt->expr, true));
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			PROPAGATE_ERROR(VisitStatement(funcDecl->parameters.array[i]));
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
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
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, false));
		break;
	}
	case Node_Return:
	{
		ReturnStmt* returnStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&returnStmt->expr, false));
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
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr, false));
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		break;
	}
	case Node_For:
	{
		ForStmt* forStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&forStmt->initialization));
		PROPAGATE_ERROR(VisitExpression(&forStmt->condition, false));
		PROPAGATE_ERROR(VisitExpression(&forStmt->increment, false));
		PROPAGATE_ERROR(VisitStatement(&forStmt->stmt));
		break;
	}
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

Result ChainedAssignmentPass(const AST* ast)
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
