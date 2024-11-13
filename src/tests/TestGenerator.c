#include "TestGenerator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "data-structures/MemoryStream.h"
#include "data-structures/Array.h"
#include "Utils.h"

#define WRITE_LITERAL(text) StreamWrite(stream, text, sizeof(text) - 1)
#define WRITE_TEXT(text) StreamWrite(stream, text, strlen(text));

static MemoryStream* stream = NULL;

static int variableNameCounter = 0;
static Array values;

static int Random()
{
    return rand();
}

static void WriteInteger(const int number)
{
    char string[CountCharsInNumber(number) + 1];
    snprintf(string, sizeof(string), "%d", number);
    WRITE_TEXT(string);
}

static void WriteDouble(const double number)
{
    char string[64];
    snprintf(string, sizeof(string), "%f", number);
    WRITE_TEXT(string);
}

static double GetValue(const int name)
{
    if (name >= values.length)
        assert(0);
    return *(double*)values.array[name];
}

static int GetRandomName()
{
    if (variableNameCounter == 0)
        assert(0);
    return Random() % variableNameCounter;
}

static void WriteName(const int name)
{
    WRITE_LITERAL("v");
    WriteInteger(name);
    WRITE_LITERAL(" ");
}

static double GetRandomDouble()
{
    return (Random() - RAND_MAX / 2) + (Random() / (double)RAND_MAX);
}

static double WriteMathExpression();

static double WriteNumberValue()
{
    if (Random() % 2 == 0)
        return WriteMathExpression();

    const bool isVariable = variableNameCounter == 0 ? false : Random() % 2 == 0;
    const int name = isVariable ? GetRandomName() : -1;

    const double value = isVariable ? GetValue(name) : GetRandomDouble();

    if (isVariable)
    {
        WriteName(name);
    }
    else
    {
        WRITE_LITERAL("(");
        WriteDouble(value);
        WRITE_LITERAL(")");
    }

    return value;
}

static double WriteMathExpression()
{
    if (Random() % 2 == 0)
        return WriteNumberValue();

    WRITE_LITERAL("(");
    const double value1 = WriteNumberValue();

    const int operator = Random() % 4;
    switch (operator)
    {
    case 0: WRITE_LITERAL("+");
        break;
    case 1: WRITE_LITERAL("-");
        break;
    case 2: WRITE_LITERAL("*");
        break;
    case 3: WRITE_LITERAL("/");
        break;
    default: assert(0);
    }

    const double value2 = WriteNumberValue();
    WRITE_LITERAL(")");

    switch (operator)
    {
    case 0: return value1 + value2;
    case 1: return value1 - value2;
    case 2: return value1 * value2;
    case 3: return value1 / value2;
    default: assert(0);
    }
}

static void WriteVariableDeclaration()
{
    WRITE_LITERAL("float ");

    WriteName(variableNameCounter);

    WRITE_LITERAL("=");
    const double value = WriteMathExpression();

    ArrayAdd(&values, &value);
    variableNameCounter++;

    WRITE_LITERAL(";\n");
}

static double WriteTest()
{
    WRITE_LITERAL("init{");

    for (int i = 0; i < 100; ++i)
    {
        WriteVariableDeclaration();
    }

    WRITE_LITERAL("}");

    return GetValue(variableNameCounter - 1);
}

char* GenerateTestSource()
{
    srand(time(NULL));

    values = AllocateArray(sizeof(double));

    stream = AllocateMemoryStream();

    const double result = WriteTest();
    printf("Result: %f\n", result);
    WRITE_LITERAL("\0");

    FreeArray(&values);

    const Buffer buffer = StreamGetBuffer(stream);
    FreeMemoryStream(stream, false);
    return (char*)buffer.buffer;
}
