#include "BlockRemoverPass.h"

#include "Common.h"

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
		case Node_If:
		{
			IfStmt* ifStmt = node->ptr;

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
			VisitBlock(whileStmt->stmt);
			break;
		}
		case Node_FunctionDeclaration:
		{
			FuncDeclStmt* funcDecl = node->ptr;
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
		case Node_LoopControl:
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
		case Node_Import:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}
	}
}

static void VisitGlobalStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
	case Node_Import:
	case Node_Input:
	case Node_Null:
		break;
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		VisitBlock(section->block);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		VisitBlock(funcDecl->block);
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
			VisitGlobalStatement(module->statements.array[i]);
	}
}
