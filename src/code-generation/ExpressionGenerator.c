#include "ExpressionGenerator.h"

#include <errno.h>

#include "StatementGenerator.h"
#include "StringUtils.h"

static long returnCounter = 0;

static bool IsDigitBase(const char c, const int base)
{
    if (base == 10) return isdigit(c);
    if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (base == 2) return c == '0' || c == '1';
    if (base == 8) return c >= '0' && c <= '7';
    assert(0);
}

static int CountCharacters(const char character, const char* string)
{
    assert(string != NULL);

    int count = 0;
    int i = 0;
    while (string[i] != '\0')
    {
        if (string[i] == character)
            count++;
        i++;
    }
    return count;
}

static Result EvaluateNumberLiteral(const Token token, bool* outInteger, char* out)
{
    assert(token.type == Token_NumberLiteral);
    assert(token.text != NULL);

    const size_t length = strlen(token.text);
    char text[length + 1];
    memcpy(text, token.text, length + 1);

    int base = 10;
    if (length >= 3 && text[0] == '0')
    {
        if (text[1] == 'x')
            base = 16;
        else if (text[1] == 'o')
            base = 8;
        else if (text[1] == 'b')
            base = 2;
    }

    bool integer = true;
    if (base == 10)
    {
        const int dotCount = CountCharacters('.', text);
        assert(dotCount >= 0);
        if (dotCount > 1)
            return ERROR_RESULT("Only one dot is allowed in a number literal", token.lineNumber);

        integer = !dotCount;
    }

    if (!integer && base != 10)
        return ERROR_RESULT("Float literal must be base 10", token.lineNumber);

    const size_t startIndex = base == 10 ? 0 : 2;
    const size_t endIndex = length;

    size_t i = startIndex;
    while (i < endIndex)
    {
        if (text[i] == '.')
        {
            if (integer)
                return ERROR_RESULT("Dots in integer literals are not allowed", token.lineNumber);
        }
        else if (!IsDigitBase(text[i], base))
        {
            const char insert[2] = {text[i], '\0'};
            return ERROR_RESULT(AllocateString1Str("Unexpected digit in number literal \"%s\"", insert), token.lineNumber);
        }

        i++;
    }

    text[endIndex] = '\0';
    if (integer)
    {
        const uint64_t number = strtoull(text + startIndex, NULL, base);
        snprintf(out, length + 1, "%llu", number);
        if (errno == ERANGE)
            return ERROR_RESULT("Invalid integer literal", token.lineNumber);
    }
    else
    {
        float _;
        const int ret = sscanf(text + startIndex, "%f", &_);
        if (ret == 0)
            return ERROR_RESULT("Invalid float literal", token.lineNumber);

        memcpy(out, text + startIndex, endIndex - startIndex + 1);
    }

    *outInteger = integer;

    return SUCCESS_RESULT;
}

static Result GenerateFunctionCallExpression(const FuncCallExpr* in, const char* moduleName, Type* outType);

