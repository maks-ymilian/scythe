#include "Scanner.h"

#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "data-structures/Map.h"

#define HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

#define ADD_KEYWORD(tokenType) {const TokenType t = tokenType; MapAdd(&keywords, GetTokenTypeString(tokenType), &t);};

static const char* source;
static long pointer;
static long currentTokenStart;
static int currentLine;

static Array tokens;

static Map keywords;

static char Advance()
{
    pointer++;
    return source[pointer];
}

static char Peek()
{
    return source[pointer + 1];
}

static bool IsAtEnd()
{
    return source[pointer + 1] == 0;
}

static const char* AllocateSubstring(const long start, const long end)
{
    int length = (end + 1) - start;
    length += 1;
    char* text = malloc(length);
    assert(text != NULL);
    memcpy(text, source + start, length - 1);
    text[length - 1] = '\0';
    return text;
}

static const char* AllocateCurrentLexeme()
{
    return AllocateSubstring(currentTokenStart, pointer);
}

static void AddNewToken(const TokenType type, const char* text)
{
    const Token token = (Token){type, currentLine, (char*)text};
    ArrayAdd(&tokens, &token);
}

static void ScanWord()
{
    pointer--;

    while (!IsAtEnd())
    {
        const char character = Advance();

        if (!isalnum(character) && character != '_')
        {
            pointer--;
            break;
        }
    }

    const char* lexeme = AllocateCurrentLexeme();
    const TokenType* tokenType = MapGet(&keywords, lexeme);
    if (tokenType != NULL)
        AddNewToken(*tokenType, NULL);
    else
        AddNewToken(Token_Identifier, lexeme);
}

static Result ScanStringLiteral()
{
    const long start = pointer + 1;
    bool foundClosingCharacter = false;
    while (!IsAtEnd())
    {
        const char character = Advance();

        if (character == '"')
        {
            foundClosingCharacter = true;
            break;
        }
    }
    const long end = pointer - 1;

    if (!foundClosingCharacter)
        return ERROR_RESULT("String literal is never closed", currentLine);

    AddNewToken(Token_StringLiteral, AllocateSubstring(start, end));
    return SUCCESS_RESULT;
}

static void ScanNumberLiteral()
{
    pointer--;

    while (!IsAtEnd())
    {
        const char character = Advance();

        if (!isalnum(character) && character != '.')
        {
            pointer--;
            break;
        }
    }

    AddNewToken(Token_NumberLiteral, AllocateCurrentLexeme());
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
        if (Peek() == symbols[i].symbol)
        {
            Advance();
            AddNewToken(symbols[i].joinedTokenType, NULL);
            return;
        }
    }

    AddNewToken(defaultType, NULL);
}

Result Scan(const char* const sourceCode, Array* outTokens)
{
    keywords = AllocateMap(sizeof(TokenType));

    ADD_KEYWORD(Token_True);
    ADD_KEYWORD(Token_False);

    ADD_KEYWORD(Token_Void);
    ADD_KEYWORD(Token_Int);
    ADD_KEYWORD(Token_Float);
    ADD_KEYWORD(Token_String);
    ADD_KEYWORD(Token_Bool);

    ADD_KEYWORD(Token_Struct);
    ADD_KEYWORD(Token_Import);
    ADD_KEYWORD(Token_Public);
    ADD_KEYWORD(Token_External);

    ADD_KEYWORD(Token_If);
    ADD_KEYWORD(Token_Else);
    ADD_KEYWORD(Token_While);
    ADD_KEYWORD(Token_For);
    ADD_KEYWORD(Token_Switch);

    ADD_KEYWORD(Token_Return);
    ADD_KEYWORD(Token_Break);
    ADD_KEYWORD(Token_Continue);

    tokens = AllocateArray(sizeof(Token));

    source = sourceCode;
    pointer = -1;
    currentLine = 1;

    bool insideLineComment = false;
    bool insideMultilineComment = false;

    while (!IsAtEnd())
    {
        const char character = Advance();

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
            const char peek = Peek();
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

        if (character == '*' && Peek() == '/' && insideMultilineComment)
        {
            insideMultilineComment = false;
            Advance();
            continue;
        }

        if (insideLineComment || insideMultilineComment)
            continue;

        switch (character)
        {
            case '(': AddNewToken(Token_LeftBracket, NULL);
                continue;
            case ')': AddNewToken(Token_RightBracket, NULL);
                continue;
            case '{': AddNewToken(Token_LeftCurlyBracket, NULL);
                continue;
            case '}': AddNewToken(Token_RightCurlyBracket, NULL);
                continue;
            case '[': AddNewToken(Token_LeftSquareBracket, NULL);
                continue;
            case ']': AddNewToken(Token_RightSquareBracket, NULL);
                continue;
            case '.': AddNewToken(Token_Dot, NULL);
                continue;
            case ',': AddNewToken(Token_Comma, NULL);
                continue;
            case ';': AddNewToken(Token_Semicolon, NULL);
                continue;
            case '@': AddNewToken(Token_At, NULL);
                continue;

            case '=': AddDoubleSymbolToken(Token_Equals, (SecondSymbol[]){{'=', Token_EqualsEquals}}, 1);
                continue;
            case '!': AddDoubleSymbolToken(Token_Exclamation, (SecondSymbol[]){{'=', Token_ExclamationEquals}}, 1);
                continue;
            case '<': AddDoubleSymbolToken(Token_LeftAngleBracket, (SecondSymbol[]){{'=', Token_LeftAngleEquals}}, 1);
                continue;
            case '>': AddDoubleSymbolToken(Token_RightAngleBracket, (SecondSymbol[]){{'=', Token_RightAngleEquals}}, 1);
                continue;
            case '+': AddDoubleSymbolToken(Token_Plus, (SecondSymbol[]){{'+', Token_PlusPlus}, {'=', Token_PlusEquals}}, 2);
                continue;
            case '-': AddDoubleSymbolToken(Token_Minus, (SecondSymbol[]){{'-', Token_MinusMinus}, {'=', Token_MinusEquals}}, 2);
                continue;
            case '*': AddDoubleSymbolToken(Token_Asterisk, (SecondSymbol[]){{'=', Token_AsteriskEquals}}, 1);
                continue;
            case '/': AddDoubleSymbolToken(Token_Slash, (SecondSymbol[]){{'=', Token_SlashEquals}}, 1);
                continue;
            case '&': AddDoubleSymbolToken(Token_Ampersand, (SecondSymbol[]){{'&', Token_AmpersandAmpersand}}, 1);
                continue;
            case '|': AddDoubleSymbolToken(Token_Pipe, (SecondSymbol[]){{'|', Token_PipePipe}}, 1);
                continue;

            default: break;
        }

        if (isdigit(character)) // number literal
            ScanNumberLiteral();
        else if (isalpha(character) || character == '_') // identifier / keyword
            ScanWord();
        else if (character == '"') // string literal
            HANDLE_ERROR(ScanStringLiteral())
        else // error
            return ERROR_RESULT("Unexpected character", currentLine);
    }
    AddNewToken(Token_EndOfFile, NULL);

    *outTokens = tokens;

    FreeMap(&keywords);

    return SUCCESS_RESULT;
}
