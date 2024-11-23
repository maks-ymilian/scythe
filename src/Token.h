#pragma once

#include "data-structures/Array.h"

typedef enum
{
    // 1 character
    Token_LeftBracket,
    Token_RightBracket,
    Token_LeftCurlyBracket,
    Token_RightCurlyBracket,
    Token_LeftSquareBracket,
    Token_RightSquareBracket,
    Token_LeftAngleBracket,
    Token_RightAngleBracket,
    Token_Dot,
    Token_Comma,
    Token_Semicolon,
    Token_Plus,
    Token_Minus,
    Token_Asterisk,
    Token_Slash,
    Token_Exclamation,
    Token_Equals,
    Token_Ampersand,
    Token_Pipe,
    Token_At,

    // 2 characters
    Token_EqualsEquals,
    Token_ExclamationEquals,
    Token_LeftAngleEquals,
    Token_RightAngleEquals,
    Token_AmpersandAmpersand,
    Token_PipePipe,
    Token_PlusPlus,
    Token_MinusMinus,
    Token_PlusEquals,
    Token_MinusEquals,
    Token_AsteriskEquals,
    Token_SlashEquals,

    // literals
    Token_NumberLiteral,
    Token_StringLiteral,
    Token_Identifier,
    Token_True,
    Token_False,

    // keywords
    Token_Init,
    Token_Slider,
    Token_Block,
    Token_Sample,
    Token_Serialize,
    Token_GFX,
    Token_Void,
    Token_Int,
    Token_Float,
    Token_String,
    Token_Bool,
    Token_If,
    Token_Else,
    Token_Struct,
    Token_Return,
    Token_Import,
    Token_Public,

    //end of file
    Token_EndOfFile,
} TokenType;

typedef struct
{
    TokenType type;
    long lineNumber;
    char* text;
} Token;

const char* GetTokenTypeString(TokenType tokenType);

Token CopyToken(Token token);

void PrintTokenArray(const Array* tokens);
void FreeTokenArray(const Array* tokens);
void FreeToken(const Token* token);
