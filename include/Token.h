#pragma once

#include "data-structures/Array.h"

typedef enum
{
    // 1 character
    LeftBracket, RightBracket, LeftCurlyBracket, RightCurlyBracket, LeftSquareBracket, RightSquareBracket,
    Dot, Comma, Semicolon, Plus, Minus, Asterisk, Slash, Exclamation, Equals,
    LeftAngleBracket, RightAngleBracket, Ampersand, Pipe, At,

    // 2 characters
    EqualsEquals, ExclamationEquals, LeftAngleEquals, RightAngleEquals, AmpersandAmpersand, PipePipe,
    PlusPlus, MinusMinus, PlusEquals, MinusEquals, AsteriskEquals, SlashEquals,

    // literals
    NumberLiteral, StringLiteral,
    Identifier, True, False,

    // keywords
    Init, Slider, Block, Sample, Serialize, GFX,
    Void, Int, Float, String, Bool,
    If, Return,

    //end of file
    EndOfFile,
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
