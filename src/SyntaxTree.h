#pragma once

#include <stdbool.h>

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
	Node_MemberAccess,
	Node_Subscript,
	Node_FunctionCall,
	Node_BlockExpression,
	Node_SizeOf,

	Node_ExpressionStatement,

	Node_Import,
	Node_Section,
	Node_VariableDeclaration,
	Node_FunctionDeclaration,
	Node_StructDeclaration,
	Node_Modifier,

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

typedef enum
{
	Unary_Plus,
	Unary_Minus,
	Unary_Negate,
	Unary_Increment,
	Unary_Decrement,
	Unary_Dereference,
} UnaryOperator;

typedef struct
{
	int lineNumber;
	UnaryOperator operatorType;
	NodePtr expression;
	bool postfix;
} UnaryExpr;

typedef enum
{
	Primitive_Any,
	Primitive_Float,
	Primitive_Int,
	Primitive_Bool,
	Primitive_Void,
} PrimitiveType;

typedef enum
{
	Literal_Float,
	Literal_Int,
	Literal_String,
	Literal_Char,
	Literal_Boolean,
	Literal_PrimitiveType,
} LiteralType;

typedef struct
{
	int lineNumber;
	LiteralType type;

	union
	{
		char* floatValue;
		uint64_t intValue;
		char* string;
		char* multiChar;
		bool boolean;
		PrimitiveType primitiveType;
	};
} LiteralExpr;

typedef struct
{
	int lineNumber;
	NodePtr baseExpr;
	Array arguments;
} FuncCallExpr;

typedef enum
{
	TypeModifier_None,
	TypeModifier_Pointer,
	TypeModifier_Array,
} TypeModifier;

typedef struct
{
	NodePtr expr;
	TypeModifier modifier;
} Type;

typedef struct
{
	int lineNumber;
	NodePtr expr;
	Type type;
} SizeOfExpr;

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
	bool publicSpecified;
	bool publicValue;
	bool externalSpecified;
	bool externalValue;
} ModifierState;

typedef struct
{
	int lineNumber;
	char* path;
	char* moduleName;
	ModifierState modifiers;
} ImportStmt;

typedef enum
{
	Section_Init,
	Section_Slider,
	Section_Block,
	Section_Sample,
	Section_Serialize,
	Section_GFX,
} SectionType;

typedef struct
{
	SectionType sectionType;
	int lineNumber;
	NodePtr block;
} SectionStmt;

typedef struct
{
	int lineNumber;
	Type type;
	char* name;
	char* externalName;
	NodePtr initializer;
	Array instantiatedVariables;
	ModifierState modifiers;
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
	bool variadic;
	ModifierState modifiers;
	int uniqueName;
} FuncDeclStmt;

typedef struct
{
	int lineNumber;
	char* name;
	Array members;
	ModifierState modifiers;
	bool isArrayType;
} StructDeclStmt;

typedef struct
{
	int lineNumber;
	NodePtr start;
	Array identifiers;
	FuncDeclStmt* funcReference;
	StructDeclStmt* typeReference;
	VarDeclStmt* varReference;
	VarDeclStmt* parentReference;
} MemberAccessExpr;

typedef struct
{
	int lineNumber;
	NodePtr baseExpr;
	NodePtr indexExpr;
	Type typeBeforeCollapse;
} SubscriptExpr;

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

typedef enum
{
	LoopControl_Break,
	LoopControl_Continue,
} LoopControlType;

typedef struct
{
	int lineNumber;
	LoopControlType type;
} LoopControlStmt;

typedef struct
{
	int lineNumber;
	ModifierState modifierState;
} ModifierStmt;

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
