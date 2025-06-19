#include "FunctionDepsPass.h"

static FuncDeclStmt* currentFunc;

static void VisitStatement(NodePtr* node);

static void AddDependency(FuncDeclStmt* funcDecl, NodePtr dependency)
{
	if (!funcDecl->dependencies.array)
		funcDecl->dependencies = AllocateArray(sizeof(NodePtr));

	ArrayAdd(&funcDecl->dependencies, &dependency);
}

static void VisitExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		VisitExpression(memberAccess->start);

		if (memberAccess->funcReference)
		{
			++memberAccess->funcReference->useCount;
			if (currentFunc)
				AddDependency(currentFunc, (NodePtr){.ptr = memberAccess->funcReference, .type = Node_FunctionDeclaration});
		}
		break;
	}
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
		BlockExpr* block = node.ptr;
		VisitStatement(&block->block);
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
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(exprStmt->expr);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(varDecl->initializer);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		currentFunc = funcDecl;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
		currentFunc = NULL;
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

void FunctionDepsPass(const AST* ast)
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
