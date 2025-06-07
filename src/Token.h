#pragma once

#include "Common.h"

typedef enum
{
	// keywords
	Token_Any,
	Token_Float,
	Token_Int,
	Token_String,
	Token_CharLiteral,
	Token_Char,
	Token_Bool,
	Token_Void,

	Token_Struct,
	Token_Import,
	Token_Public,
	Token_Private,
	Token_External,
	Token_Internal,
	Token_As,

	Token_If,
	Token_Else,
	Token_While,
	Token_For,
	Token_Switch,

	Token_Return,
	Token_Break,
	Token_Continue,

	Token_SizeOf,

	Token_Input,

	// literals
	Token_NumberLiteral,
	Token_StringLiteral,
	Token_Identifier,
	Token_True,
	Token_False,

	// 3 characters
	Token_Ellipsis,

	// 2 characters
	Token_PlusEquals,
	Token_MinusEquals,
	Token_AsteriskEquals,
	Token_SlashEquals,
	Token_AmpersandEquals,
	Token_PipeEquals,
	Token_PercentEquals,
	Token_TildeEquals,
	Token_CaretEquals,

	Token_LeftAngleEquals,
	Token_RightAngleEquals,
	Token_EqualsEquals,
	Token_ExclamationEquals,
	Token_AmpersandAmpersand,
	Token_PipePipe,
	Token_PlusPlus,
	Token_MinusMinus,

	Token_LeftAngleLeftAngle,
	Token_RightAngleRightAngle,

	Token_MinusRightAngle,

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
	Token_Colon,
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
	Token_Tilde,
	Token_Percent,
	Token_Caret,

	Token_EndOfFile,

	TokenType_Max,
} TokenType;

typedef struct
{
	TokenType type;
	const char* text;
	size_t textSize;
	int lineNumber;
} Token;

const char* GetTokenTypeString(TokenType tokenType);
