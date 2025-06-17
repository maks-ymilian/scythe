#include "RemoveUnusedPass.h"

#include "data-structures/Map.h"

static Map pointerToReference;

static void AddReference(void* pointer, NodePtr* reference)
{
	char key[64];
	snprintf(key, sizeof(key), "%p", pointer);
	MapAdd(&pointerToReference, key, &reference);
}

static NodePtr* GetReference(void* pointer)
{
	char key[64];
	snprintf(key, sizeof(key), "%p", pointer);
	NodePtr** reference = MapGet(&pointerToReference, key);
	ASSERT(reference);
	ASSERT(*reference);
	ASSERT((*reference)->type == Node_FunctionDeclaration);
	return *reference;
}

static void ProcessFuncDecl(FuncDeclStmt* funcDecl)
{
	if (funcDecl->useCount != 0)
		return;

	for (size_t i = 0; i < funcDecl->dependencies.length; ++i)
	{
		NodePtr* node = funcDecl->dependencies.array[i];
		ASSERT(node->type == Node_FunctionDeclaration);
		FuncDeclStmt* f = node->ptr;

		--f->useCount;
		ProcessFuncDecl(f);
	}

	NodePtr* node = GetReference(funcDecl);
	*node = NULL_NODE;
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Null:
	case Node_ExpressionStatement:
	case Node_VariableDeclaration:
		break;
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		AddReference(funcDecl, node);
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
		ProcessFuncDecl(funcDecl);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node->ptr;
		ASSERT(input->varDecl.type == Node_VariableDeclaration);
		VisitStatement(&input->varDecl);
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
		VisitStatement(&whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void RemoveUnusedPass(const AST* ast)
{
	pointerToReference = AllocateMap(sizeof(NodePtr*));

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}

	FreeMap(&pointerToReference);
}
