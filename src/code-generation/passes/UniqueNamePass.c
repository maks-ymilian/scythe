#include "UniqueNamePass.h"

#include <assert.h>

static int uniqueNameCounter = 0;

static void VisitStatement(const NodePtr node)
{
	switch (node.type)
	{
	case Node_ExpressionStatement:
		break;
	case Node_VariableDeclaration:
		VarDeclStmt* varDecl = node.ptr;
		varDecl->uniqueName = ++uniqueNameCounter;
		break;
	case Node_FunctionDeclaration:
		FuncDeclStmt* funcDecl = node.ptr;
		funcDecl->uniqueName = ++uniqueNameCounter;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			const NodePtr* node = funcDecl->parameters.array[i];
			assert(node->type == Node_VariableDeclaration);
			VisitStatement(*node);
		}
		assert(funcDecl->block.type == Node_BlockStatement);
		VisitStatement(funcDecl->block);
		break;
	case Node_StructDeclaration:
		break;

	case Node_BlockStatement:
		const BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
		{
			const NodePtr* node = block->statements.array[i];
			VisitStatement(*node);
		}
		break;

	case Node_If:
		const IfStmt* ifStmt = node.ptr;
		VisitStatement(ifStmt->falseStmt);
		VisitStatement(ifStmt->trueStmt);
		break;

	case Node_Null:
		break;

	default: INVALID_VALUE(node.type);
	}
}

static void VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_BlockStatement);
	VisitStatement(section->block);
}

static void VisitModule(const ModuleNode* module)
{
	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			VisitSection(stmt->ptr);
			break;
		case Node_Import:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}
}

void UniqueNamePass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		VisitModule(node->ptr);
	}
}
