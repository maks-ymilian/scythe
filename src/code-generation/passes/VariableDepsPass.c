#include "VariableDepsPass.h"

#include "data-structures/Map.h"
#include "data-structures/Array.h"
#include "StringUtils.h"

typedef Map Env;

static SectionStmt* currentSection;

static Env VisitStatement(NodePtr node, Env* env);

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

static void EnvSetDep(Env* env, const VarDeclStmt* var, NodePtr dep, bool add)
{
	char key[64];
	snprintf(key, sizeof(key), "%p", (void*)var);
	Array* deps = MapGet(env, key);
	if (deps)
	{
		if (!add)
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
}

static Array* EnvGetDeps(Env* env, const VarDeclStmt* var)
{
	char key[64];
	snprintf(key, sizeof(key), "%p", (void*)var);
	Array* deps = MapGet(env, key);
	ASSERT(deps);
	ASSERT(deps->length > 0);
	return deps;
}

static void VisitExpression(NodePtr node, Env* env)
{
	switch (node.type)
	{
	case Node_Literal:
	case Node_Null:
		break;
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		ASSERT(memberAccess->varReference);

		if (memberAccess->varReference->inputStmt)
			break;

		// for each dep that is in the env but not in the memberAccess, +1 the assignments uses counter
		Array* envDeps = EnvGetDeps(env, memberAccess->varReference);
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
		VisitExpression(binary->left, env);
		VisitExpression(binary->right, env);
		break;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		VisitExpression(unary->expression, env);
		break;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(*(NodePtr*)funcCall->arguments.array[i], env);

		// this pass should walk in the way of the control flow,
		// dont enter function declarations, enter functions through function calls
		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		ASSERT(memberAccess->funcReference);
		for (size_t i = 0; i < memberAccess->funcReference->parameters.length; ++i)
			VisitStatement(*(NodePtr*)memberAccess->funcReference->parameters.array[i], env);
		VisitStatement(memberAccess->funcReference->block, env);
		break;
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		VisitExpression(subscript->baseExpr, env);
		VisitExpression(subscript->indexExpr, env);
		break;
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node.ptr;
		VisitStatement(block->block, env);
		break;
	}
	default: INVALID_VALUE(node.type);
	}
}

static Env VisitStatement(NodePtr node, Env* env)
{
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
		exprStmt->section = currentSection;

		if (exprStmt->expr.type == Node_Binary &&
			((BinaryExpr*)exprStmt->expr.ptr)->operatorType == Binary_Assignment &&
			((BinaryExpr*)exprStmt->expr.ptr)->left.type == Node_MemberAccess)
		{
			BinaryExpr* binary = exprStmt->expr.ptr;
			MemberAccessExpr* memberAccess = binary->left.ptr;
			ASSERT(memberAccess->varReference);
			VisitExpression(binary->right, env);

			bool lastDepIsInDifferentSection = false;
			Array* deps = EnvGetDeps(env, memberAccess->varReference);
			for (size_t i = 0; i < deps->length; ++i)
			{
				NodePtr* dep = deps->array[i];
				SectionStmt* section = NULL;
				if (dep->type == Node_ExpressionStatement)
					section = ((ExpressionStmt*)dep->ptr)->section;
				else if (dep->type == Node_VariableDeclaration)
					section = ((VarDeclStmt*)dep->ptr)->section;
				else
					UNREACHABLE();
				ASSERT(section);

				if (section != exprStmt->section)
				{
					lastDepIsInDifferentSection = true;
					break;
				}
			}

			EnvSetDep(env, memberAccess->varReference, node, lastDepIsInDifferentSection);
		}
		else
			VisitExpression(exprStmt->expr, env);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node.ptr;
		varDecl->section = currentSection;
		VisitExpression(varDecl->initializer, env);
		EnvSetDep(env, varDecl, node, false);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node.ptr;
		VisitStatement(input->varDecl, env);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node.ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(*(NodePtr*)block->statements.array[i], env);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node.ptr;
		VisitExpression(ifStmt->expr, env);

		Env otherEnv = EnvCopy(env);
		VisitStatement(ifStmt->trueStmt, &otherEnv);
		VisitStatement(ifStmt->falseStmt, env);
		// when exiting an if, merge the two envs from the two branches
		EnvMerge(env, &otherEnv);
		EnvFree(&otherEnv);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node.ptr;
		Env beforeEnv = EnvCopy(env);
		// while loops must be visited twice because they can run multiple times
		VisitExpression(whileStmt->expr, env);
		VisitStatement(whileStmt->stmt, env);
		VisitExpression(whileStmt->expr, env);
		VisitStatement(whileStmt->stmt, env);

		// merge it with the env from before the while loop
		EnvMerge(env, &beforeEnv);
		EnvFree(&beforeEnv);
		break;
	}
	default: INVALID_VALUE(node.type);
	}

	return *env;
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
		{
			NodePtr* node = module->statements.array[i];
			if (node->type == Node_Null || node->type == Node_Import || node->type == Node_Input)
				continue;
			ASSERT(node->type == Node_Section);
			SectionStmt* section = node->ptr;
			currentSection = section;

			ASSERT(section->block.type == Node_BlockStatement);
			VisitStatement(section->block, &env);
			// sections other than @init must be visited twice because they can run multiple times
			if (section->sectionType != Section_Init)
				VisitStatement(section->block, &env);
		}
	}
	EnvFree(&env);
}
