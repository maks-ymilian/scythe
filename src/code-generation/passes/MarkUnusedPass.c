#include "MarkUnusedPass.h"

static void VisitStatement(NodePtr* node, bool endOfFunction);
static void ProcessAssignment(NodePtr assignment);

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

	funcDecl->unused = true;
}

static void AddDepsInNode(NodePtr node, Array* deps)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Null:
	case Node_Input:
		break;
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		for (size_t i = 0; i < memberAccess->deps.length; ++i)
		{
			NodePtr* dep = memberAccess->deps.array[i];
			ArrayAdd(deps, dep);
		}
		break;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		if (binary->left.type == Node_MemberAccess)
			ASSERT(binary->operatorType != Binary_Assignment);
		AddDepsInNode(binary->left, deps);
		AddDepsInNode(binary->right, deps);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		AddDepsInNode(unary->expression, deps);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			AddDepsInNode(*(NodePtr*)funcCall->arguments.array[i], deps);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		AddDepsInNode(subscript->baseExpr, deps);
		AddDepsInNode(subscript->indexExpr, deps);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node.ptr;
		AddDepsInNode(block->block, deps);
		break;
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node.ptr;
		if (exprStmt->expr.type == Node_Binary &&
			((BinaryExpr*)exprStmt->expr.ptr)->operatorType == Binary_Assignment &&
			((BinaryExpr*)exprStmt->expr.ptr)->left.type == Node_MemberAccess)
		{
			BinaryExpr* binary = exprStmt->expr.ptr;
			AddDepsInNode(binary->right, deps);
		}
		else
			AddDepsInNode(exprStmt->expr, deps);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node.ptr;
		AddDepsInNode(varDecl->initializer, deps);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			AddDepsInNode(*(NodePtr*)block->statements.array[i], deps);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node.ptr;
		AddDepsInNode(ifStmt->expr, deps);
		AddDepsInNode(ifStmt->falseStmt, deps);
		AddDepsInNode(ifStmt->trueStmt, deps);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node.ptr;
		AddDepsInNode(whileStmt->expr, deps);
		AddDepsInNode(whileStmt->stmt, deps);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

static Array GetDepsInNode(NodePtr node)
{
	Array deps = AllocateArray(sizeof(NodePtr));
	AddDepsInNode(node, &deps);
	return deps;
}

static bool NodeHasSideEffects(NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_MemberAccess:
	case Node_Unary:
	case Node_Null:
		return false;
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		return binary->operatorType == Binary_Assignment ||
			   NodeHasSideEffects(binary->left) ||
			   NodeHasSideEffects(binary->right);
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
		{
			if (NodeHasSideEffects(*(NodePtr*)funcCall->arguments.array[i]))
				return true;
		}
		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		ASSERT(memberAccess->funcReference);
		if (memberAccess->funcReference->modifiers.externalValue)
			return true;
		return NodeHasSideEffects(memberAccess->funcReference->block);
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		return NodeHasSideEffects(subscript->baseExpr) ||
			   NodeHasSideEffects(subscript->indexExpr);
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node.ptr;
		return NodeHasSideEffects(block->block);
	}
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node.ptr;
		if (exprStmt->expr.type == Node_Binary &&
			((BinaryExpr*)exprStmt->expr.ptr)->operatorType == Binary_Assignment &&
			((BinaryExpr*)exprStmt->expr.ptr)->left.type == Node_MemberAccess)
			return true;
		else
			return NodeHasSideEffects(exprStmt->expr);
	}
	case Node_VariableDeclaration:
		return true;
	case Node_BlockStatement:
	{
		BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
		{
			if (NodeHasSideEffects(*(NodePtr*)block->statements.array[i]))
				return true;
		}
		return false;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node.ptr;
		return NodeHasSideEffects(ifStmt->expr) ||
			   NodeHasSideEffects(ifStmt->trueStmt) ||
			   NodeHasSideEffects(ifStmt->falseStmt);
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node.ptr;
		return NodeHasSideEffects(whileStmt->expr) ||
			   NodeHasSideEffects(whileStmt->stmt);
	}
	default: INVALID_VALUE(node.type);
	}
}

static int* GetUseCount(NodePtr assignment)
{
	if (assignment.type == Node_VariableDeclaration)
		return &((VarDeclStmt*)assignment.ptr)->useCount;
	else if (assignment.type == Node_ExpressionStatement)
		return &((ExpressionStmt*)assignment.ptr)->useCount;
	else
		UNREACHABLE();
}

static bool GetDoNotOptimize(NodePtr assignment)
{
	if (assignment.type == Node_VariableDeclaration)
		return ((VarDeclStmt*)assignment.ptr)->doNotOptimize;
	else if (assignment.type == Node_ExpressionStatement)
		return ((ExpressionStmt*)assignment.ptr)->doNotOptimize;
	else
		UNREACHABLE();
}

