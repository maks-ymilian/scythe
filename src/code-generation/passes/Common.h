#pragma once

#include "SyntaxTree.h"

#define ARRAY_STRUCT_MEMBER_COUNT 2
#define ARRAY_STRUCT_PTR_MEMBER_INDEX 0
#define ARRAY_STRUCT_LENGTH_MEMBER_INDEX 1

typedef struct
{
	StructDeclStmt* effectiveType;
	StructDeclStmt* pointerType;
	bool isPointer;
} StructTypeInfo;

typedef struct
{
	PrimitiveType effectiveType;
	PrimitiveType pointerType;
	bool pointerTypeIsStruct;
	bool isPointer;
} PrimitiveTypeInfo;

StructTypeInfo GetStructTypeInfoFromType(Type type);
StructTypeInfo GetStructTypeInfoFromExpr(NodePtr node);
PrimitiveTypeInfo GetPrimitiveTypeInfoFromType(Type type);
PrimitiveTypeInfo GetPrimitiveTypeInfoFromExpr(NodePtr node);
Type AllocTypeFromExpr(NodePtr node, int lineNumber);

VarDeclStmt* GetPtrMember(StructDeclStmt* type);

NodePtr AllocIdentifier(VarDeclStmt* varDecl, int lineNumber);
NodePtr AllocSetVariable(VarDeclStmt* varDecl, NodePtr value, int lineNumber);
NodePtr AllocAssignmentStatement(NodePtr left, NodePtr right, int lineNumber);
NodePtr AllocInteger(uint64_t value, int lineNumber);
NodePtr AllocIntConversion(NodePtr expr, int lineNumber);
