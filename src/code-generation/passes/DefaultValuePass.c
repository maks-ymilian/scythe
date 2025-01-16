#include "DefaultValuePass.h"

#include <assert.h>

static const char* currentFilePath = NULL;

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	default: unreachable();
	}
	return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_Block);
	const BlockStmt* block = section->block.ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));

	return SUCCESS_RESULT;
}

static Result VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			PROPAGATE_ERROR(VisitSection(stmt->ptr));
			break;
		case Node_Import:
			break;
		default: unreachable();
		}
	}
	return SUCCESS_RESULT;
}

Result DefaultValuePass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	return SUCCESS_RESULT;
}
