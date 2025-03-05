#include "ReturnTaggingPass.h"

#include <assert.h>

static FuncDeclStmt* currentFunction;

static void VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_FunctionDeclaration:
		FuncDeclStmt* funcDecl = node->ptr;
		currentFunction = funcDecl;
		VisitStatement(&funcDecl->block);
		break;
	case Node_Return:
		ReturnStmt* returnStmt = node->ptr;
		assert(returnStmt->function == NULL);
		assert(currentFunction != NULL);
		returnStmt->function = currentFunction;
		break;
	case Node_BlockStatement:
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	case Node_If:
		IfStmt* ifStmt = node->ptr;
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		break;
	case Node_Section:
		SectionStmt* section = node->ptr;
		VisitStatement(&section->block);
		break;
	case Node_While:
		WhileStmt* whileStmt = node->ptr;
		VisitStatement(&whileStmt->stmt);
		break;
	case Node_VariableDeclaration:
	case Node_StructDeclaration:
	case Node_ExpressionStatement:
	case Node_LoopControl:
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}
}

void ReturnTaggingPass(const AST* ast)
{
	currentFunction = NULL;

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}
}
