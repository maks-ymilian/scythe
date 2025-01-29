#include "GlobalSectionPass.h"

#include <assert.h>
#include <stdio.h>

static NodePtr globalInitSection;

static void AddToInitSection(const NodePtr* node)
{
	assert(globalInitSection.type == Node_Section);
	const SectionStmt* section = globalInitSection.ptr;
	assert(section->block.type == Node_Block);
	BlockStmt* block = section->block.ptr;
	ArrayAdd(&block->statements, node);
}

static void VisitBlock(const NodePtr* node)
{
	if (node->type != Node_Block)
		return;

	BlockStmt* block = node->ptr;

	if (block->statements.length == 0)
	{
		const NodePtr node = AllocASTNode(
			&(ExpressionStmt){
				.expr = AllocASTNode(
					&(LiteralExpr){
						.lineNumber = block->lineNumber,
						.type = Literal_Int,
						.intValue = 0,
					},
					sizeof(LiteralExpr), Node_Literal)},
			sizeof(ExpressionStmt), Node_ExpressionStatement);

		ArrayAdd(&block->statements, &node);
		return;
	}

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
		case Node_Block:
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
		case Node_For:
		{
			const ForStmt* forStmt = node->ptr;
			VisitBlock(&forStmt->stmt);
			break;
		}
		case Node_Return:
		case Node_LoopControl:
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
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
			const FuncDeclStmt* funcDecl = stmt->ptr;
			VisitBlock(&funcDecl->block);
			[[fallthrough]];
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
		case Node_Import: break;
		default: INVALID_VALUE(stmt->type);
		}
	}
}

void GlobalSectionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		ModuleNode* module = node->ptr;

		globalInitSection = AllocASTNode(
			&(SectionStmt){
				.lineNumber = -1,
				.sectionType = Section_Init,
				.block = AllocASTNode(
					&(BlockStmt){
						.statements = AllocateArray(sizeof(NodePtr)),
					},
					sizeof(BlockStmt), Node_Block),
			},
			sizeof(SectionStmt), Node_Section);

		VisitModule(module);

		if (((BlockStmt*)((SectionStmt*)globalInitSection.ptr)->block.ptr)->statements.length != 0)
			ArrayInsert(&module->statements, &globalInitSection, 0);
		else
			FreeASTNode(globalInitSection);
	}
}
