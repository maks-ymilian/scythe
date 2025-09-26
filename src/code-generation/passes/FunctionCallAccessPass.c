#include "FunctionCallAccessPass.h"

#include "Common.h"
#include "StringUtils.h"

static void VisitStatement(NodePtr* node);
static void VisitExpression(NodePtr* node);


static void VisitMemberAccess(NodePtr* node)
{
	ASSERT(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;
	VisitExpression(&memberAccess->start);

	if (memberAccess->start.type == Node_FunctionCall ||
		memberAccess->start.type == Node_BlockExpression)
	{
		BlockStmt* blockStmt = AllocASTNode(
			&(BlockStmt){
				.lineNumber = memberAccess->lineNumber,
				.statements = AllocateArray(sizeof(NodePtr)),
			},
			sizeof(BlockStmt), Node_BlockStatement)
								   .ptr;
		NodePtr blockExpr = AllocASTNode(
			&(BlockExpr){
				.type = AllocTypeFromExpr(*node, memberAccess->lineNumber),
				.block = (NodePtr){.ptr = blockStmt, .type = Node_BlockStatement},
			},
			sizeof(BlockExpr), Node_BlockExpression);

		NodePtr varDecl = AllocASTNode(
			&(VarDeclStmt){
				.lineNumber = memberAccess->lineNumber,
				.initializer = memberAccess->start,
				.type = AllocTypeFromExpr(memberAccess->start, memberAccess->lineNumber),
				.name = AllocateString("temp"),
				.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
				.uniqueName = -1,
			},
			sizeof(VarDeclStmt), Node_VariableDeclaration);
		ArrayAdd(&blockStmt->statements, &varDecl);

		NodePtr returnStmt = AllocASTNode(
			&(ReturnStmt){
				.lineNumber = memberAccess->lineNumber,
				.expr = *node,
			},
			sizeof(ReturnStmt), Node_Return);
		ArrayAdd(&blockStmt->statements, &returnStmt);

		memberAccess->start = NULL_NODE;
		memberAccess->varParentReference = varDecl.ptr;

		*node = blockExpr;
	}
}

static void VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_MemberAccess:
	{
		VisitMemberAccess(node);
		break;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node->ptr;
		VisitExpression(&binary->left);
		VisitExpression(&binary->right);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		for (size_t i = 0; i < funcCall->arguments.length; i++)
			VisitExpression(funcCall->arguments.array[i]);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr);
		VisitExpression(&subscript->indexExpr);
		break;
	}
	case Node_SizeOf:
	{
		SizeOfExpr* sizeOf = node->ptr;
		VisitExpression(&sizeOf->expr);
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
	case Node_Desc:
	case Node_Null:
		break;
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer);
		break;
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr);
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
		VisitExpression(&ifStmt->expr);
		break;
	}
	case Node_Return:
	{
		ReturnStmt* returnStmt = node->ptr;
		VisitExpression(&returnStmt->expr);
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
		VisitExpression(&whileStmt->expr);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	case Node_For:
	{
		ForStmt* forStmt = node->ptr;
		VisitStatement(&forStmt->initialization);
		VisitExpression(&forStmt->condition);
		VisitExpression(&forStmt->increment);
		VisitStatement(&forStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void FunctionCallAccessPass(const AST* ast)
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