static Result GenerateLiteralExpression(const LiteralExpr* in, Type* outType)
{
    const Token literal = in->value;
    switch (literal.type)
    {
    case Token_NumberLiteral:
    {
        char number[strlen(literal.text) + 1];
        bool integer;
        HANDLE_ERROR(EvaluateNumberLiteral(literal, &integer, number));
        *outType = integer ? GetKnownType("int") : GetKnownType("float");

        WRITE_TEXT(number);
        return SUCCESS_RESULT;
    }
    case Token_StringLiteral:
    {
        WRITE_LITERAL("\"");
        if (literal.text[0] != '\0')
            WRITE_TEXT(literal.text);
        WRITE_LITERAL("\"");
        *outType = GetKnownType("string");
        return SUCCESS_RESULT;
    }
    case Token_Identifier:
    {
        SymbolData* symbol = NULL;
        if (IsModuleName(in->value.text))
        {
            assert(in->next.type == Node_Literal || in->next.type == Node_FunctionCall);
            if (in->next.type == Node_Literal)
            {
                const LiteralExpr* next = in->next.ptr;
                HANDLE_ERROR(GetSymbol(next->value.text, false, in->value.text, in->value.lineNumber, &symbol));
                in = in->next.ptr;
            }
            else if (in->next.type == Node_FunctionCall)
            {
                HANDLE_ERROR(GenerateFunctionCallExpression(in->next.ptr, in->value.text, outType));
                return SUCCESS_RESULT;
            }
        }
        else
            HANDLE_ERROR(GetSymbol(in->value.text, false, NULL, in->value.lineNumber, &symbol));
        assert(symbol != NULL);

        if (symbol->symbolType != SymbolType_Variable)
            return ERROR_RESULT("Identifier must be the name of a variable", in->value.lineNumber);

        if (symbol->variableData.external)
        {
            if (in->next.ptr != NULL)
                return ERROR_RESULT("Cannot use access operator on external variable", in->value.lineNumber);

            WRITE_TEXT(in->value.text);
            *outType = symbol->variableData.type;
            return SUCCESS_RESULT;
        }

        WRITE_LITERAL(VARIABLE_PREFIX);
        WRITE_TEXT(in->value.text);

        Type type = symbol->variableData.type;
        const LiteralExpr* current = in;
        while (current->next.ptr != NULL)
        {
            assert(current->next.type == Node_Literal);
            const LiteralExpr* next = current->next.ptr;

            WRITE_LITERAL(".");

            if (type.metaType != MetaType_Struct)
                return ERROR_RESULT("Cannot access member of a non-struct type", in->value.lineNumber);

            SymbolData* memberSymbol = MapGet(&symbol->variableData.symbolTable, next->value.text);
            if (memberSymbol == NULL)
                return ERROR_RESULT(
                    AllocateString2Str("Could not find member \"%s\" in struct \"%s\"",
                        next->value.text, type.name),
                    next->value.lineNumber);

            assert(memberSymbol->symbolType == SymbolType_Variable);
            type = memberSymbol->variableData.type;
            symbol = memberSymbol;

            WRITE_TEXT(next->value.text);

            current = next;
        }

        WriteInteger(symbol->uniqueName);

        *outType = type;
        return SUCCESS_RESULT;
    }
    case Token_True:
    {
        WRITE_LITERAL("1");
        *outType = GetKnownType("bool");
        return SUCCESS_RESULT;
    }
    case Token_False:
    {
        WRITE_LITERAL("0");
        *outType = GetKnownType("bool");
        return SUCCESS_RESULT;
    }
    default:
        assert(0);
    }
}

Result CheckAssignmentCompatibility(const Type left, const Type right, const long lineNumber)
{
    if (left.id == right.id)
        return SUCCESS_RESULT;

    const Type intType = GetKnownType("int");
    const Type floatType = GetKnownType("float");
    if ((left.id == intType.id || left.id == floatType.id) &&
        (right.id == intType.id || right.id == floatType.id))
        return SUCCESS_RESULT;

    const char* message = AllocateString2Str(
        "Cannot convert from type \"%s\" to type \"%s\"",
        right.name, left.name);
    return ERROR_RESULT(message, lineNumber);
}

static int Max(const int a, const int b)
{
    if (a > b) return a;
    return b;
};

Result GenerateExpression(const NodePtr* in, Type* outType, bool expectingExpression, bool convertToInteger);

