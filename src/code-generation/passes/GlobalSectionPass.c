#include "GlobalSectionPass.h"

#include <stdio.h>

static NodePtr globalInitSection;

static void AddToInitSection(const NodePtr* node)
{
	ASSERT(globalInitSection.type == Node_Section);
	const SectionStmt* section = globalInitSection.ptr;
	ASSERT(section->block.type == Node_BlockStatement);
	BlockStmt* block = section->block.ptr;
	ArrayAdd(&block->statements, node);
}

static void VisitBlock(const NodePtr* node)
{
	if (node->type != Node_BlockStatement)
		return;

	BlockStmt* block = node->ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
	{
		const NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_FunctionDeclaration:
		{
			const FuncDeclStmt* funcDecl = node->ptr;
			VisitBlock(&funcDecl->block);

			AddToInitSection(node);
			ArrayRemove(&block->statements, i);
			i--;
			break;
		}
		case Node_BlockStatement:
		{
			VisitBlock(node);
			break;
		}
		case Node_If:
		{
			const IfStmt* ifStmt = node->ptr;
			VisitBlock(&ifStmt->trueStmt);
			VisitBlock(&ifStmt->falseStmt);
			break;
		}
		case Node_While:
		{
			const WhileStmt* whileStmt = node->ptr;
			VisitBlock(&whileStmt->stmt);
			break;
		}
		case Node_Return:
		case Node_LoopControl:
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}
	}
}

static void VisitModule(ModuleNode* module)
{
	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_FunctionDeclaration:
		{
			const FuncDeclStmt* funcDecl = stmt->ptr;
			VisitBlock(&funcDecl->block);

			AddToInitSection(stmt);
			ArrayRemove(&module->statements, i);
			i--;
			break;
		}
		case Node_StructDeclaration:
		case Node_VariableDeclaration:
		{
			AddToInitSection(stmt);
			ArrayRemove(&module->statements, i);
			i--;
			break;
		}
		case Node_Section:
		{
			const SectionStmt* section = stmt->ptr;
			VisitBlock(&section->block);
			break;
		}
		case Node_BlockStatement:
		{
			BlockStmt* block = stmt->ptr;
			for (size_t j = 0; j < block->statements.length; j++)
				ArrayInsert(&module->statements, block->statements.array[j], i + j + 1);
			ArrayClear(&block->statements);
			FreeASTNode(*stmt);

			ArrayRemove(&module->statements, i);
			i--;
			break;
		}
		case Node_Import:
		case Node_Null:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}
}

void GlobalSectionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		ASSERT(node->type == Node_Module);
		ModuleNode* module = node->ptr;

		globalInitSection = AllocASTNode(
			&(SectionStmt){
				.lineNumber = -1,
				.sectionType = Section_Init,
				.block = AllocASTNode(
					&(BlockStmt){
						.statements = AllocateArray(sizeof(NodePtr)),
					},
					sizeof(BlockStmt), Node_BlockStatement),
			},
			sizeof(SectionStmt), Node_Section);

		VisitModule(module);

		if (((BlockStmt*)((SectionStmt*)globalInitSection.ptr)->block.ptr)->statements.length != 0)
			ArrayInsert(&module->statements, &globalInitSection, 0);
		else
			FreeASTNode(globalInitSection);
	}
}
