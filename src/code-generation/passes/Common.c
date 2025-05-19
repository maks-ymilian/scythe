#include "Common.h"

#include <assert.h>

VarDeclStmt* GetPtrMember(StructDeclStmt* type)
{
	assert(type->isArrayType);
	assert(type->members.length == ARRAY_STRUCT_MEMBER_COUNT);
	NodePtr* node = type->members.array[ARRAY_STRUCT_PTR_MEMBER_INDEX];
	assert(node->type == Node_VariableDeclaration);
	return node->ptr;
}

NodePtr AllocIdentifier(VarDeclStmt* varDecl, int lineNumber)
{
	assert(varDecl != NULL);
	return AllocASTNode(
		&(MemberAccessExpr){
			.lineNumber = lineNumber,
			.start = NULL_NODE,
			.identifiers = (Array){.array = NULL},
			.funcReference = NULL,
			.typeReference = NULL,
			.varReference = varDecl,
			.parentReference = NULL,
		},
		sizeof(MemberAccessExpr), Node_MemberAccess);
}

NodePtr AllocAssignmentStatement(NodePtr left, NodePtr right, int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.left = left,
					.right = right,
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

NodePtr AllocSetVariable(VarDeclStmt* varDecl, NodePtr value, int lineNumber)
{
	return AllocAssignmentStatement(AllocIdentifier(varDecl, lineNumber), value, lineNumber);
}

NodePtr AllocInteger(uint64_t value, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_Int,
			.intValue = value,
		},
		sizeof(LiteralExpr), Node_Literal);
}
