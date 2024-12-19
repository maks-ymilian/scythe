#include "GlobalSectionPass.h"

#include <stdio.h>
#include <assert.h>
#include <stddef.h>

static NodePtr globalInitSection;

static void AddToInitSection(const NodePtr* node)
{
    assert(globalInitSection.type == Node_Section);
    const SectionStmt* section = globalInitSection.ptr;
    assert(section->block.type == Node_Block);
    BlockStmt* block = section->block.ptr;
    ArrayAdd(&block->statements, node);
}

static Result VisitBlock(const NodePtr* node)
{
    if (node->type != Node_Block)
        return SUCCESS_RESULT;

    BlockStmt* block = node->ptr;
    for (size_t i = 0; i < block->statements.length; ++i)
    {
        const NodePtr* node = block->statements.array[i];
        switch (node->type)
        {
        case Node_FunctionDeclaration:
        {
            AddToInitSection(node);
            ArrayRemove(&block->statements, i);
            i--;
            break;
        }
        case Node_Block:
        {
            PROPAGATE_ERROR(VisitBlock(node));
            break;
        }
        case Node_If:
        {
            const IfStmt* ifStmt = node->ptr;
            PROPAGATE_ERROR(VisitBlock(&ifStmt->trueStmt));
            PROPAGATE_ERROR(VisitBlock(&ifStmt->falseStmt));
            break;
        }
        case Node_While:
        {
            const WhileStmt* whileStmt = node->ptr;
            PROPAGATE_ERROR(VisitBlock(&whileStmt->stmt));
            break;
        }
        case Node_For:
        {
            const ForStmt* forStmt = node->ptr;
            PROPAGATE_ERROR(VisitBlock(&forStmt->stmt));
            break;
        }
        case Node_Return:
        case Node_LoopControl:
        case Node_ExpressionStatement:
        case Node_VariableDeclaration:
            break;
        default: unreachable();
        }
    }
    return SUCCESS_RESULT;
}

static Result VisitModule(ModuleNode* module)
{
    for (size_t i = 0; i < module->statements.length; ++i)
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
        case Node_Section:
        {
            const SectionStmt* section = stmt->ptr;
            PROPAGATE_ERROR(VisitBlock(&section->block));
            break;
        }
        case Node_Import: break;
        default: unreachable();
        }
    }

    return SUCCESS_RESULT;
}

Result GlobalSectionPass(const AST* ast)
{
    for (size_t i = 0; i < ast->nodes.length; ++i)
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
                    },
                    sizeof(BlockStmt), Node_Block),
            },
            sizeof(SectionStmt), Node_Section);

        PROPAGATE_ERROR(VisitModule(module));

        ArrayInsert(&module->statements, &globalInitSection, 0);
    }

    return SUCCESS_RESULT;
}
