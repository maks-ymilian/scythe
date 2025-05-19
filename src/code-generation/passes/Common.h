#pragma once

#include "SyntaxTree.h"

#define ARRAY_STRUCT_MEMBER_COUNT 2
#define ARRAY_STRUCT_PTR_MEMBER_INDEX 0
#define ARRAY_STRUCT_LENGTH_MEMBER_INDEX 1

VarDeclStmt* GetPtrMember(StructDeclStmt* type);

NodePtr AllocIdentifier(VarDeclStmt* varDecl, int lineNumber);
NodePtr AllocSetVariable(VarDeclStmt* varDecl, NodePtr value, int lineNumber);
NodePtr AllocAssignmentStatement(NodePtr left, NodePtr right, int lineNumber);
NodePtr AllocInteger(uint64_t value, int lineNumber);
