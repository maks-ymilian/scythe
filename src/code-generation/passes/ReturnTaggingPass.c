#include "ReturnTaggingPass.h"

static Array stack;

static void Push(NodePtr function)
{
	ArrayAdd(&stack, &function);
}

static NodePtr Peek(void)
{
	ASSERT(stack.length != 0);
	NodePtr* function = stack.array[stack.length - 1];
	ASSERT(function);
	return *function;
}

static NodePtr Pop(void)
{
	NodePtr function = Peek();
	ArrayRemove(&stack, stack.length - 1);
	return function;
}

static void VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
	case Node_StructDeclaration:
	case Node_ExpressionStatement:
	case Node_LoopControl:
	case Node_Import:
	case Node_Input:
	case Node_Desc:
	case Node_Null:
		break;
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		Push(*node);
		VisitStatement(&funcDecl->block);
		Pop();
		break;
	}
	case Node_Return:
	{
		ReturnStmt* returnStmt = node->ptr;
		ASSERT(returnStmt->function.ptr == NULL);
		returnStmt->function = Peek();
		ASSERT(returnStmt->function.ptr);
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
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		Push(*node);
		VisitStatement(&section->block);
		Pop();
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

void ReturnTaggingPass(const AST* ast)
{
	stack = AllocateArray(sizeof(NodePtr));

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}

	FreeArray(&stack);
}
