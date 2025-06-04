#include "CallArgumentPass.h"

static const char* currentFilePath = NULL;


static Result VisitExpression(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_FunctionCall:
		PROPAGATE_ERROR(VisitFunctionCall(node->ptr));
		break;
	case Node_Binary:
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&binary->left));
		PROPAGATE_ERROR(VisitExpression(&binary->right));
		break;
	case Node_Unary:
		const UnaryExpr* unary = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&unary->expression));
		break;
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&memberAccess->start));
		break;
	case Node_Subscript:
		const SubscriptExpr* subscript = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&subscript->baseExpr));
		PROPAGATE_ERROR(VisitExpression(&subscript->indexExpr));
		break;
	case Node_SizeOf:
		SizeOfExpr* sizeOf = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&sizeOf->expr));
		break;
	case Node_Literal:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_BlockStatement:
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		break;
	case Node_Section:
		const SectionStmt* section = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&section->block));
		break;
	case Node_If:
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr));
		break;
	case Node_FunctionDeclaration:
		const FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			PROPAGATE_ERROR(VisitStatement(funcDecl->parameters.array[i]));
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
		break;
	case Node_Return:
		const ReturnStmt* returnStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&returnStmt->expr));
		break;
	case Node_VariableDeclaration:
		const VarDeclStmt* varDecl = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer));
		break;
	case Node_StructDeclaration:
		const StructDeclStmt* structDecl = node->ptr;
		for (size_t i = 0; i < structDecl->members.length; ++i)
			PROPAGATE_ERROR(VisitStatement(structDecl->members.array[i]));
		break;
	case Node_ExpressionStatement:
		const ExpressionStmt* exprStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&exprStmt->expr));
		break;
	case Node_While:
		const WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&whileStmt->expr));
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		break;
	case Node_LoopControl:
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}
	return SUCCESS_RESULT;
}

Result CallArgumentPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
