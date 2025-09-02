#include "BlockRemoverPass.h"

#include "Common.h"

static void VisitStatement(const NodePtr* node);
static void VisitExpression(NodePtr* node);

static void VisitBlock(NodePtr node)
{
	if (node.ptr == NULL)
		return;

	ASSERT(node.type == Node_BlockStatement);
	BlockStmt* block = node.ptr;

	for (size_t i = 0; i < block->statements.length; ++i)
	{
		NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_BlockStatement:
		{
			BlockStmt* innerBlock = node->ptr;
			for (size_t j = 0; j < innerBlock->statements.length; ++j)
				ArrayInsert(&block->statements, innerBlock->statements.array[j], i + j + 1);
			ArrayClear(&innerBlock->statements);
			FreeASTNode(*node);
			*node = NULL_NODE;
			break;
		}
		case Node_ExpressionStatement:
		{
			ExpressionStmt* exprStmt = node->ptr;
			if (exprStmt->expr.type != Node_BlockExpression)
			{
				VisitExpression(&exprStmt->expr);
				break;
			}

			BlockExpr* blockExpr = exprStmt->expr.ptr;
			ASSERT(blockExpr->block.type == Node_BlockStatement);
			BlockStmt* innerBlock = blockExpr->block.ptr;
			for (size_t j = 0; j < innerBlock->statements.length; ++j)
				ArrayInsert(&block->statements, innerBlock->statements.array[j], i + j + 1);
			ArrayClear(&innerBlock->statements);
			FreeASTNode(*node);
			*node = NULL_NODE;
			break;
		}
		case Node_If:
		{
			IfStmt* ifStmt = node->ptr;

			VisitExpression(&ifStmt->expr);

			ASSERT(ifStmt->trueStmt.type == Node_BlockStatement);
			BlockStmt* trueBlock = ifStmt->trueStmt.ptr;
			if (trueBlock->statements.length == 0)
			{
				NodePtr nothing = AllocASTNode(
					&(ExpressionStmt){
						.lineNumber = ifStmt->lineNumber,
						.expr = AllocUInt64Integer(0, ifStmt->lineNumber),
					},
					sizeof(ExpressionStmt), Node_ExpressionStatement);
				ArrayAdd(&trueBlock->statements, &nothing);
			}

			if (ifStmt->falseStmt.ptr != NULL)
			{
				ASSERT(ifStmt->falseStmt.type == Node_BlockStatement);
				BlockStmt* falseBlock = ifStmt->falseStmt.ptr;
				if (falseBlock->statements.length == 0)
				{
					FreeASTNode(ifStmt->falseStmt);
					ifStmt->falseStmt = NULL_NODE;
				}
			}

			VisitBlock(ifStmt->trueStmt);
			VisitBlock(ifStmt->falseStmt);
			break;
		}
		case Node_While:
		{
			WhileStmt* whileStmt = node->ptr;
			VisitExpression(&whileStmt->expr);
			VisitBlock(whileStmt->stmt);
			break;
		}
		case Node_FunctionDeclaration:
		{
			FuncDeclStmt* funcDecl = node->ptr;
			for (size_t i = 0; i < funcDecl->parameters.length; ++i)
				VisitStatement(funcDecl->parameters.array[i]);
			VisitBlock(funcDecl->block);
			break;
		}
		case Node_Section:
		{
			SectionStmt* section = node->ptr;
			VisitBlock(section->block);
			break;
		}
		case Node_Return:
		{
			ReturnStmt* returnStmt = node->ptr;
			VisitExpression(&returnStmt->expr);
			break;
		}
		case Node_VariableDeclaration:
		{
			VisitStatement(node);
			break;
		}
		case Node_Import:
		case Node_LoopControl:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}
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
		MemberAccessExpr* memberAccess = node->ptr;
		VisitExpression(&memberAccess->start);
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
		VisitExpression(&funcCall->baseExpr);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
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
	case Node_BlockExpression:
	{
		BlockExpr* block = node->ptr;
		VisitBlock(block->block);

		ASSERT(block->block.type == Node_BlockStatement);
		BlockStmt* blockStmt = block->block.ptr;

		int statementCount = 0;
		size_t statementIndex = 0;
		for (size_t i = 0; i < blockStmt->statements.length; ++i)
		{
			NodePtr* node = blockStmt->statements.array[i];
			if (node->ptr)
			{
				++statementCount;
				statementIndex = i;
			}
		}

		if (statementCount == 1)
		{
			NodePtr* statementNode = blockStmt->statements.array[statementIndex];
			if (statementNode->type == Node_ExpressionStatement)
			{
				ExpressionStmt* exprStmt = statementNode->ptr;
				ArrayClear(&blockStmt->statements);
				FreeASTNode(*node);
				*node = exprStmt->expr;
			}
		}
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_Input:
	case Node_Null:
		break;
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		VisitBlock(section->block);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void BlockRemoverPass(const AST* ast)
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
