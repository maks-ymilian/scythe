#include "UniqueNamePass.h"

#include <assert.h>

static int uniqueNameCounter = 0;

static void VisitExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Binary:
		const BinaryExpr* binary = node.ptr;
		VisitExpression(binary->left);
		VisitExpression(binary->right);
		break;
	case Node_Unary:
		const UnaryExpr* unary = node.ptr;
		VisitExpression(unary->expression);
		break;
	case Node_Literal:
		break;
	case Node_FunctionCall:
		const FuncCallExpr* funcCall = node.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(*(NodePtr*)funcCall->arguments.array[i]);
		break;
	case Node_Subscript:
		const SubscriptExpr* subscript = node.ptr;
		VisitExpression(subscript->addressExpr);
		VisitExpression(subscript->indexExpr);
		break;
	case Node_Null:
		break;
	default: INVALID_VALUE(node.type);
	}
}

static void VisitStatement(const NodePtr node)
{
	switch (node.type)
	{
	case Node_ExpressionStatement:
		const ExpressionStmt* exprStmt = node.ptr;
		VisitExpression(exprStmt->expr);
		break;
	case Node_VariableDeclaration:
		VarDeclStmt* varDecl = node.ptr;
		VisitExpression(varDecl->initializer);
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
		VisitExpression(ifStmt->expr);
		VisitStatement(ifStmt->falseStmt);
		VisitStatement(ifStmt->trueStmt);
		break;
	case Node_Section:
		const SectionStmt* section = node.ptr;
		assert(section->block.type == Node_BlockStatement);
		VisitStatement(section->block);
		break;
	case Node_While:
		const WhileStmt* whileStmt = node.ptr;
		VisitExpression(whileStmt->expr);
		VisitStatement(whileStmt->stmt);
		break;
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node.type);
	}
}

void UniqueNamePass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(*((NodePtr*)module->statements.array[i]));
	}
}
