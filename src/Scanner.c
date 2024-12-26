#include "Scanner.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "data-structures/Map.h"

static const char* source;
static size_t pointer;
static size_t currentTokenStart;
static int currentLine;

static Array tokens;

static Map keywords;

static void AddKeyword(const TokenType tokenType)
{
	MapAdd(&keywords, GetTokenTypeString(tokenType), &tokenType);
}

static char* AllocateSubstring(const size_t start, const size_t end)
{
	size_t length = end - start;
	length += 1;
	char* text = malloc(length);
	assert(text != NULL);
	memcpy(text, source + start, length - 1);
	text[length - 1] = '\0';
	return text;
}

static void AddToken(const TokenType type, char* text)
{
	const Token token = (Token){
		.type = type,
		.lineNumber = currentLine,
		.text = text,
	};
	ArrayAdd(&tokens, &token);
}

static void ScanWord()
{
	for (; source[pointer] != '\0'; pointer++)
	{
		if (!isalnum(source[pointer]) && source[pointer] != '_')
			break;
	}

	char* string = AllocateSubstring(currentTokenStart, pointer);
	const TokenType* tokenType = MapGet(&keywords, string);
	if (tokenType != NULL)
		AddToken(*tokenType, NULL);
	else
		AddToken(Token_Identifier, string);
}

static Result ScanStringLiteral()
{
	const size_t start = pointer;
	pointer++;
	for (; source[pointer] != '\0'; pointer++)
	{
		if (source[pointer] == '"')
			break;
	}
	if (source[pointer] == '\0')
		return ERROR_RESULT("String literal is never closed", currentLine, NULL);

	AddToken(Token_StringLiteral, AllocateSubstring(start + 1, pointer));
	pointer++;
	return SUCCESS_RESULT;
}

static void ScanNumberLiteral()
{
	for (; source[pointer] != '\0'; pointer++)
	{
		if (!isalnum(source[pointer]) && source[pointer] != '.')
			break;
	}

	AddToken(Token_NumberLiteral, AllocateSubstring(currentTokenStart, pointer));
}

typedef struct
{
	char symbol;
	TokenType joinedTokenType;
} SecondSymbol;

static void AddDoubleSymbolToken(const TokenType defaultType, const SecondSymbol* symbols, const size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		if (source[pointer + 1] == symbols[i].symbol)
		{
			pointer++;
			AddToken(symbols[i].joinedTokenType, NULL);
			return;
		}
	}

	AddToken(defaultType, NULL);
}

Result Scan(const char* const sourceCode, Array* outTokens)
{
	keywords = AllocateMap(sizeof(TokenType));

	AddKeyword(Token_True);
	AddKeyword(Token_False);

	AddKeyword(Token_Void);
	AddKeyword(Token_Int);
	AddKeyword(Token_Float);
	AddKeyword(Token_String);
	AddKeyword(Token_Bool);

	AddKeyword(Token_Struct);
	AddKeyword(Token_Import);
	AddKeyword(Token_Public);
	AddKeyword(Token_External);

	AddKeyword(Token_If);
	AddKeyword(Token_Else);
	AddKeyword(Token_While);
	AddKeyword(Token_For);
	AddKeyword(Token_Switch);

	AddKeyword(Token_Return);
	AddKeyword(Token_Break);
	AddKeyword(Token_Continue);

	tokens = AllocateArray(sizeof(Token));

	source = sourceCode;
	currentLine = 1;

	bool insideLineComment = false;
	bool insideMultilineComment = false;

	for (pointer = 0; source[pointer] != '\0'; pointer++)
	{
		const char character = source[pointer];

		if (character == '\n')
		{
			currentLine++;
			insideLineComment = false;
			continue;
		}

		if (character == ' ' ||
			character == '\t' ||
			character == '\r')
			continue;

		currentTokenStart = pointer;

		if (character == '/')
		{
			const char peek = source[pointer + 1];
			if (peek == '/' && !insideMultilineComment)
			{
				insideLineComment = true;
				continue;
			}
			else if (peek == '*')
			{
				insideMultilineComment = true;
				continue;
			}
		}

		if (character == '*' && source[pointer + 1] == '/' && insideMultilineComment)
		{
			insideMultilineComment = false;
			pointer++;
			continue;
		}

		if (insideLineComment || insideMultilineComment)
			continue;

		switch (character)
		{
		case '(':
			AddToken(Token_LeftBracket, NULL);
			continue;
		case ')':
			AddToken(Token_RightBracket, NULL);
			continue;
		case '{':
			AddToken(Token_LeftCurlyBracket, NULL);
			continue;
		case '}':
			AddToken(Token_RightCurlyBracket, NULL);
			continue;
		case '[':
			AddToken(Token_LeftSquareBracket, NULL);
			continue;
		case ']':
			AddToken(Token_RightSquareBracket, NULL);
			continue;
		case '.':
			AddToken(Token_Dot, NULL);
			continue;
		case ',':
			AddToken(Token_Comma, NULL);
			continue;
		case ';':
			AddToken(Token_Semicolon, NULL);
			continue;
		case '@':
			AddToken(Token_At, NULL);
			continue;

		case '=':
			AddDoubleSymbolToken(Token_Equals, (SecondSymbol[]){{'=', Token_EqualsEquals}}, 1);
			continue;
		case '!':
			AddDoubleSymbolToken(Token_Exclamation, (SecondSymbol[]){{'=', Token_ExclamationEquals}}, 1);
			continue;
		case '<':
			AddDoubleSymbolToken(Token_LeftAngleBracket, (SecondSymbol[]){{'=', Token_LeftAngleEquals}}, 1);
			continue;
		case '>':
			AddDoubleSymbolToken(Token_RightAngleBracket, (SecondSymbol[]){{'=', Token_RightAngleEquals}}, 1);
			continue;
		case '+':
			AddDoubleSymbolToken(Token_Plus, (SecondSymbol[]){{'+', Token_PlusPlus}, {'=', Token_PlusEquals}}, 2);
			continue;
		case '-':
			AddDoubleSymbolToken(Token_Minus, (SecondSymbol[]){{'-', Token_MinusMinus}, {'=', Token_MinusEquals}}, 2);
			continue;
		case '*':
			AddDoubleSymbolToken(Token_Asterisk, (SecondSymbol[]){{'=', Token_AsteriskEquals}}, 1);
			continue;
		case '/':
			AddDoubleSymbolToken(Token_Slash, (SecondSymbol[]){{'=', Token_SlashEquals}}, 1);
			continue;
		case '&':
			AddDoubleSymbolToken(Token_Ampersand, (SecondSymbol[]){{'&', Token_AmpersandAmpersand}}, 1);
			continue;
		case '|':
			AddDoubleSymbolToken(Token_Pipe, (SecondSymbol[]){{'|', Token_PipePipe}}, 1);
			continue;

		default: break;
		}

		if (isdigit(character)) // number literal
			ScanNumberLiteral();
		else if (isalpha(character) || character == '_') // identifier / keyword
			ScanWord();
		else if (character == '"') // string literal
			PROPAGATE_ERROR(ScanStringLiteral());
		else // error
			return ERROR_RESULT("Unexpected character", currentLine, NULL);

		pointer--;
	}
	AddToken(Token_EndOfFile, NULL);

	*outTokens = tokens;

	FreeMap(&keywords);

	return SUCCESS_RESULT;
}

void FreeTokenArray(const Array* tokens)
{
	for (size_t i = 0; i < tokens->length; ++i)
	{
		const Token* token = tokens->array[i];
		if (token->text != NULL)
			free(token->text);
	}
	FreeArray(tokens);
}