static NodePtr GetAssignmentRight(NodePtr assignment)
{
	if (assignment.type == Node_VariableDeclaration)
		return ((VarDeclStmt*)assignment.ptr)->initializer;
	else if (assignment.type == Node_ExpressionStatement)
	{

		ExpressionStmt* exprStmt = assignment.ptr;
		ASSERT(exprStmt->expr.type == Node_Binary);
		BinaryExpr* binary = exprStmt->expr.ptr;
		ASSERT(binary->operatorType == Binary_Assignment);
		return binary->right;
	}
	else
		UNREACHABLE();
}

static void SetAssignmentUnused(NodePtr assignment)
{
	if (assignment.type == Node_VariableDeclaration)
		((VarDeclStmt*)assignment.ptr)->unused = true;
	else if (assignment.type == Node_ExpressionStatement)
		((ExpressionStmt*)assignment.ptr)->unused = true;
	else
		UNREACHABLE();
}

static void SetAssignmentKeepRight(NodePtr assignment)
{
	if (assignment.type == Node_VariableDeclaration)
		((VarDeclStmt*)assignment.ptr)->keepRight = true;
	else if (assignment.type == Node_ExpressionStatement)
		((ExpressionStmt*)assignment.ptr)->keepRight = true;
	else
		UNREACHABLE();
}

static void UpdateDeps(NodePtr node)
{
	Array deps = GetDepsInNode(node);
	for (size_t i = 0; i < deps.length; ++i)
	{
		NodePtr* node = deps.array[i];
		int* useCount = GetUseCount(*node);
		--(*useCount);
		ProcessAssignment(*node);
	}
	FreeArray(&deps);
}

static void ProcessAssignment(NodePtr assignment)
{
	ASSERT(*GetUseCount(assignment) >= 0);
	if (GetDoNotOptimize(assignment) || *GetUseCount(assignment) != 0)
		return;

	SetAssignmentUnused(assignment);
	if (NodeHasSideEffects(GetAssignmentRight(assignment)))
		SetAssignmentKeepRight(assignment);
	else
		UpdateDeps(GetAssignmentRight(assignment));
}

static void VisitExpression(const NodePtr node, bool parentIsExprStmt)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_Null:
	case Node_MemberAccess:
		break;
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		VisitExpression(binary->left, false);
		VisitExpression(binary->right, false);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		VisitExpression(unary->expression, false);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		VisitExpression(funcCall->baseExpr, false);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(*(NodePtr*)funcCall->arguments.array[i], false);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		VisitExpression(subscript->baseExpr, false);
		VisitExpression(subscript->indexExpr, false);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node.ptr;
		ASSERT(blockExpr->block.type == Node_BlockStatement);
		BlockStmt* block = blockExpr->block.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i], !parentIsExprStmt && i == block->statements.length - 1);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

static void VisitStatement(NodePtr* node, bool endOfFunction)
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

		if (exprStmt->expr.type == Node_Binary &&
			((BinaryExpr*)exprStmt->expr.ptr)->operatorType == Binary_Assignment &&
			((BinaryExpr*)exprStmt->expr.ptr)->left.type == Node_MemberAccess)
		{
			BinaryExpr* binary = exprStmt->expr.ptr;
			MemberAccessExpr* memberAccess = binary->left.ptr;
			ASSERT(memberAccess->varReference);
			if (memberAccess->varReference->modifiers.externalValue)
				exprStmt->doNotOptimize = true;

			ProcessAssignment(*node);
			VisitExpression(exprStmt->expr, true);
			break;
		}
		else if (!NodeHasSideEffects(*node) && !endOfFunction)
		{
			UpdateDeps(*node);
			*node = NULL_NODE;
			break;
		}
		VisitExpression(exprStmt->expr, true);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(varDecl->initializer, false);
		ProcessAssignment(*node);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node->ptr;
		ASSERT(input->varDecl.type == Node_VariableDeclaration);
		VarDeclStmt* varDecl = input->varDecl.ptr;
		varDecl->doNotOptimize = true;
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			varDecl->doNotOptimize = true;
		}

		if (!funcDecl->modifiers.externalValue)
		{
			ASSERT(funcDecl->block.type == Node_BlockStatement);
			BlockStmt* block = funcDecl->block.ptr;
			for (size_t i = 0; i < block->statements.length; ++i)
				VisitStatement(block->statements.array[i], i == block->statements.length - 1);
		}

		ProcessFuncDecl(funcDecl);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i], false);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitExpression(ifStmt->expr, false);
		VisitStatement(&ifStmt->falseStmt, false);
		VisitStatement(&ifStmt->trueStmt, false);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		ASSERT(section->block.type == Node_BlockStatement);
		VisitStatement(&section->block, false);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		VisitExpression(whileStmt->expr, false);
		VisitStatement(&whileStmt->stmt, false);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void MarkUnusedPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i], false);
	}
}
