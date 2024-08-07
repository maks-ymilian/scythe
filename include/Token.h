#pragma once

#include "data-structures/Array.h"

typedef enum
{
    LeftBracket, RightBracket, LeftCurlyBracket, RightCurlyBracket, LeftSquareBracket, RightSquareBracket,
    Dot, Comma, Semicolon, Plus, Minus, Asterisk, Slash, Exclamation, Equals,
    LeftAngleBracket, RightAngleBracket, Ampersand, Pipe, At,

    EqualsEquals, ExclamationEquals, LeftAngleEquals, RightAngleEquals, AmpersandAmpersand, PipePipe,
    PlusPlus, MinusMinus, PlusEquals, MinusEquals, AsteriskEquals, SlashEquals,

    NumberLiteral, StringLiteral,

    Identifier,

    Init, Slider, Block, Sample, Serialize, GFX,
    Int,

    EndOfFile,
} TokenType;

typedef struct
{
    TokenType type;
    long lineNumber;
    const char* text;
} Token;

const char* GetTokenTypeString(TokenType tokenType);

Token CopyToken(Token token);

void PrintTokenArray(const Array* tokens);
void FreeTokenArray(const Array* tokens);
void FreeToken(const Token* token);
