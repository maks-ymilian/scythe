#include "CopyPropagationPass.h"

#include "data-structures/Map.h"

#include <string.h>

typedef Map CopyAssignments;

static FuncDeclStmt* currentFunction;

static void VisitStatement(NodePtr* node, CopyAssignments* map, bool modifyAST);

static CopyAssignments AllocCopyAssignments()
{
	return AllocateMap(sizeof(VarDeclStmt*));
}

static CopyAssignments FreeCopyAssignments(const CopyAssignments* map)
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
			if (*(VarDeclStmt**)i->value == *(VarDeclStmt**)j->value && strcmp(i->key, j->key) == 0)
			{
				exists = true;
				break;
			}
		}

		if (exists == false)
			ArrayAdd(&deleteKeys, &i->key);
	}
	FreeCopyAssignments(map2);

	for (size_t i = 0; i < deleteKeys.length; ++i)
		MapRemove(map1, *(char**)deleteKeys.array[i]);
	FreeArray(&deleteKeys);
}

static VarDeclStmt* GetCopyAssignmentValue(const CopyAssignments* map, const VarDeclStmt* key)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (void*)key);
	VarDeclStmt** value = MapGet(map, keyStr);
	if (!value)
		return NULL;
	return *value;
}

static bool AddCopyAssignment(CopyAssignments* map, const VarDeclStmt* key, const VarDeclStmt* value)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (void*)key);
	return MapAdd(map, keyStr, &value);
}

static void DeleteAllPairsWithVar(CopyAssignments* map, const VarDeclStmt* var)
{
	char keyStr[64];
	snprintf(keyStr, sizeof(keyStr), "%p", (void*)var);
	VarDeclStmt** value = MapGet(map, keyStr);

	Array deleteKeys = AllocateArray(sizeof(char*));
	for (MAP_ITERATE(i, map))
	{
		if (*(VarDeclStmt**)i->value == var || strcmp(i->key, keyStr) == 0)
			ArrayAdd(&deleteKeys, &i->key);
	}

	for (size_t i = 0; i < deleteKeys.length; ++i)
		MapRemove(map, *(char**)deleteKeys.array[i]);
	FreeArray(&deleteKeys);
}

static void VisitExpression(NodePtr* node, CopyAssignments* map, bool modifyAST)
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

		VarDeclStmt* value = GetCopyAssignmentValue(map, memberAccess->varReference);
		if (value && !(value->functionParamOf && value->functionParamOf != currentFunction))
			memberAccess->varReference = value;
		break;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node->ptr;
		if (binary->operatorType == Binary_Assignment)
			ASSERT(binary->left.type != Node_MemberAccess);
		VisitExpression(&binary->left, map, modifyAST);
		VisitExpression(&binary->right, map, modifyAST);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression, map, modifyAST);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(funcCall->arguments.array[i], map, modifyAST);

		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* baseExpr = funcCall->baseExpr.ptr;
		ASSERT(baseExpr->funcReference);

		FuncDeclStmt* funcDecl = baseExpr->funcReference;
		currentFunction = funcDecl;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
		{
			NodePtr* node = funcDecl->parameters.array[i];
			ASSERT(node->type == Node_VariableDeclaration);
			VarDeclStmt* varDecl = node->ptr;
			varDecl->functionParamOf = funcDecl;
			VisitStatement(node, map, modifyAST);
		}
		VisitStatement(&funcDecl->block, map, modifyAST);
		currentFunction = NULL;
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr, map, modifyAST);
		VisitExpression(&subscript->indexExpr, map, modifyAST);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node->ptr;
		VisitStatement(&blockExpr->block, map, modifyAST);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void ProcessAssignment(NodePtr node, CopyAssignments* map)
{
	VarDeclStmt* assignmentLeft = NULL;
	VarDeclStmt* assignmentRight = NULL;

	if (node.type == Node_VariableDeclaration)
	{
		VarDeclStmt* varDecl = node.ptr;
		assignmentLeft = varDecl;

		if (varDecl->initializer.type == Node_MemberAccess)
		{
			MemberAccessExpr* right = varDecl->initializer.ptr;
			ASSERT(right->varReference);
			assignmentRight = right->varReference;
		}
	}
	else if (node.type == Node_Binary)
	{
		BinaryExpr* binary = node.ptr;

		ASSERT(binary->left.type == Node_MemberAccess);
		MemberAccessExpr* left = binary->left.ptr;
		ASSERT(left->varReference);
		assignmentLeft = left->varReference;

		if (binary->left.ptr != binary->right.ptr &&
			binary->right.type == Node_MemberAccess)
		{
			MemberAccessExpr* right = binary->right.ptr;
			ASSERT(right->varReference);
			assignmentRight = right->varReference;
		}
	}
	else
		UNREACHABLE();

	ASSERT(assignmentLeft);
	DeleteAllPairsWithVar(map, assignmentLeft);

	if (assignmentRight)
	{
		bool success = AddCopyAssignment(map, assignmentLeft, assignmentRight);
		ASSERT(success);
	}
}

static void VisitStatement(NodePtr* node, CopyAssignments* map, bool modifyAST)
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
			VisitExpression(&binary->right, map, modifyAST);
			ProcessAssignment((NodePtr){.ptr = binary, .type = Node_Binary}, map);
		}
		else
			VisitExpression(&exprStmt->expr, map, modifyAST);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer, map, modifyAST);
		ProcessAssignment(*node, map);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node->ptr;
		VisitStatement(&input->varDecl, map, modifyAST);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i], map, modifyAST);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitExpression(&ifStmt->expr, map, modifyAST);

		CopyAssignments otherMap = CopyCopyAssignments(map);
		VisitStatement(&ifStmt->falseStmt, &otherMap, modifyAST);
		VisitStatement(&ifStmt->trueStmt, map, modifyAST);
		MergeCopyAssignments(map, &otherMap);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		CopyAssignments beforeMap = CopyCopyAssignments(map);

		VisitExpression(&whileStmt->expr, map, false);
		VisitStatement(&whileStmt->stmt, map, false);
		VisitExpression(&whileStmt->expr, map, modifyAST);
		VisitStatement(&whileStmt->stmt, map, modifyAST);

		MergeCopyAssignments(map, &beforeMap);
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
			if (node->type == Node_Null || node->type == Node_Import || node->type == Node_Input)
				continue;

			CopyAssignments map = AllocCopyAssignments();

			ASSERT(node->type == Node_Section);
			SectionStmt* section = node->ptr;
			ASSERT(section->block.type == Node_BlockStatement);
			if (section->sectionType != Section_Init)
				VisitStatement(&section->block, &map, false);
			VisitStatement(&section->block, &map, true);

			FreeCopyAssignments(&map);
		}
	}
}
