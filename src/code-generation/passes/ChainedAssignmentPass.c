#include "ChainedAssignmentPass.h"

#include "Common.h"

static void VisitExpression(NodePtr* node, bool parentIsExprStmt);
static void VisitStatement(NodePtr* node);

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

static void ConvertCompoundAssignment(NodePtr* node)
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
		return;

	binary->right = AllocASTNode(
		&(BinaryExpr){
			.lineNumber = binary->lineNumber,
			.operatorType = getCompoundAssignmentOperator[binary->operatorType],
			.right = binary->right,
			.left = CopyASTNode(binary->left),
		},
		sizeof(BinaryExpr), Node_Binary);
	binary->operatorType = Binary_Assignment;
}

static void VisitBinaryExpression(NodePtr* node, bool parentIsExprStmt)
{
	ASSERT(node->type == Node_Binary);
	BinaryExpr* binary = node->ptr;

	VisitExpression(&binary->left, false);
	VisitExpression(&binary->right, false);

	if (binary->left.type == Node_Binary &&
		((BinaryExpr*)binary->left.ptr)->operatorType == Binary_Assignment)
		return;

	NodePtr* nested = NULL;
	if (parentIsExprStmt)
	{
		if (binary->operatorType != Binary_Assignment)
			return;

		if (binary->right.type != Node_Binary)
			return;

		nested = &binary->right;
	}
	else
		nested = node;

	ASSERT(nested->type == Node_Binary);
	BinaryExpr* nestedBinary = nested->ptr;
	if (nestedBinary->operatorType != Binary_Assignment)
		return;

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
}

static void VisitExpression(NodePtr* node, bool parentIsExprStmt)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_Binary:
	{
		ConvertCompoundAssignment(node);
		VisitBinaryExpression(node, parentIsExprStmt);
		break;
	}
	case Node_Unary:
	{
		ConvertIncrement(node);
		if (node->type == Node_Unary)
		{
			UnaryExpr* unary = node->ptr;
			VisitExpression(&unary->expression, false);
		}
		else
			VisitExpression(node, false);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		for (size_t i = 0; i < funcCall->arguments.length; i++)
			VisitExpression(funcCall->arguments.array[i], false);
		break;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node->ptr;
		VisitExpression(&memberAccess->start, false);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr, false);
		VisitExpression(&subscript->indexExpr, false);
		break;
	}
	case Node_SizeOf:
	{
		SizeOfExpr* sizeOf = node->ptr;
		VisitExpression(&sizeOf->expr, false);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node->ptr;
		VisitStatement(&block->block);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(NodePtr* node)
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
		VisitExpression(&varDecl->initializer, false);
		break;
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr, true);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
		break;
	}
	case Node_BlockStatement:
	{
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		VisitExpression(&ifStmt->expr, false);
		break;
	}
	case Node_Return:
	{
		ReturnStmt* returnStmt = node->ptr;
		VisitExpression(&returnStmt->expr, false);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		VisitStatement(&section->block);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		VisitExpression(&whileStmt->expr, false);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	case Node_For:
	{
		ForStmt* forStmt = node->ptr;
		VisitStatement(&forStmt->initialization);
		VisitExpression(&forStmt->condition, false);
		VisitExpression(&forStmt->increment, false);
		VisitStatement(&forStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void ChainedAssignmentPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}
}
