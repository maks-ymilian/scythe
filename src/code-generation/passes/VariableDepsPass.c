#include "VariableDepsPass.h"

#include "data-structures/Map.h"
#include "data-structures/Array.h"
#include "StringUtils.h"

typedef Map Env;

static Env EnvAlloc(void)
{
	return AllocateMap(sizeof(Array));
}

static void EnvFree(Env* env)
{
	for (MAP_ITERATE(i, env))
	{
		Array* deps = i->value;
		FreeArray(deps);
	}
	FreeMap(env);
}

static Array DepsCopy(const Array* deps)
{
	Array newDeps = AllocateArray(sizeof(NodePtr));
	for (size_t i = 0; i < deps->length; ++i)
		ArrayAdd(&newDeps, deps->array[i]);
	return newDeps;
}

static Env EnvCopy(const Env* env)
{
	Env new = EnvAlloc();
	for (MAP_ITERATE(i, env))
	{
		Array* deps = i->value;
		Array newDeps = DepsCopy(deps);
		MapAdd(&new, i->key, &newDeps);
	}
	return new;
}

static void DepsMerge(Array* dest, const Array* deps)
{
	ASSERT(dest);

	if (!dest->array)
	{
		*dest = DepsCopy(deps);
		return;
	}

	for (size_t i = 0; i < deps->length; ++i)
	{
		NodePtr* dep = deps->array[i];

		bool exists = false;
		for (size_t j = 0; j < dest->length; ++j)
		{
			NodePtr* destDep = dest->array[j];
			ASSERT(destDep->ptr);
			ASSERT(dep->ptr);
			if (destDep->ptr == dep->ptr)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
			ArrayAdd(dest, dep);
	}
}

static void EnvMerge(Env* dest, const Env* env)
{
	for (MAP_ITERATE(i, env))
	{
		Array* envDeps = i->value;
		Array* destDeps = MapGet(dest, i->key);
		if (destDeps)
			DepsMerge(destDeps, envDeps);
		else
		{
			Array newDeps = DepsCopy(envDeps);
			MapAdd(dest, i->key, &newDeps);
		}
	}
}

static void EnvSetDep(Env* env, const VarDeclStmt* var, NodePtr dep)
{
	char* key = AllocateString1Str1Int("%s%d", var->name, var->uniqueName);
	Array* deps = MapGet(env, key);
	if (deps)
	{
		ArrayClear(deps);
		ArrayAdd(deps, &dep);
	}
	else
	{
		Array deps = AllocateArray(sizeof(NodePtr));
		ArrayAdd(&deps, &dep);
		bool result = MapAdd(env, key, &deps);
		ASSERT(result);
	}
	free(key);
}

static Array* EnvGetDeps(Env* env, const VarDeclStmt* var)
{
	char* key = AllocateString1Str1Int("%s%d", var->name, var->uniqueName);
	Array* deps = MapGet(env, key);
	free(key);
	ASSERT(deps);
	ASSERT(deps->length > 0);
	return deps;
}

static Env VisitStatement(NodePtr node, Env* env, bool replace);

static Env VisitExpression(NodePtr node, Env* env, bool replace)
{
	Env new = EnvCopy(env);

	switch (node.type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		ASSERT(memberAccess->varReference);

		// for each dep that is in the env but not in the memberAccess, +1 the assignments uses counter
		Array* envDeps = EnvGetDeps(&new, memberAccess->varReference);
		for (size_t i = 0; i < envDeps->length; ++i)
		{
			NodePtr* envDep = envDeps->array[i];

			bool existsInMemberAccess = false;
			for (size_t j = 0; j < memberAccess->deps.length; ++j)
			{
				NodePtr* useDep = memberAccess->deps.array[j];
				ASSERT(useDep->ptr);
				ASSERT(envDep->ptr);
				if (useDep->ptr == envDep->ptr)
				{
					existsInMemberAccess = true;
					break;
				}
			}

			if (!existsInMemberAccess)
			{
				// +1 the assignments uses counter
				if (envDep->type == Node_VariableDeclaration)
					++((VarDeclStmt*)envDep->ptr)->useCount;
				else if (envDep->type == Node_ExpressionStatement)
					++((ExpressionStmt*)envDep->ptr)->useCount;
				else
					UNREACHABLE();
			}
		}

		// merge its current deps with the deps in the env
		DepsMerge(&memberAccess->deps, envDeps);
		break;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		if (binary->operatorType == Binary_Assignment)
			ASSERT(binary->left.type != Node_MemberAccess);
		new = VisitExpression(binary->left, &new, true);
		new = VisitExpression(binary->right, &new, true);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		new = VisitExpression(unary->expression, &new, true);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			new = VisitExpression(*(NodePtr*)funcCall->arguments.array[i], &new, true);

		// this pass should walk in the way of the control flow,
		// dont enter function declarations, enter functions through function calls
		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		ASSERT(memberAccess->funcReference);
		for (size_t i = 0; i < memberAccess->funcReference->parameters.length; ++i)
			new = VisitStatement(*(NodePtr*)memberAccess->funcReference->parameters.array[i], &new, true);
		new = VisitStatement(memberAccess->funcReference->block, &new, true);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		new = VisitExpression(subscript->baseExpr, &new, true);
		new = VisitExpression(subscript->indexExpr, &new, true);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node.ptr;
		new = VisitStatement(block->block, &new, true);
		break;
	}
	default: INVALID_VALUE(node.type);
	}

	if (replace)
		EnvFree(env);
	return new;
}