static long CountStructVariables(const Type structType)
{
    const SymbolData* symbol = GetKnownSymbol(structType.name, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData* structData = &symbol->structData;

    long count = 0;
    for (int i = 0; i < structData->members->length; ++i)
    {
        const NodePtr* member = structData->members->array[i];
        if (member->type != Node_VariableDeclaration)
            continue;

        const VarDeclStmt* varDecl = member->ptr;
        Type memberType;
        ASSERT_ERROR(GetTypeFromToken(varDecl->type, &memberType, false));

        if (memberType.metaType == MetaType_Struct)
        {
            count += CountStructVariables(memberType);
            continue;
        }

        count++;
    }
    return count;
}

typedef Result (*FuncParamGenerator)(NodePtr callExpr, Type paramType, bool lastParam, int lineNumber);

static Result GenerateFunctionCallParams(
    const FuncCallExpr* in,
    const FunctionSymbolData function,
    const FuncParamGenerator paramFunc)
{
    const int paramCount = Max(function.parameters.length, in->parameters.length);
    for (int i = 0; i < paramCount; ++i)
    {
        const FunctionParameter* funcParam = NULL;
        if (i < function.parameters.length)
            funcParam = function.parameters.array[i];

        const NodePtr* callExpr = NULL;
        if (i < in->parameters.length)
            callExpr = (NodePtr*)in->parameters.array[i];

        if (funcParam == NULL && callExpr != NULL ||
            funcParam != NULL && funcParam->defaultValue.ptr == NULL && callExpr == NULL)
            return ERROR_RESULT(AllocateString2Int("Function has %d parameter(s), but is called with %d argument(s)",
                                    function.parameters.length, in->parameters.length), in->identifier.lineNumber);

        const NodePtr* writeExpr;
        if (callExpr != NULL) writeExpr = callExpr;
        else writeExpr = &funcParam->defaultValue;

        assert(funcParam != NULL);
        HANDLE_ERROR(paramFunc(*writeExpr, funcParam->type, i == paramCount - 1, in->identifier.lineNumber));
    }

    return SUCCESS_RESULT;
}

static Result GenerateFuncParam(const NodePtr callExpr, const Type paramType, const bool lastParam, const int lineNumber)
{
    Type type;
    HANDLE_ERROR(GenerateExpression(&callExpr, &type, true, paramType.id == GetKnownType("int").id));
    HANDLE_ERROR(CheckAssignmentCompatibility(paramType, type, lineNumber));
    if (!lastParam) WRITE_LITERAL(",");

    return SUCCESS_RESULT;
}

static Result GenerateExternalFunctionCallExpression(const FuncCallExpr* in, const FunctionSymbolData function)
{
    assert(function.returnType.metaType == MetaType_Primitive);

    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("(");
    HANDLE_ERROR(GenerateFunctionCallParams(in, function, GenerateFuncParam));
    WRITE_LITERAL(")");

    return SUCCESS_RESULT;
}

static Result GeneratePushFuncParam(const NodePtr callExpr, const Type paramType, const bool lastParam, const int lineNumber)
{
    HANDLE_ERROR(GeneratePushValue(callExpr, paramType, lineNumber));
    return SUCCESS_RESULT;
}

static Result GenerateFunctionCallExpression(const FuncCallExpr* in, const char* moduleName, Type* outType)
{
    SymbolData* symbol;
    HANDLE_ERROR(GetSymbol(in->identifier.text, false, moduleName, in->identifier.lineNumber, &symbol));
    if (symbol->symbolType != SymbolType_Function)
        return ERROR_RESULT("Identifier must be the name of a function", in->identifier.lineNumber);
    const FunctionSymbolData function = symbol->functionData;

    *outType = function.returnType;

    if (function.external)
        return GenerateExternalFunctionCallExpression(in, function);

    WRITE_LITERAL("(");
    HANDLE_ERROR(GenerateFunctionCallParams(in, function, GeneratePushFuncParam));
    WRITE_LITERAL("func_");
    WRITE_TEXT(in->identifier.text);
    WriteInteger(symbol->uniqueName);
    WRITE_LITERAL("(););");

    if (function.returnType.id != GetKnownType("void").id)
    {
        const long variableCount = function.returnType.metaType == MetaType_Struct ? CountStructVariables(function.returnType) : 1;
        for (int i = 0; i < variableCount; ++i)
        {
            WRITE_LITERAL("__ret");
            returnCounter++;
            WriteInteger(returnCounter);
            WRITE_LITERAL("=stack_pop();");
        }
    }

    return SUCCESS_RESULT;
}

static Result UnaryOperatorErrorResult(const Token operator, const Type type)
{
    return ERROR_RESULT_TOKEN(AllocateString1Str("Cannot use operator \"#t\" on type \"%s\"", type.name),
                              operator.lineNumber, operator.type);
}

static Result GenerateUnaryExpression(const UnaryExpr* in, Type* outType)
{
    WRITE_LITERAL("(");

    WRITE_TEXT(GetTokenTypeString(in->operator.type));

    Type type;
    HANDLE_ERROR(GenerateExpression(&in->expression, &type, true, false));

    switch (in->operator.type)
    {
    case Token_Exclamation:
        if (type.id != GetKnownType("bool").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    case Token_Minus:
        if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    case Token_Plus:
        if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    default:
        assert(0);
    }

    *outType = type;

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

static void GenerateFunctionStructAssignment(const LiteralExpr* left, const StructSymbolData structData, long* returnNumber)
{
    //todo is this the same code
    long _returnNumber = returnCounter;
    if (returnNumber == NULL) returnNumber = &_returnNumber;

    for (int i = 0; i < structData.members->length; i++)
    {
        const NodePtr* node = structData.members->array[i];
        if (node->type != Node_VariableDeclaration)
            continue;
        const VarDeclStmt* varDecl = node->ptr;
        LiteralExpr varDeclIdentifier = (LiteralExpr){varDecl->identifier, NULL};

        LiteralExpr* leftLast = (LiteralExpr*)left;
        while (leftLast->next.ptr != NULL)
        {
            assert(leftLast->next.type == Node_Literal);
            leftLast = leftLast->next.ptr;
        }

        leftLast->next = (NodePtr){.ptr = &varDeclIdentifier, .type = Node_Literal};

        Type memberType;
        ASSERT_ERROR(GetTypeFromToken(varDecl->type, &memberType, false));
        if (memberType.metaType == MetaType_Struct)
        {
            const SymbolData* symbol = GetKnownSymbol(memberType.name, true, NULL);
            assert(symbol->symbolType == SymbolType_Struct);
            const StructSymbolData* structData = &symbol->structData;
            GenerateFunctionStructAssignment(left, *structData, returnNumber);
            leftLast->next = NULL_NODE;
            continue;
        }

        const NodePtr leftNode = (NodePtr){(void*)left, Node_Literal};
        Type _;
        ASSERT_ERROR(GenerateExpression(&leftNode, &_, true, false));

        leftLast->next = NULL_NODE;

        WRITE_LITERAL("=");
        WRITE_LITERAL("__ret");
        WriteInteger(*returnNumber);
        --*returnNumber;
        WRITE_LITERAL(";");
    }
}

static void GenerateLiteralStructAssignment(LiteralExpr* left, LiteralExpr* right, const StructSymbolData structData)
{
    const Token equals = (Token){Token_Equals, 0, NULL};

    for (int i = 0; i < structData.members->length; ++i)
    {
        const NodePtr* node = structData.members->array[i];
        switch (node->type)
        {
        case Node_VariableDeclaration:
        {
            const VarDeclStmt* varDecl = node->ptr;
            LiteralExpr varDeclIdentifier = (LiteralExpr){varDecl->identifier, NULL};

            LiteralExpr* leftLast = left;
            while (leftLast->next.ptr != NULL)
            {
                assert(leftLast->next.type == Node_Literal);
                leftLast = leftLast->next.ptr;
            }
            LiteralExpr* rightLast = right;
            while (rightLast->next.ptr != NULL)
            {
                assert(rightLast->next.type == Node_Literal);
                rightLast = rightLast->next.ptr;
            }

            leftLast->next = (NodePtr){.ptr = &varDeclIdentifier, .type = Node_Literal};
            rightLast->next = (NodePtr){.ptr = &varDeclIdentifier, .type = Node_Literal};

            BinaryExpr expr = (BinaryExpr){(NodePtr){left, Node_Literal}, equals, (NodePtr){right, Node_Literal}};
            NodePtr node = (NodePtr){&expr, Node_Binary};

            Type outType;
            ASSERT_ERROR(GenerateExpression(&node, &outType, true, false));

            leftLast->next = NULL_NODE;
            rightLast->next = NULL_NODE;

            WRITE_LITERAL(";");
            break;
        }
        default: assert(0);
        }
    }
}

static bool LiteralHasFunctionCall(const LiteralExpr* literal)
{
    while (literal->next.ptr != NULL)
    {
        if (literal->next.type == Node_FunctionCall)
            return true;
        literal = literal->next.ptr;
    }

    return false;
}

static void GenerateStructAssignment(const NodePtr left, const NodePtr right, const Type structType)
{
    assert(left.type == Node_Literal);

    const SymbolData* symbol = GetKnownSymbol(structType.name, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData structData = symbol->structData;

    bool isFunctionCall = right.type == Node_FunctionCall;
    if (!isFunctionCall)
    {
        assert(right.type == Node_Literal);
        isFunctionCall = LiteralHasFunctionCall(right.ptr);
    }

    if (isFunctionCall) GenerateFunctionStructAssignment(left.ptr, structData, NULL);
    else GenerateLiteralStructAssignment(left.ptr, right.ptr, structData);
}

static bool IsAssignmentOperator(const TokenType token)
{
    switch (token)
    {
    case Token_Equals:
    case Token_PlusEquals:
    case Token_MinusEquals:
    case Token_SlashEquals:
    case Token_AsteriskEquals:
        return true;
    default: return false;
    }
}

static Result GenerateBinaryExpression(const BinaryExpr* in, Type* outType)
{
    if (IsAssignmentOperator(in->operator.type) &&
        in->right.type == Node_Binary &&
        IsAssignmentOperator(((BinaryExpr*)in->right.ptr)->operator.type))
        return ERROR_RESULT("Chained assignment is not allowed", in->operator.lineNumber);

    WRITE_LITERAL("(");

    const size_t pos = GetStreamPosition();

    BeginRead();
    Type leftType;
    HANDLE_ERROR(GenerateExpression(&in->left, &leftType, true, false));
    const Buffer left = EndRead();

    BeginRead();
    WRITE_TEXT(GetTokenTypeString(in->operator.type));
    const Buffer operator = EndRead();

    BeginRead();
    Type rightType;
    HANDLE_ERROR(GenerateExpression(&in->right, &rightType, true, false));
    const Buffer right = EndRead();

    SetStreamPosition(pos);

    const Result operatorTypeError = ERROR_RESULT_TOKEN(
        AllocateString2Str("Cannot use operator \"#t\" on types \"%s\" and \"%s\"",
            leftType.name, rightType.name),
        in->operator.lineNumber, in->operator.type);

    bool textWritten = false;

    switch (in->operator.type)
    {
    case Token_Equals:
    case Token_PlusEquals:
    case Token_MinusEquals:
    case Token_SlashEquals:
    case Token_AsteriskEquals:
        // assignment
    {
        bool isLvalue = false;
        if (in->left.type == Node_Literal)
        {
            const LiteralExpr* literal = in->left.ptr;
            if (literal->value.type == Token_Identifier)
                isLvalue = true;
        }

        if (!isLvalue)
            return ERROR_RESULT("Cannot assign to r-value", in->operator.lineNumber);

        HANDLE_ERROR(CheckAssignmentCompatibility(leftType, rightType, in->operator.lineNumber));

        *outType = leftType;

        if (in->operator.type == Token_Equals)
        {
            if (leftType.id == GetKnownType("int").id)
            {
                Write(left.buffer, left.length);
                Write(operator.buffer, operator.length);
                WRITE_LITERAL("(");
                Write(right.buffer, right.length);
                WRITE_LITERAL("|0)");
                textWritten = true;
            }
            else if (leftType.metaType == MetaType_Struct)
            {
                assert(in->operator.type == Token_Equals);
                assert(leftType.id == rightType.id);

                if (in->right.type == Node_FunctionCall ||
                    in->right.type == Node_Literal && LiteralHasFunctionCall(in->right.ptr))
                    Write(right.buffer, right.length);

                GenerateStructAssignment(in->left, in->right, leftType);
                WRITE_LITERAL("0");

                textWritten = true;
            }
            break;
        }

        TokenType arithmeticOperator;
        switch (in->operator.type)
        {
        case Token_PlusEquals:
            arithmeticOperator = Token_Plus;
            break;
        case Token_MinusEquals:
            arithmeticOperator = Token_Minus;
            break;
        case Token_AsteriskEquals:
            arithmeticOperator = Token_Asterisk;
            break;
        case Token_SlashEquals:
            arithmeticOperator = Token_Slash;
            break;
        default: assert(0);
        }

        Write(left.buffer, left.length);
        WRITE_TEXT(GetTokenTypeString(Token_Equals));
        WRITE_LITERAL("((");
        Write(left.buffer, left.length);
        WRITE_TEXT(GetTokenTypeString(arithmeticOperator));
        Write(right.buffer, right.length);
        WRITE_LITERAL(")");
        if (leftType.id == GetKnownType("int").id)
            WRITE_LITERAL("|0");
        WRITE_LITERAL(")");

        textWritten = true;

        // fallthrough
    }

    case Token_Plus:
    case Token_Minus:
    case Token_Slash:
    case Token_Asterisk:
        // arithmetic
    {
        const Type intType = GetKnownType("int");
        const Type floatType = GetKnownType("float");
        if ((leftType.id != intType.id &&
             leftType.id != floatType.id) ||
            (rightType.id != intType.id &&
             rightType.id != floatType.id))
            return operatorTypeError;

        if (leftType.id == floatType.id || rightType.id == floatType.id)
            *outType = floatType;
        else
            *outType = intType;

        break;
    }

    case Token_EqualsEquals:
    case Token_ExclamationEquals:
        // equality
    {
        if (leftType.metaType == MetaType_Struct || rightType.metaType == MetaType_Struct)
            return operatorTypeError;

        if (!CheckAssignmentCompatibility(leftType, rightType, 69420).success)
            return operatorTypeError;

        *outType = GetKnownType("bool");
        break;
    }
    case Token_LeftAngleBracket:
    case Token_RightAngleBracket:
    case Token_LeftAngleEquals:
    case Token_RightAngleEquals:
        // number comparison
    {
        const Type intType = GetKnownType("int");
        const Type floatType = GetKnownType("float");
        if ((leftType.id != intType.id &&
             leftType.id != floatType.id) ||
            (rightType.id != intType.id &&
             rightType.id != floatType.id))
            return operatorTypeError;

        *outType = GetKnownType("bool");
        break;
    }
    case Token_AmpersandAmpersand:
    case Token_PipePipe:
        // boolean operators
    {
        const Type boolType = GetKnownType("bool");
        if (leftType.id != boolType.id || rightType.id != boolType.id)
            return operatorTypeError;

        *outType = GetKnownType("bool");
        break;
    }

    default: assert(0);
    }

    if (!textWritten)
    {
        Write(left.buffer, left.length);
        Write(operator.buffer, operator.length);
        Write(right.buffer, right.length);
    }

    free(left.buffer);
    free(operator.buffer);
    free(right.buffer);

    WRITE_LITERAL(")");
    return SUCCESS_RESULT;
}

Result GenerateExpression(const NodePtr* in, Type* outType, const bool expectingExpression, const bool convertToInteger)
{
    if (in->type == Node_Null)
    {
        if (expectingExpression)
            assert(0);
        return SUCCESS_RESULT;
    }

    if (convertToInteger)
        WRITE_LITERAL("(");

    Result result;

    switch (in->type)
    {
    case Node_Binary:
        result = GenerateBinaryExpression(in->ptr, outType);
        break;
    case Node_Unary:
        result = GenerateUnaryExpression(in->ptr, outType);
        break;
    case Node_Literal:
        result = GenerateLiteralExpression(in->ptr, outType);
        break;
    case Node_FunctionCall:
        result = GenerateFunctionCallExpression(in->ptr, NULL, outType);
        break;
    default:
        assert(0);
    }

    if (convertToInteger)
    {
        if (outType->id == GetKnownType("float").id)
            WRITE_LITERAL("|0");
        WRITE_LITERAL(")");
    }

    return result;
}
