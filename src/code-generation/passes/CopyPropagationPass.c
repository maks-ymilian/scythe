#include "CopyPropagationPass.h"

#include "data-structures/Map.h"

#include <string.h>

typedef Map CopyAssignments;

static FuncDeclStmt* currentFunction;

static void VisitStatement(NodePtr* node, CopyAssignments* map, bool modifyAST, bool modifyMap);

static CopyAssignments AllocCopyAssignments(void)
{
	return AllocateMap(sizeof(NodePtr));
}

static void FreeCopyAssignments(const CopyAssignments* map)
{
	FreeMap(map);
}

static CopyAssignments CopyCopyAssignments(const CopyAssignments* map)
{
	CopyAssignments new = AllocCopyAssignments();
	for (MAP_ITERATE(i, map))
	{
		bool success = MapAdd(&new, i->key, i->value);
		ASSERT(success);
	}
	return new;
}

static void MergeCopyAssignments(CopyAssignments* map1, const CopyAssignments* map2)
{
	Array deleteKeys = AllocateArray(sizeof(char*));
	for (MAP_ITERATE(i, map1))
	{
		bool exists = false;
		for (MAP_ITERATE(j, map2))
		{
			if (((NodePtr*)i->value)->ptr == ((NodePtr*)j->value)->ptr && strcmp(i->key, j->key) == 0)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
			ArrayAdd(&deleteKeys, &i->key);
	}
	FreeCopyAssignments(map2);

	for (size_t i = 0; i < deleteKeys.length; ++i)
		MapRemove(map1, *(char**)deleteKeys.array[i]);
	FreeArray(&deleteKeys);
}

static NodePtr GetCopyAssignmentValue(const CopyAssignments* map, const VarDeclStmt* key)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (const void*)key);
	NodePtr* value = MapGet(map, keyStr);
	if (!value)
		return NULL_NODE;
	return *value;
}

static bool AddCopyAssignment(CopyAssignments* map, const VarDeclStmt* key, NodePtr value)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (const void*)key);
	return MapAdd(map, keyStr, &value);
}

static void DeleteAllPairsWithVar(CopyAssignments* map, const VarDeclStmt* var)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (const void*)var);

	Array deleteKeys = AllocateArray(sizeof(char*));
	for (MAP_ITERATE(i, map))
	{
		if (((NodePtr*)i->value)->ptr == var || strcmp(i->key, keyStr) == 0)
			ArrayAdd(&deleteKeys, &i->key);
	}

	for (size_t i = 0; i < deleteKeys.length; ++i)
		MapRemove(map, *(char**)deleteKeys.array[i]);
	FreeArray(&deleteKeys);
}

static bool ExprIsConstant(NodePtr expr)
{
	switch (expr.type)
	{
	case Node_Literal:
	case Node_SizeOf:
		return true;
	case Node_MemberAccess:
	case Node_Subscript:
	case Node_FunctionCall:
	case Node_BlockExpression:
		return false;
	case Node_Binary:
	{
		BinaryExpr* binary = expr.ptr;
		return ExprIsConstant(binary->left) && ExprIsConstant(binary->right);
	}
	case Node_Unary:
	{
		UnaryExpr* unary = expr.ptr;
		return ExprIsConstant(unary->expression);
	}
	default: INVALID_VALUE(expr.type);
	}
}

static void ProcessAssignment(VarDeclStmt* assignmentLeft, NodePtr assignmentRight, CopyAssignments* map)
{
	ASSERT(assignmentLeft);
	DeleteAllPairsWithVar(map, assignmentLeft);

	if (assignmentRight.type == Node_MemberAccess && assignmentLeft != ((MemberAccessExpr*)assignmentRight.ptr)->varReference)
	{
		MemberAccessExpr* memberAccess = assignmentRight.ptr;
		ASSERT(memberAccess->varReference);
		bool success = AddCopyAssignment(map, assignmentLeft, (NodePtr){.ptr = memberAccess->varReference, .type = Node_VariableDeclaration});
		ASSERT(success);
	}
	else if (assignmentRight.ptr && ExprIsConstant(assignmentRight))
	{
		bool success = AddCopyAssignment(map, assignmentLeft, assignmentRight);
		ASSERT(success);
	}
}

