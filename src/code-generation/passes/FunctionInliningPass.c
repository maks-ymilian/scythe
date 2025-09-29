#include "FunctionInliningPass.h"

#include "data-structures/Map.h"

#include <string.h>

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

static FuncDeclStmt* GetFuncDecl(FuncCallExpr* funcCall)
{
	ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
	ASSERT(memberAccess->funcReference);
	return memberAccess->funcReference;
}

static void VisitExpression(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		VisitExpression(&funcCall->baseExpr);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(funcCall->arguments.array[i]);

		if (!GetFuncDecl(funcCall)->isBlockExpression || GetFuncDecl(funcCall)->useCount > 1)
			break;
		ASSERT(GetFuncDecl(funcCall)->parameters.length == 0);

		*node = AllocASTNode(
			&(BlockExpr){
				.block = GetFuncDecl(funcCall)->block,
			},
			sizeof(BlockExpr), Node_BlockExpression);

		NodePtr* funcDeclNode = GetReference(GetFuncDecl(funcCall));
		GetFuncDecl(funcCall)->block = NULL_NODE;
		FreeASTNode(*funcDeclNode);
		*funcDeclNode = NULL_NODE;
		break;
	}
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
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr);
		VisitExpression(&subscript->indexExpr);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Desc:
	case Node_Null:
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		AddReference(funcDecl, node);
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
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
		VisitExpression(&ifStmt->expr);
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
		VisitExpression(&whileStmt->expr);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void FunctionInliningPass(const AST* ast)
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
