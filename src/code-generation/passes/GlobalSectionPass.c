#include "GlobalSectionPass.h"

#include <stdio.h>
#include <string.h>

static NodePtr globalInitSection;

static void AddToInitSection(const NodePtr* node)
{
    assert(globalInitSection.type == Node_Section);
    const SectionStmt* section = globalInitSection.ptr;
    assert(section->block.type == Node_Block);
    BlockStmt* block = section->block.ptr;
    ArrayAdd(&block->statements, node);
}

static Result VisitBlock(const BlockStmt* block)
{
    return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
    assert(section->block.type == Node_Block);
    HANDLE_ERROR(VisitBlock(section->block.ptr));

    return SUCCESS_RESULT;
}

static Result VisitModule(ModuleNode* module)
{
    for (int i = 0; i < module->statements.length; ++i)
    {
        const NodePtr* stmt = module->statements.array[i];
        switch (stmt->type)
        {
        case Node_StructDeclaration:
        case Node_FunctionDeclaration:
        case Node_VariableDeclaration:
        {
            AddToInitSection(stmt);
            ArrayRemove(&module->statements, i);
            i--;
            break;
        }
        case Node_Section: VisitSection(stmt->ptr);
            break;
        case Node_Import: break;
        default: assert(0);
        }
    }

    return SUCCESS_RESULT;
}

Result GlobalSectionPass(const AST* ast)
{
    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        ModuleNode* module = node->ptr;

        globalInitSection = AllocASTNode(
            &(SectionStmt)
            {
                .lineNumber = -1,
                .sectionType = Section_Init,
                .block = AllocASTNode(
                    &(BlockStmt)
                    {
                        .statements = AllocateArray(sizeof(NodePtr)),
                    }, sizeof(BlockStmt), Node_Block),
            }, sizeof(SectionStmt), Node_Section);

        HANDLE_ERROR(VisitModule(module));

        ArrayAdd(&module->statements, &globalInitSection);
    }

    return SUCCESS_RESULT;
}
