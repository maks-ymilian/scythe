#include "Common.h"

#include <assert.h>

NodePtr AllocIdentifier(VarDeclStmt* varDecl, int lineNumber)
{
	assert(varDecl != NULL);
	return AllocASTNode(
		&(MemberAccessExpr){
			.lineNumber = lineNumber,
			.start = NULL,
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
