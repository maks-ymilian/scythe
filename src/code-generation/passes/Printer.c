#include "Printer.h"

#include <assert.h>

static void VisitStatement(const NodePtr* node);
static void VisitExpression(const NodePtr* node)
{
	printf("(");
	switch (node->type)
	{
	case Node_Binary:
	{
		printf("Binary");
		const BinaryExpr* binary = node->ptr;
		VisitExpression(&binary->left);
		VisitExpression(&binary->right);
		break;
	}
	case Node_Unary:
	{
		printf("Unary");
		const UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression);
		break;
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* memberAccess = node->ptr;
		printf("MemberAccess %d ", memberAccess->lineNumber);
		VisitExpression(&memberAccess->start);
		for (size_t i = 0; i < memberAccess->identifiers.length; ++i)
			printf(".%s", *(char**)memberAccess->identifiers.array[i]);
		break;
	}
	case Node_Subscript:
	{
		printf("Subscript");
		const SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr);
		VisitExpression(&subscript->indexExpr);
		break;
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = node->ptr;
		printf("FunctionCall %d ", funcCall->lineNumber);
		VisitExpression(&funcCall->baseExpr);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(funcCall->arguments.array[i]);
		break;
	}
	case Node_BlockExpression:
	{
		printf("BlockExpression");
		// const BlockExpr* blockExpr = node->ptr;
		// VisitStatement(&blockExpr->block);
		break;
	}
	case Node_Literal:
	{
		printf("Literal");
		break;
	}
	case Node_Null:
	{
		printf("Null");
		break;
	}
	default: INVALID_VALUE(node->type);
	}
	printf(")");
}

static void VisitStatement(const NodePtr* node)
{
	printf("Statement:\n");
	switch (node->type)
	{
	case Node_BlockStatement:
	{
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		VisitStatement(&section->block);
		break;
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		VisitExpression(&ifStmt->expr);
		break;
	}
	case Node_FunctionDeclaration:
	{
		const FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
		break;
	}
	case Node_Return:
	{
		const ReturnStmt* returnStmt = node->ptr;
		VisitExpression(&returnStmt->expr);
		break;
	}
	case Node_VariableDeclaration:
	{
		const VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer);
		break;
	}
	case Node_StructDeclaration:
	{
		const StructDeclStmt* structDecl = node->ptr;
		for (size_t i = 0; i < structDecl->members.length; ++i)
			VisitStatement(structDecl->members.array[i]);
		break;
	}
	case Node_ExpressionStatement:
	{
		const ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr);
		break;
	}
	case Node_While:
	{
		const WhileStmt* whileStmt = node->ptr;
		VisitExpression(&whileStmt->expr);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	case Node_For:
	{
		const ForStmt* forStmt = node->ptr;
		VisitStatement(&forStmt->initialization);
		VisitExpression(&forStmt->condition);
		VisitExpression(&forStmt->increment);
		VisitStatement(&forStmt->stmt);
		break;
	}
	case Node_LoopControl:
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}
	printf("\n\n");
}

void Printer(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}
}
