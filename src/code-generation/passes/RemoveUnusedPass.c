#include "RemoveUnusedPass.h"

static void VisitStatement(NodePtr* node);

static void VisitExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_Null:
	case Node_MemberAccess:
		break;
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		VisitExpression(binary->left);
		VisitExpression(binary->right);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		VisitExpression(unary->expression);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		VisitExpression(funcCall->baseExpr);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(*(NodePtr*)funcCall->arguments.array[i]);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		VisitExpression(subscript->baseExpr);
		VisitExpression(subscript->indexExpr);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node.ptr;
		ASSERT(blockExpr->block.type == Node_BlockStatement);
		BlockStmt* block = blockExpr->block.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Null:
	case Node_Input:
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;

		if (exprStmt->expr.type == Node_Binary)
		{
			BinaryExpr* binary = exprStmt->expr.ptr;
			if (exprStmt->unused)
			{
				if (exprStmt->keepRight)
					exprStmt->expr = binary->right;
				else
					*node = NULL_NODE; 
			}
		}

		VisitExpression(exprStmt->expr);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;

		if (varDecl->unused)
		{
			if (varDecl->keepRight)
			{
				*node = AllocASTNode(
					&(ExpressionStmt){
						.expr = varDecl->initializer,
					}, sizeof(ExpressionStmt), Node_ExpressionStatement);
				VisitStatement(node);
			}
			else
				*node = NULL_NODE; 

			return;
		}

		VisitExpression(varDecl->initializer);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;

		if (funcDecl->unused)
		{
			*node = NULL_NODE;
			break;
		}

		if (!funcDecl->modifiers.externalValue)
		{
			ASSERT(funcDecl->block.type == Node_BlockStatement);
			BlockStmt* block = funcDecl->block.ptr;
			for (size_t i = 0; i < block->statements.length; ++i)
				VisitStatement(block->statements.array[i]);
		}
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitExpression(ifStmt->expr);
		VisitStatement(&ifStmt->falseStmt);
		VisitStatement(&ifStmt->trueStmt);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		ASSERT(section->block.type == Node_BlockStatement);
		VisitStatement(&section->block);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		VisitExpression(whileStmt->expr);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void RemoveUnusedPass(const AST* ast)
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
