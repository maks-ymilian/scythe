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
    Token identifier;
    NodePtr subscript;
} ArrayAccessExpr;

ArrayAccessExpr* AllocArrayAccessExpr(ArrayAccessExpr expr);


typedef struct
{
    Token token;
} LiteralExpr;

LiteralExpr* AllocLiteral(LiteralExpr expr);


typedef struct
{
    NodePtr value;
    NodePtr next;
} MemberAccessExpr;
MemberAccessExpr* AllocMemberAccess(MemberAccessExpr expr);
// LiteralExpr* DeepCopyLiteral(const LiteralExpr* expr);
// void FreeLiteral(LiteralExpr* expr);


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
    NodePtr arrayLength;
    Token externalIdentifier;
    bool array;
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
    Token externalIdentifier;
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
    Token whileToken;
    NodePtr expr;
    NodePtr stmt;
} WhileStmt;

WhileStmt* AllocWhileStmt(WhileStmt stmt);

typedef struct
{
    Token forToken;
    NodePtr initialization;
    NodePtr condition;
    NodePtr increment;
    NodePtr stmt;
} ForStmt;

ForStmt* AllocForStmt(ForStmt stmt);


typedef struct
{
    enum
    {
        LoopControl_Break,
        LoopControl_Continue,
    } type;

    int lineNumber;
} LoopControlStmt;

LoopControlStmt* AllocLoopControlStmt(LoopControlStmt stmt);


typedef struct
{
    Array statements;
} AST;


void FreeSyntaxTree(AST root);
