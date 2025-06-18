#include "UniqueNamePass.h"

#include "data-structures/Map.h"

static int uniqueNameCounter = 0;
static Map names;

static void VisitStatement(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Null:
		break;
	case Node_ExpressionStatement:
		break;
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node.ptr;
		bool exists = !MapAdd(&names, varDecl->name, NULL);
		if (exists)
			varDecl->uniqueName = ++uniqueNameCounter;
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node.ptr;
		bool exists = !MapAdd(&names, funcDecl->name, NULL);
		if (exists)
			funcDecl->uniqueName = ++uniqueNameCounter;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			const NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VisitStatement(*node);
		}
		VisitStatement(funcDecl->block);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node.ptr;
		ASSERT(input->varDecl.type == Node_VariableDeclaration);
		VisitStatement(input->varDecl);
		break;
	}
	case Node_BlockStatement:
	{
		const BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
		{
			const NodePtr* node = block->statements.array[i];
			VisitStatement(*node);
		}
		break;
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node.ptr;
		VisitStatement(ifStmt->falseStmt);
		VisitStatement(ifStmt->trueStmt);
		break;
	}
	case Node_Section:
	{
		const SectionStmt* section = node.ptr;
		ASSERT(section->block.type == Node_BlockStatement);
		VisitStatement(section->block);
		break;
	}
	case Node_While:
	{
		const WhileStmt* whileStmt = node.ptr;
		VisitStatement(whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

void UniqueNamePass(const AST* ast)
{
	names = AllocateMap(0);

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(*((NodePtr*)module->statements.array[i]));
	}

	FreeMap(&names);
}
