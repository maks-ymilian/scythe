#include "ResolverPass.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "StringUtils.h"
#include "data-structures/Map.h"

static const char* currentFilePath = NULL;

static Map modules;

typedef struct Scope Scope;

struct Scope
{
	Scope* parent;
	Map declarations;
};

static Scope* currentScope = NULL;

static void PushScope()
{
	Scope* new = malloc(sizeof(Scope));
	*new = (Scope){
		.declarations = AllocateMap(sizeof(NodePtr)),
		.parent = NULL,
	};

	if (currentScope == NULL)
	{
		currentScope = new;
		return;
	}

	new->parent = currentScope;
	currentScope = new;
}

static void PopScope(Map* outMap)
{
	assert(currentScope != NULL);
	Scope* scope = currentScope;

	currentScope = currentScope->parent;

	if (outMap == NULL)
		FreeMap(&scope->declarations);
	else
		*outMap = scope->declarations;

	free(scope);
}

static Result RegisterDeclaration(const char* name, const NodePtr* node, const int lineNumber)
{
	assert(currentScope != NULL);
	if (!MapAdd(&currentScope->declarations, name, node))
		return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), lineNumber, currentFilePath);
	return SUCCESS_RESULT;
}

static bool NodeIsPublicDeclaration(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration: return ((VarDeclStmt*)node->ptr)->public;
	case Node_FunctionDeclaration: return ((FuncDeclStmt*)node->ptr)->public;
	case Node_StructDeclaration: return ((StructDeclStmt*)node->ptr)->public;
	default: unreachable();
	}
}

static Result InitializeIdentifierReference(
	IdentifierReference* identifier,
	const int lineNumber,
	const Map* module,
	const char* moduleName)
{
	if (module != NULL)
	{
		assert(moduleName != NULL);

		const NodePtr* node = MapGet(module, identifier->text);
		if (node == NULL)
			return ERROR_RESULT(
				AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", identifier->text, moduleName),
				lineNumber,
				currentFilePath);

		if (!NodeIsPublicDeclaration(node))
			return ERROR_RESULT(
				AllocateString2Str("Declaration \"%s\" in module \"%s\" is private", identifier->text, moduleName),
				lineNumber,
				currentFilePath);

		identifier->reference = *node;
		return SUCCESS_RESULT;
	}
	assert(moduleName == NULL);

	assert(currentScope != NULL);
	const Scope* scope = currentScope;
	while (scope != NULL)
	{
		void* get = MapGet(&scope->declarations, identifier->text);
		if (get != NULL)
		{
			identifier->reference = *(NodePtr*)get;
			return SUCCESS_RESULT;
		}

		scope = scope->parent;
	}

	identifier->reference = NULL_NODE;
	return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", identifier->text), lineNumber, currentFilePath);
}

static Result ResolveExpression(const NodePtr* node, const Map* module, const char* moduleName);

static Result ResolveMemberAccess(const MemberAccessExpr* memberAccess)
{
	const MemberAccessExpr* current = memberAccess;

	const Map* module = NULL;
	const char* moduleName = NULL;
	if (current->value.type == Node_Literal)
	{
		const LiteralExpr* literal = current->value.ptr;
		PROPAGATE_ERROR(ResolveExpression(&current->value, NULL, NULL));

		assert(literal->type == Literal_Identifier);
		if (literal->identifier.reference.type == Node_Import)
		{
			const ImportStmt* import = literal->identifier.reference.ptr;
			moduleName = import->moduleName;
			assert(moduleName);
			module = MapGet(&modules, moduleName);
			assert(module);
		}

		if (current->next.ptr == NULL)
			return SUCCESS_RESULT;

		assert(current->next.type == Node_MemberAccess);
		current = current->next.ptr;
	}

	while (true)
	{
		PROPAGATE_ERROR(ResolveExpression(&current->value, module, moduleName));

		if (current->next.ptr == NULL)
			break;

		assert(current->next.type == Node_MemberAccess);
		current = current->next.ptr;
	}

	return SUCCESS_RESULT;
}

static Result ResolveExpression(const NodePtr* node, const Map* module, const char* moduleName)
{
	assert(
		(module != NULL && moduleName != NULL) ||
		(module == NULL && moduleName == NULL));

	switch (node->type)
	{
	case Node_Literal:
	{
		LiteralExpr* literal = node->ptr;

		if (literal->type == Literal_Identifier)
		{
			if (literal->identifier.reference.ptr == NULL)
				PROPAGATE_ERROR(InitializeIdentifierReference(
					&literal->identifier,
					literal->lineNumber,
					module,
					moduleName));

			return SUCCESS_RESULT;
		}
		else if (literal->type == Literal_Int ||
				 literal->type == Literal_Float ||
				 literal->type == Literal_String ||
				 literal->type == Literal_Boolean)
			return SUCCESS_RESULT;

		unreachable();
		return SUCCESS_RESULT;
	}
	case Node_Binary:
	{
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&binary->left, module, moduleName));
		PROPAGATE_ERROR(ResolveExpression(&binary->right, module, moduleName));
		return SUCCESS_RESULT;
	}
	case Node_Unary:
	{
		const UnaryExpr* unary = node->ptr;
		return ResolveExpression(&unary->expression, module, moduleName);
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* memberAccess = node->ptr;
		PROPAGATE_ERROR(ResolveMemberAccess(memberAccess));
		return SUCCESS_RESULT;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;

		if (funcCall->identifier.reference.ptr == NULL)
			PROPAGATE_ERROR(InitializeIdentifierReference(
				&funcCall->identifier,
				funcCall->lineNumber,
				module,
				moduleName));

		for (size_t i = 0; i < funcCall->parameters.length; ++i)
		{
			const NodePtr* node = funcCall->parameters.array[i];
			PROPAGATE_ERROR(ResolveExpression(node, module, moduleName));
		}

		return SUCCESS_RESULT;
	}
	case Node_ArrayAccess:
	{
		ArrayAccessExpr* arrayAccess = node->ptr;

		if (arrayAccess->identifier.reference.ptr == NULL)
			PROPAGATE_ERROR(InitializeIdentifierReference(
				&arrayAccess->identifier,
				arrayAccess->lineNumber,
				module,
				moduleName));

		PROPAGATE_ERROR(ResolveExpression(&arrayAccess->subscript, module, moduleName));

		return SUCCESS_RESULT;
	}
	case Node_Null: return SUCCESS_RESULT;
	default: unreachable();
	}
}

