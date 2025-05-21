#pragma once

#include "Common.h"

typedef enum
{
	// keywords
	Token_Any,
	Token_Float,
	Token_Int,
	Token_String,
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
	long lineNumber;
	char* text;
} Token;

static const char* GetTokenTypeString(const TokenType tokenType)
{
	switch (tokenType)
	{
	// keywords
	case Token_Any: return "any";
	case Token_Float: return "float";
	case Token_Int: return "int";
	case Token_String: return "string";
	case Token_Char: return "char";
	case Token_Bool: return "bool";
	case Token_Void: return "void";

	case Token_Struct: return "struct";
	case Token_Import: return "import";
	case Token_Public: return "public";
	case Token_Private: return "private";
	case Token_External: return "external";
	case Token_Internal: return "internal";
	case Token_As: return "as";

	case Token_If: return "if";
	case Token_Else: return "else";
	case Token_While: return "while";
	case Token_For: return "for";
	case Token_Switch: return "switch";

	case Token_Return: return "return";
	case Token_Break: return "break";
	case Token_Continue: return "continue";

	case Token_SizeOf: return "sizeof";

	// literals
	case Token_NumberLiteral: return "NumberLiteral";
	case Token_StringLiteral: return "StringLiteral";
	case Token_Identifier: return "Identifier";
	case Token_True: return "true";
	case Token_False: return "false";

	// 3 characters
	case Token_Ellipsis: return "...";

	// 2 characters
	case Token_PlusEquals: return "+=";
	case Token_MinusEquals: return "-=";
	case Token_AsteriskEquals: return "*=";
	case Token_SlashEquals: return "/=";
	case Token_AmpersandEquals: return "&=";
	case Token_PipeEquals: return "|=";
	case Token_PercentEquals: return "%=";
	case Token_TildeEquals: return "~=";
	case Token_CaretEquals: return "^=";

	case Token_LeftAngleEquals: return "<=";
	case Token_RightAngleEquals: return ">=";
	case Token_EqualsEquals: return "==";
	case Token_ExclamationEquals: return "!=";
	case Token_AmpersandAmpersand: return "&&";
	case Token_PipePipe: return "||";
	case Token_PlusPlus: return "++";
	case Token_MinusMinus: return "--";

	case Token_LeftAngleLeftAngle: return "<<";
	case Token_RightAngleRightAngle: return ">>";

	case Token_MinusRightAngle: return "->";

	// 1 character
	case Token_LeftBracket: return "(";
	case Token_RightBracket: return ")";
	case Token_LeftCurlyBracket: return "{";
	case Token_RightCurlyBracket: return "}";
	case Token_LeftSquareBracket: return "[";
	case Token_RightSquareBracket: return "]";
	case Token_LeftAngleBracket: return "<";
	case Token_RightAngleBracket: return ">";
	case Token_Dot: return ".";
	case Token_Comma: return ",";
	case Token_Colon: return ":";
	case Token_Semicolon: return ";";
	case Token_Plus: return "+";
	case Token_Minus: return "-";
	case Token_Asterisk: return "*";
	case Token_Slash: return "/";
	case Token_Exclamation: return "!";
	case Token_Equals: return "=";
	case Token_Ampersand: return "&";
	case Token_Pipe: return "|";
	case Token_At: return "@";
	case Token_Tilde: return "~";
	case Token_Percent: return "%";
	case Token_Caret: return "^";

	case Token_EndOfFile: return "EndOfFile";

	default: INVALID_VALUE(tokenType);
	}
}
