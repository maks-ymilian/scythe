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
        AddNewToken(Identifier, lexeme);
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

    AddNewToken(StringLiteral, AllocateSubstring(start, end));
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

    AddNewToken(NumberLiteral, AllocateCurrentLexeme());
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
    ADD_KEYWORD(Init);
    ADD_KEYWORD(Sample);
    ADD_KEYWORD(Block);
    ADD_KEYWORD(Serialize);
    ADD_KEYWORD(GFX);
    ADD_KEYWORD(Slider);

    ADD_KEYWORD(Void);
    ADD_KEYWORD(Int);
    ADD_KEYWORD(Float);
    ADD_KEYWORD(String);
    ADD_KEYWORD(Bool);

    ADD_KEYWORD(If);
    ADD_KEYWORD(Else);
    ADD_KEYWORD(Struct);
    ADD_KEYWORD(True);
    ADD_KEYWORD(False);
    ADD_KEYWORD(Return);
    ADD_KEYWORD(Import);
    ADD_KEYWORD(Public);

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
            case '(': AddNewToken(LeftBracket, NULL);
                continue;
            case ')': AddNewToken(RightBracket, NULL);
                continue;
            case '{': AddNewToken(LeftCurlyBracket, NULL);
                continue;
            case '}': AddNewToken(RightCurlyBracket, NULL);
                continue;
            case '[': AddNewToken(LeftSquareBracket, NULL);
                continue;
            case ']': AddNewToken(RightSquareBracket, NULL);
                continue;
            case '.': AddNewToken(Dot, NULL);
                continue;
            case ',': AddNewToken(Comma, NULL);
                continue;
            case ';': AddNewToken(Semicolon, NULL);
                continue;
            case '@': AddNewToken(At, NULL);
                continue;

            case '=': AddDoubleSymbolToken(Equals, (SecondSymbol[]){{'=', EqualsEquals}}, 1);
                continue;
            case '!': AddDoubleSymbolToken(Exclamation, (SecondSymbol[]){{'=', ExclamationEquals}}, 1);
                continue;
            case '<': AddDoubleSymbolToken(LeftAngleBracket, (SecondSymbol[]){{'=', LeftAngleEquals}}, 1);
                continue;
            case '>': AddDoubleSymbolToken(RightAngleBracket, (SecondSymbol[]){{'=', RightAngleEquals}}, 1);
                continue;
            case '+': AddDoubleSymbolToken(Plus, (SecondSymbol[]){{'+', PlusPlus}, {'=', PlusEquals}}, 2);
                continue;
            case '-': AddDoubleSymbolToken(Minus, (SecondSymbol[]){{'-', MinusMinus}, {'=', MinusEquals}}, 2);
                continue;
            case '*': AddDoubleSymbolToken(Asterisk, (SecondSymbol[]){{'=', AsteriskEquals}}, 1);
                continue;
            case '/': AddDoubleSymbolToken(Slash, (SecondSymbol[]){{'=', SlashEquals}}, 1);
                continue;
            case '&': AddDoubleSymbolToken(Ampersand, (SecondSymbol[]){{'&', AmpersandAmpersand}}, 1);
                continue;
            case '|': AddDoubleSymbolToken(Pipe, (SecondSymbol[]){{'|', PipePipe}}, 1);
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
    AddNewToken(EndOfFile, NULL);

    *outTokens = tokens;

    FreeMap(&keywords);

    return SUCCESS_RESULT;
}
