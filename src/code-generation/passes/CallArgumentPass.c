#include "CallArgumentPass.h"

static const char* currentFilePath = NULL;

static size_t Max(size_t a, size_t b)
{
	return a > b ? a : b;
}

static Result VisitFunctionCall(FuncCallExpr* funcCall)
{
	assert(funcCall->baseExpr.type == Node_MemberAccess);
	const MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
	const FuncDeclStmt* funcDecl = memberAccess->funcReference;
	assert(funcDecl != NULL);

	bool foundInitializer = false;
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		NodePtr* node = funcDecl->parameters.array[i];
		assert(node->type == Node_VariableDeclaration);
		VarDeclStmt* param = node->ptr;

		if (param->initializer.ptr != NULL)
			foundInitializer = true;

		if (param->initializer.ptr == NULL && foundInitializer)
		{
			return ERROR_RESULT("Default parameters must be at the end of the parameter list",
				funcCall->lineNumber, currentFilePath);
		}
	}

	for (size_t i = 0; i < Max(funcCall->arguments.length, funcDecl->parameters.length); ++i)
	{
		NodePtr* arg = NULL;
		if (i < funcCall->arguments.length)
			arg = funcCall->arguments.array[i];

		VarDeclStmt* param = NULL;
		if (i < funcDecl->parameters.length)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			assert(node->type == Node_VariableDeclaration);
			param = node->ptr;
		}

		if (arg != NULL && param == NULL)
		{
			return ERROR_RESULT("Function called with incorrect number of arguments",
				funcCall->lineNumber, currentFilePath);
		}
		else if (arg == NULL && param != NULL)
		{
			if (param->initializer.ptr == NULL)
				return ERROR_RESULT("Function called with incorrect number of arguments",
					funcCall->lineNumber, currentFilePath);

			NodePtr copy = CopyASTNode(param->initializer);
			ArrayAdd(&funcCall->arguments, &copy);
		}
	}

	return SUCCESS_RESULT;
}

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

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
