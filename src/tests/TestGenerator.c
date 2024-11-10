#include "TestGenerator.h"

#include "code-generation/Common.h"
#include "code-generation/CodeGenerator.h"
#include "SyntaxTree.h"

char* GenerateTestSource(size_t* outLength)
{
    BlockStmt* blockStmt = AllocBlockStmt((BlockStmt){.statements = AllocateArray(sizeof(NodePtr))});
    VarDeclStmt* varDecl = AllocVarDeclStmt((VarDeclStmt)
    {
        .type = (Token){.type = Float, .text = NULL, .lineNumber = 0},
        .initializer = (NodePtr){.ptr = NULL, .type = NullNode},
        .identifier = (Token){.type = Identifier, .text = "yooo", .lineNumber = 0}
    });
    const NodePtr statement1 = (NodePtr){.ptr = varDecl, .type = VariableDeclaration};
    ArrayAdd(&blockStmt->statements, &statement1);

    const NodePtr blockPtr = (NodePtr){.ptr = blockStmt, .type = BlockStatement};
    SectionStmt* section = AllocSectionStmt((SectionStmt)
    {
        .type = (Token){.type = Init, .text = NULL, .lineNumber = 0},
        .block = blockPtr,
    });
    const NodePtr statement = (NodePtr){.type = Section, .ptr = section};
    Program* syntaxTree = AllocProgram((Program){.statements = AllocateArray(sizeof(NodePtr))});
    ArrayAdd(&syntaxTree->statements, &statement);

    uint8_t* source = NULL;
    ASSERT_ERROR(GenerateCode(syntaxTree, &source, outLength));
    return (char*)source;
}
