#pragma once

#include "Token.h"
#include "data-structures/Array.h"

typedef enum
{
    NoExpression,
    BinaryExpression,
    UnaryExpression,
    LiteralExpression,
} ExprType;

typedef struct
{
    void* ptr;
    ExprType type;
} ExprPtr;


typedef struct
{
    ExprPtr left;
    Token operator;
    ExprPtr right;
} BinaryExpr;

BinaryExpr* AllocBinary(BinaryExpr expr);


typedef struct
{
    Token operator;
    ExprPtr expression;
} UnaryExpr;

UnaryExpr* AllocUnary(UnaryExpr expr);


typedef struct
{
    Token value;
} LiteralExpr;

LiteralExpr* AllocLiteral(LiteralExpr expr);


typedef enum
{
    Section,
    ExpressionStatement,
    VariableDeclaration,
    ProgramRoot,
} StmtType;

typedef struct
{
    void* ptr;
    StmtType type;
} StmtPtr;


typedef struct
{
    Token type;
    Array statements;
} SectionStmt;

SectionStmt* AllocSectionStmt(SectionStmt stmt);


typedef struct
{
    ExprPtr expr;
} ExpressionStmt;

ExpressionStmt* AllocExpressionStmt(ExpressionStmt stmt);


typedef struct
{
    Token type;
    Token identifier;
    ExprPtr initializer;
} VarDeclStmt;

VarDeclStmt* AllocVarDeclStmt(VarDeclStmt stmt);


typedef struct
{
    Array statements;
}Program;

Program* AllocProgram(Program program);


void FreeSyntaxTree(StmtPtr root);