static void VisitExpression(NodePtr* node, CopyAssignments* map, bool modifyAST, bool modifyMap)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_MemberAccess:
	{
		if (!modifyAST)
			break;

		MemberAccessExpr* memberAccess = node->ptr;
		if (!memberAccess->varReference)
			break;

		NodePtr valueNode = GetCopyAssignmentValue(map, memberAccess->varReference);
		if (valueNode.type == Node_VariableDeclaration)
		{
			VarDeclStmt* value = valueNode.ptr;
			if (!value->functionParamOf || value->functionParamOf == currentFunction)
				memberAccess->varReference = value;
		}
		else if (valueNode.ptr)
		{
			FreeASTNode(*node);
			*node = CopyASTNode(valueNode);
		}
		break;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node->ptr;
		if (binary->operatorType == Binary_Assignment)
			ASSERT(binary->left.type != Node_MemberAccess);
		VisitExpression(&binary->left, map, modifyAST, modifyMap);
		VisitExpression(&binary->right, map, modifyAST, modifyMap);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression, map, modifyAST, modifyMap);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;

		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* baseExpr = funcCall->baseExpr.ptr;
		ASSERT(baseExpr->funcReference);

		FuncDeclStmt* funcDecl = baseExpr->funcReference;

		for (size_t i = 0; i < funcCall->arguments.length; ++i)
		{
			NodePtr* node = funcCall->arguments.array[i];
			if (funcDecl->modifiers.externalValue && node->type == Node_MemberAccess)
			{
				MemberAccessExpr* memberAccess = node->ptr;
				ASSERT(memberAccess->varReference);
				ProcessAssignment(memberAccess->varReference, NULL_NODE, map);
			}
			else
				VisitExpression(node, map, modifyAST, modifyMap);
		}

		CopyAssignments funcMap = AllocCopyAssignments();

		currentFunction = funcDecl;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			varDecl->functionParamOf = funcDecl;
			VisitStatement(node, &funcMap, modifyAST, modifyMap);
		}
		VisitStatement(&funcDecl->block, &funcMap, modifyAST, modifyMap);
		currentFunction = NULL;

		// FreeCopyAssignments(&funcMap); todo

		currentFunction = funcDecl;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			varDecl->functionParamOf = funcDecl;
			VisitStatement(node, map, false, modifyMap);
		}
		VisitStatement(&funcDecl->block, map, false, modifyMap);
		currentFunction = NULL;
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr, map, modifyAST, modifyMap);
		VisitExpression(&subscript->indexExpr, map, modifyAST, modifyMap);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node->ptr;
		VisitStatement(&blockExpr->block, map, modifyAST, modifyMap);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(NodePtr* node, CopyAssignments* map, bool modifyAST, bool modifyMap)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_FunctionDeclaration:
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
			VisitExpression(&binary->right, map, modifyAST, modifyMap);
			if (modifyMap)
			{
				ASSERT(binary->left.type == Node_MemberAccess);
				MemberAccessExpr* left = binary->left.ptr;
				ASSERT(left->varReference);
				ProcessAssignment(left->varReference, binary->right, map);
			}
		}
		else
			VisitExpression(&exprStmt->expr, map, modifyAST, modifyMap);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer, map, modifyAST, modifyMap);
		if (modifyMap)
			ProcessAssignment(varDecl, varDecl->initializer, map);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node->ptr;
		VisitStatement(&input->varDecl, map, modifyAST, modifyMap);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i], map, modifyAST, modifyMap);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitExpression(&ifStmt->expr, map, modifyAST, modifyMap);

		CopyAssignments otherMap = CopyCopyAssignments(map);
		VisitStatement(&ifStmt->falseStmt, &otherMap, modifyAST, modifyMap);
		VisitStatement(&ifStmt->trueStmt, map, modifyAST, modifyMap);
		MergeCopyAssignments(map, &otherMap);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		CopyAssignments beforeMap = CopyCopyAssignments(map);

		VisitExpression(&whileStmt->expr, map, false, modifyMap);
		VisitStatement(&whileStmt->stmt, map, false, modifyMap);

		MergeCopyAssignments(map, &beforeMap);

		VisitExpression(&whileStmt->expr, map, modifyAST, false);
		VisitStatement(&whileStmt->stmt, map, modifyAST, false);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void CopyPropagationPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
		{
			NodePtr* node = module->statements.array[i];
			if (node->type == Node_Null || node->type == Node_Import || node->type == Node_Input || node->type == Node_Desc)
				continue;

			CopyAssignments map = AllocCopyAssignments();
			ASSERT(node->type == Node_Section);
			SectionStmt* section = node->ptr;
			ASSERT(section->block.type == Node_BlockStatement);
			VisitStatement(&section->block, &map, true, true);
			FreeCopyAssignments(&map);
		}
	}
}
