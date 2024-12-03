#pragma once

#include <stdbool.h>

#include "Token.h"
#include "data-structures/Array.h"

#define NULL_NODE (NodePtr){.ptr = NULL, .type = Node_Null}

typedef enum
{
    Node_Null,
    Node_Binary,
    Node_Unary,
    Node_Literal,
    Node_FunctionCall,
    Node_Import,
    Node_Section,
    Node_Block,
    Node_ExpressionStatement,
    Node_Return,
    Node_If,
    Node_VariableDeclaration,
    Node_FunctionDeclaration,
    Node_StructDeclaration,
} NodeType;

typedef struct
{
    void* ptr;
    NodeType type;
} NodePtr;


typedef struct
{
    NodePtr left;
    Token operator;
    NodePtr right;
} BinaryExpr;

BinaryExpr* AllocBinary(BinaryExpr expr);


typedef struct
{
    Token operator;
    NodePtr expression;
} UnaryExpr;

UnaryExpr* AllocUnary(UnaryExpr expr);


typedef struct
{
    Token identifier;
    Array parameters;
} FuncCallExpr;

FuncCallExpr* AllocFuncCall(FuncCallExpr expr);


typedef struct LiteralExpr LiteralExpr;

typedef struct LiteralExpr
{
    Token value;
    NodePtr next;
} LiteralExpr;

LiteralExpr* AllocLiteral(LiteralExpr expr);


typedef struct
{
    Array statements;
} BlockStmt;

BlockStmt* AllocBlockStmt(BlockStmt stmt);


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
    Token identifier;
    NodePtr block;
} SectionStmt;

SectionStmt* AllocSectionStmt(SectionStmt stmt);


typedef struct
{
    NodePtr expr;
} ExpressionStmt;

ExpressionStmt* AllocExpressionStmt(ExpressionStmt stmt);


typedef struct
{
    Token ifToken;
    NodePtr expr;
    NodePtr trueStmt;
    NodePtr falseStmt;
} IfStmt;

IfStmt* AllocIfStmt(IfStmt stmt);


typedef struct
{
    NodePtr expr;
    Token returnToken;
} ReturnStmt;

ReturnStmt* AllocReturnStmt(ReturnStmt stmt);


typedef struct
{
    Token type;
    Token identifier;
    NodePtr initializer;
    Token externalIdentifier;
    bool public;
    bool external;
} VarDeclStmt;

VarDeclStmt* AllocVarDeclStmt(VarDeclStmt stmt);


typedef struct
{
    Token type;
    Token identifier;
    Array parameters;
    NodePtr block;
    bool public;
    bool external;
} FuncDeclStmt;

FuncDeclStmt* AllocFuncDeclStmt(FuncDeclStmt stmt);


typedef struct
{
    Token identifier;
    Array members;
    bool public;
} StructDeclStmt;

StructDeclStmt* AllocStructDeclStmt(StructDeclStmt stmt);


typedef struct
{
    Token import;
    char* file;
    bool public;
} ImportStmt;

ImportStmt* AllocImportStmt(ImportStmt stmt);

typedef struct
{
    Array statements;
} AST;


void FreeSyntaxTree(AST root);
