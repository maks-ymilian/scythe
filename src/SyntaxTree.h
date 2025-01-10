#pragma once

#include "data-structures/Array.h"

#define NULL_NODE \
	(NodePtr) { .ptr = NULL, .type = Node_Null }

typedef enum
{
	Node_Null,

	Node_Binary,
	Node_Unary,
	Node_Literal,
	Node_FunctionCall,
	Node_ArrayAccess,
	Node_MemberAccess,

	Node_ExpressionStatement,

	Node_Import,
	Node_Section,
	Node_VariableDeclaration,
	Node_FunctionDeclaration,
	Node_StructDeclaration,

	Node_Block,

	Node_If,
	Node_While,
	Node_For,
	Node_LoopControl,

	Node_Return,

	Node_Module,
} NodeType;

typedef struct
{
	void* ptr;
	NodeType type;
} NodePtr;

typedef struct
{
	char* text;
	NodePtr reference;
} IdentifierReference;


typedef enum
{
	Binary_BoolAnd,
	Binary_BoolOr,
	Binary_IsEqual,
	Binary_NotEqual,
	Binary_GreaterThan,
	Binary_GreaterOrEqual,
	Binary_LessThan,
	Binary_LessOrEqual,

	Binary_Add,
	Binary_Subtract,
	Binary_Multiply,
	Binary_Divide,

	Binary_Assignment,
	Binary_AddAssign,
	Binary_SubtractAssign,
	Binary_MultiplyAssign,
	Binary_DivideAssign,
} BinaryOperator;

typedef struct
{
	int lineNumber;
	BinaryOperator operatorType;
	NodePtr left;
	NodePtr right;
} BinaryExpr;

typedef struct
{
	enum
	{
		Unary_Plus,
		Unary_Minus,
		Unary_Negate,
		Unary_Increment,
		Unary_Decrement,
	} operatorType;

	int lineNumber;
	NodePtr expression;
} UnaryExpr;

typedef typeof(((UnaryExpr*)0)->operatorType) UnaryOperator;

typedef enum
{
	Primitive_Void,
	Primitive_Float,
	Primitive_Int,
	Primitive_Bool,
	Primitive_String,
} PrimitiveType;

typedef struct
{
	int lineNumber;

	enum
	{
		Literal_Float,
		Literal_Int,
		Literal_String,
		Literal_Identifier,
		Literal_Boolean,
		Literal_PrimitiveType,
	} type;

	union
	{
		double floatValue;
		uint64_t intValue;
		char* string;
		IdentifierReference identifier;
		bool boolean;
		PrimitiveType primitiveType;
	};
} LiteralExpr;

typedef typeof(((LiteralExpr*)0)->type) LiteralType;

typedef struct
{
	int lineNumber;
	IdentifierReference identifier;
	Array parameters;
} FuncCallExpr;

typedef struct
{
	int lineNumber;
	IdentifierReference identifier;
	NodePtr subscript;
} ArrayAccessExpr;

typedef struct
{
	NodePtr value;
	NodePtr next;
} MemberAccessExpr;


typedef struct
{
	NodePtr expr;
} ExpressionStmt;


typedef struct
{
	int lineNumber;
	char* path;
	char* moduleName;
	bool public;
} ImportStmt;


typedef struct
{
	enum
	{
		Section_Init,
		Section_Slider,
		Section_Block,
		Section_Sample,
		Section_Serialize,
		Section_GFX,
	} sectionType;

	int lineNumber;
	NodePtr block;
} SectionStmt;

typedef typeof(((SectionStmt*)0)->sectionType) SectionType;

typedef struct
{
	int lineNumber;
	NodePtr type;
	char* name;
	char* externalName;
	NodePtr initializer;
	NodePtr arrayLength;
	bool array;
	bool public;
	bool external;
} VarDeclStmt;

typedef struct
{
	int lineNumber;
	NodePtr type;
	char* name;
	char* externalName;
	Array parameters;
	NodePtr block;
	bool public;
	bool external;
} FuncDeclStmt;

typedef struct
{
	int lineNumber;
	char* name;
	Array members;
	bool public;
} StructDeclStmt;


typedef struct
{
	Array statements;
} BlockStmt;


typedef struct
{
	int lineNumber;
	NodePtr expr;
	NodePtr trueStmt;
	NodePtr falseStmt;
} IfStmt;

typedef struct
{
	int lineNumber;
	NodePtr expr;
	NodePtr stmt;
} WhileStmt;

typedef struct
{
	int lineNumber;
	NodePtr initialization;
	NodePtr condition;
	NodePtr increment;
	NodePtr stmt;
} ForStmt;

typedef struct
{
	enum
	{
		LoopControl_Break,
		LoopControl_Continue,
	} type;

	int lineNumber;
} LoopControlStmt;

typedef typeof(((LoopControlStmt*)0)->type) LoopControlType;


typedef struct
{
	int lineNumber;
	NodePtr expr;
} ReturnStmt;


typedef struct
{
	char* path;
	char* moduleName;
	Array statements;
} ModuleNode;


typedef struct
{
	Array nodes;
} AST;

NodePtr AllocASTNode(const void* node, size_t size, NodeType type);
void FreeASTNode(NodePtr node);
void FreeAST(AST root);
