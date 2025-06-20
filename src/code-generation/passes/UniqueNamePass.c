#include "UniqueNamePass.h"

#include "data-structures/Map.h"

static int uniqueNameCounter;
static Map names;
static Map pointers;

static void VisitStatement(const NodePtr node);

static bool AddPointer(void* ptr)
{
	char key[64];
	snprintf(key, sizeof(key), "%p", ptr);
	return MapAdd(&pointers, key, 0);
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
		if (memberAccess->varReference &&
			!memberAccess->varReference->modifiers.externalValue &&
			memberAccess->varReference->uniqueName == -1 &&
			AddPointer(memberAccess->varReference) &&
			!MapAdd(&names, memberAccess->varReference->name, NULL))
			memberAccess->varReference->uniqueName = ++uniqueNameCounter;
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
		VisitStatement(block->block);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

static void VisitStatement(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Null:
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node.ptr;
		VisitExpression(exprStmt->expr);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node.ptr;
		if (varDecl->uniqueName == -1 &&
			AddPointer(varDecl) &&
			!MapAdd(&names, varDecl->name, NULL))
			varDecl->uniqueName = ++uniqueNameCounter;
		VisitExpression(varDecl->initializer);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node.ptr;
		if (funcDecl->modifiers.externalValue)
			break;

		if (funcDecl->uniqueName == -1 &&
			AddPointer(funcDecl) &&
			!MapAdd(&names, funcDecl->name, NULL))
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
		VarDeclStmt* varDecl = input->varDecl.ptr;
		if (varDecl->uniqueName == -1 &&
			AddPointer(varDecl) &&
			!MapAdd(&names, varDecl->name, NULL))
			varDecl->uniqueName = ++uniqueNameCounter;
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
		VisitExpression(ifStmt->expr);
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
		VisitExpression(whileStmt->expr);
		VisitStatement(whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

void UniqueNamePass(const AST* ast)
{
	names = AllocateMap(0);
	pointers = AllocateMap(0);

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(*((NodePtr*)module->statements.array[i]));
	}

	FreeMap(&names);
	FreeMap(&pointers);
}
