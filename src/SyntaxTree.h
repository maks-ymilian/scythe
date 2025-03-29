#pragma once

#include "Token.h"
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
	Node_MemberAccess,
	Node_BlockExpression,

	Node_ExpressionStatement,

	Node_Import,
	Node_Section,
	Node_VariableDeclaration,
	Node_FunctionDeclaration,
	Node_StructDeclaration,

	Node_BlockStatement,

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

	Binary_BitAnd,
	Binary_BitOr,
	Binary_XOR,

	Binary_Add,
	Binary_Subtract,
	Binary_Multiply,
	Binary_Divide,
	Binary_Exponentiation,
	Binary_Modulo,
	Binary_LeftShift,
	Binary_RightShift,

	Binary_Assignment,
	Binary_AddAssign,
	Binary_SubtractAssign,
	Binary_MultiplyAssign,
	Binary_DivideAssign,
	Binary_ModuloAssign,
	Binary_ExponentAssign,
	Binary_BitAndAssign,
	Binary_BitOrAssign,
	Binary_XORAssign,
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
	int lineNumber;

	enum
	{
		Unary_Plus,
		Unary_Minus,
		Unary_Negate,
		Unary_Increment,
		Unary_Decrement,
	} operatorType;

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
		char* floatValue;
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
	Array arguments;
} FuncCallExpr;

typedef struct
{
	int lineNumber;
	NodePtr value;
	NodePtr next;
} MemberAccessExpr;

typedef struct
{
	NodePtr expr;
	bool array;
} Type;

typedef struct
{
	Type type;
	NodePtr block;
} BlockExpr;


typedef struct
{
	int lineNumber;
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
	Type type;
	Type arrayType;
	char* name;
	char* externalName;
	NodePtr initializer;
	Array instantiatedVariables;
	bool public;
	bool external;
	int uniqueName;
} VarDeclStmt;

typedef struct
{
	int lineNumber;
	Type type;
	Type oldType;
	char* name;
	char* externalName;
	Array parameters;
	Array oldParameters;
	NodePtr block;
	VarDeclStmt* globalReturn;
	bool public;
	bool external;
	int uniqueName;
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
	int lineNumber;
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
	int lineNumber;

	enum
	{
		LoopControl_Break,
		LoopControl_Continue,
	} type;
} LoopControlStmt;

typedef typeof(((LoopControlStmt*)0)->type) LoopControlType;


typedef struct
{
	int lineNumber;
	NodePtr expr;
	FuncDeclStmt* function;
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

extern TokenType binaryOperatorToTokenType[];
extern BinaryOperator tokenTypeToBinaryOperator[];

extern TokenType unaryOperatorToTokenType[];
extern UnaryOperator tokenTypeToUnaryOperator[];

extern TokenType primitiveTypeToTokenType[];
extern PrimitiveType tokenTypeToPrimitiveType[];

extern BinaryOperator getCompoundAssignmentOperator[];

NodePtr AllocASTNode(const void* node, size_t size, NodeType type);
NodePtr CopyASTNode(NodePtr node);
void FreeASTNode(NodePtr node);
void FreeAST(AST root);