static Result VisitStatement(const NodePtr* node);

static Result VisitBlock(const BlockStmt* block)
{
	PushScope();
	for (size_t i = 0; i < block->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
	PopScope(NULL);

	return SUCCESS_RESULT;
}

static Result InitializeTypeReference(
	TypeReference* inout,
	const int lineNumber,
	const Map* module,
	const char* moduleName)
{
	assert(
		(module != NULL && moduleName != NULL) ||
		(module == NULL && moduleName == NULL));

	if (inout->primitive)
		return SUCCESS_RESULT;

	IdentifierReference hack = {.text = inout->text, .reference = NULL_NODE};
	PROPAGATE_ERROR(InitializeIdentifierReference(&hack, lineNumber, module, moduleName));
	inout->reference = hack.reference;
	assert(inout->reference.type == Node_StructDeclaration);

	return SUCCESS_RESULT;
}

static Result RecursiveRegisterImportNode(const NodePtr* importNode, const bool topLevel)
{
	assert(importNode->type == Node_Import);
	const ImportStmt* import = importNode->ptr;

	const Map* module = MapGet(&modules, import->moduleName);
	assert(module != NULL);

	PROPAGATE_ERROR(RegisterDeclaration(import->moduleName, importNode, import->lineNumber));

	if (import->public || topLevel)
	{
		for (MAP_ITERATE(i, module))
		{
			const NodePtr* node = i->value;
			if (node->type == Node_Import &&
				((ImportStmt*)node->ptr)->public)
				RecursiveRegisterImportNode(node, false);
		}
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&varDecl->initializer, NULL, NULL));
		PROPAGATE_ERROR(ResolveExpression(&varDecl->arrayLength, NULL, NULL));
		PROPAGATE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
		PROPAGATE_ERROR(InitializeTypeReference(&varDecl->type, varDecl->lineNumber, NULL, NULL));
		return SUCCESS_RESULT;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;

		PushScope();
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			PROPAGATE_ERROR(VisitStatement(funcDecl->parameters.array[i]));

		if (funcDecl->block.ptr != NULL)
		{
			assert(funcDecl->block.type == Node_Block);
			const BlockStmt* block = funcDecl->block.ptr;
			for (size_t i = 0; i < block->statements.length; ++i)
				PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		}
		PopScope(NULL);

		PROPAGATE_ERROR(InitializeTypeReference(&funcDecl->type, funcDecl->lineNumber, NULL, NULL));
		PROPAGATE_ERROR(RegisterDeclaration(funcDecl->name, node, funcDecl->lineNumber));
		return SUCCESS_RESULT;
	}
	case Node_StructDeclaration:
	{
		const StructDeclStmt* structDecl = node->ptr;

		PushScope();
		for (size_t i = 0; i < structDecl->members.length; ++i)
			PROPAGATE_ERROR(VisitStatement(structDecl->members.array[i]));
		PopScope(NULL);

		PROPAGATE_ERROR(RegisterDeclaration(structDecl->name, node, structDecl->lineNumber));
		return SUCCESS_RESULT;
	}

	case Node_Block: return VisitBlock(node->ptr);
	case Node_ExpressionStatement: return ResolveExpression(&((ExpressionStmt*)node->ptr)->expr, NULL, NULL);
	case Node_Return: return ResolveExpression(&((ReturnStmt*)node->ptr)->expr, NULL, NULL);

	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		assert(section->block.type == Node_Block);
		return VisitBlock(section->block.ptr);
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&ifStmt->expr, NULL, NULL));
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PopScope(NULL);
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_While:
	{
		const WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&whileStmt->expr, NULL, NULL));
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_For:
	{
		const ForStmt* forStmt = node->ptr;
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&forStmt->initialization));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->condition, NULL, NULL));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->increment, NULL, NULL));
		PROPAGATE_ERROR(VisitStatement(&forStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}

	case Node_Import:
	{
		PROPAGATE_ERROR(RecursiveRegisterImportNode(node, true));
		return SUCCESS_RESULT;
	}
	case Node_LoopControl:
	case Node_Null:
		return SUCCESS_RESULT;
	default: unreachable();
	}
}

static Result VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	PushScope();
	for (size_t i = 0; i < module->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));

	Map declarations;
	PopScope(&declarations);
	if (!MapAdd(&modules, module->moduleName, &declarations))
		unreachable();

	return SUCCESS_RESULT;
}

Result ResolverPass(const AST* ast)
{
	modules = AllocateMap(sizeof(Map));
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	for (MAP_ITERATE(i, &modules))
		FreeMap(i->value);
	FreeMap(&modules);

	return SUCCESS_RESULT;
}
