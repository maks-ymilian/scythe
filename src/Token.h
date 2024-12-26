#pragma once

#include <stddef.h>

typedef enum
{
	Token_Null,

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
	Token_Void,
	Token_Int,
	Token_Float,
	Token_String,
	Token_Bool,

	Token_Struct,
	Token_Import,
	Token_Public,
	Token_External,

	Token_If,
	Token_Else,
	Token_While,
	Token_For,
	Token_Switch,

	Token_Return,
	Token_Break,
	Token_Continue,

	// end of file
	Token_EndOfFile,
} TokenType;

typedef struct
{
	TokenType type;
	long lineNumber;
	char* text;
} Token;

static inline const char* GetTokenTypeString(const TokenType tokenType)
{
	switch (tokenType)
	{
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

	case Token_EqualsEquals: return "==";
	case Token_ExclamationEquals: return "!=";
	case Token_LeftAngleEquals: return "<=";
	case Token_RightAngleEquals: return ">=";
	case Token_AmpersandAmpersand: return "&&";
	case Token_PipePipe: return "||";
	case Token_PlusPlus: return "++";
	case Token_MinusMinus: return "--";
	case Token_PlusEquals: return "+=";
	case Token_MinusEquals: return "-=";
	case Token_AsteriskEquals: return "*=";
	case Token_SlashEquals: return "/=";

	case Token_NumberLiteral: return "NumberLiteral";
	case Token_StringLiteral: return "StringLiteral";
	case Token_Identifier: return "Identifier";
	case Token_True: return "true";
	case Token_False: return "false";

	case Token_Void: return "void";
	case Token_Int: return "int";
	case Token_Float: return "float";
	case Token_String: return "string";
	case Token_Bool: return "bool";

	case Token_Struct: return "struct";
	case Token_Import: return "import";
	case Token_Public: return "public";
	case Token_External: return "external";

	case Token_If: return "if";
	case Token_Else: return "else";
	case Token_While: return "while";
	case Token_For: return "for";
	case Token_Switch: return "switch";

	case Token_Return: return "return";
	case Token_Break: return "break";
	case Token_Continue: return "continue";

	case Token_EndOfFile: return "EndOfFile";

	default: unreachable();
	}
}
