#pragma once

#include "Token.h"
#include "data-structures/Array.h"

typedef enum
{
    NullNode,
    BinaryExpression,
    UnaryExpression,
    LiteralExpression,
    FunctionCallExpression,

    Section,
    BlockStatement,
    ExpressionStatement,
    ReturnStatement,
    VariableDeclaration,
    FunctionDeclaration,

    RootNode,
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


typedef struct
{
    Token value;
} LiteralExpr;

LiteralExpr* AllocLiteral(LiteralExpr expr);


typedef struct
{
    Array statements;
} BlockStmt;

BlockStmt* AllocBlockStmt(BlockStmt stmt);


typedef struct
{
    Token type;
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
    NodePtr expr;
    Token returnToken;
} ReturnStmt;

ReturnStmt* AllocReturnStmt(ReturnStmt stmt);


typedef struct
{
    Token type;
    Token identifier;
    NodePtr initializer;
} VarDeclStmt;

VarDeclStmt* AllocVarDeclStmt(VarDeclStmt stmt);


typedef struct
{
    Token type;
    Token identifier;
    Array parameters;
    NodePtr block;
} FuncDeclStmt;

FuncDeclStmt* AllocFuncDeclStmt(FuncDeclStmt stmt);


typedef struct
{
    Array statements;
} Program;

Program* AllocProgram(Program program);


void FreeSyntaxTree(NodePtr root);