static Env VisitStatement(NodePtr node, Env* env, bool replace)
{
	Env new = EnvCopy(env);

	switch (node.type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_FunctionDeclaration:
	case Node_Null:
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node.ptr;

		if (exprStmt->expr.type == Node_Binary &&
			((BinaryExpr*)exprStmt->expr.ptr)->operatorType == Binary_Assignment &&
			((BinaryExpr*)exprStmt->expr.ptr)->left.type == Node_MemberAccess)
		{
			BinaryExpr* binary = exprStmt->expr.ptr;
			MemberAccessExpr* memberAccess = binary->left.ptr;
			ASSERT(memberAccess->varReference);
			new = VisitExpression(binary->right, &new, true);
			EnvSetDep(&new, memberAccess->varReference, node);
		}
		else
			new = VisitExpression(exprStmt->expr, &new, true);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node.ptr;
		new = VisitExpression(varDecl->initializer, &new, true);
		EnvSetDep(&new, varDecl, node);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node.ptr;
		new = VisitStatement(input->varDecl, &new, true);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			new = VisitStatement(*(NodePtr*)block->statements.array[i], &new, true);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node.ptr;
		new = VisitExpression(ifStmt->expr, &new, true);

		Env beforeEnv = EnvCopy(&new);
		new = VisitStatement(ifStmt->trueStmt, &beforeEnv, false);
		Env falseEnv = VisitStatement(ifStmt->falseStmt, &beforeEnv, false);
		// when exiting an if, merge the two envs from the two branches
		EnvMerge(&new, &falseEnv);
		EnvFree(&falseEnv);
		EnvFree(&beforeEnv);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node.ptr;
		ASSERT(section->block.type == Node_BlockStatement);
		new = VisitStatement(section->block, &new, true);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node.ptr;
		Env beforeEnv = EnvCopy(&new);
		// while loops must be called twice because they can run multiple times
		new = VisitExpression(whileStmt->expr, &new, true);
		new = VisitStatement(whileStmt->stmt, &new, true);
		new = VisitExpression(whileStmt->expr, &new, true);
		new = VisitStatement(whileStmt->stmt, &new, true);

		// merge it with the env from before the while loop
		EnvMerge(&new, &beforeEnv);
		EnvFree(&beforeEnv);
		break;
	}
	default: INVALID_VALUE(node.type);
	}

	if (replace)
		EnvFree(env);
	return new;
}

void VariableDepsPass(const AST* ast)
{
	Env env = EnvAlloc();
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			env = VisitStatement(*(NodePtr*)module->statements.array[i], &env, true);
	}
	EnvFree(&env);
}
