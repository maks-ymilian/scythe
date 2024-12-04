#include "StatementGenerator.h"

#include "ExpressionGenerator.h"
#include "StringUtils.h"

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    Type type;
    const Result result = GenerateExpression(&in->expr, &type, false, false);
    WRITE_LITERAL(";\n");
    return result;
}

static Result GenerateVariableDeclaration(const VarDeclStmt* in, const char* prefix, int* outUniqueName);

static void GenerateStructMemberNames(
    const LiteralExpr* expr,
    const Type exprType,
    const char* beforeText,
    const char* afterText,
    const bool reverseOrder)
{
    const SymbolData* symbol = GetKnownSymbol(exprType.name, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData structSymbol = symbol->structData;

    for (int i = reverseOrder ? structSymbol.members->length - 1 : 0;
         reverseOrder ? i >= 0 : i < structSymbol.members->length;
         reverseOrder ? --i : ++i)
    {
        const NodePtr* node = structSymbol.members->array[i];

        switch (node->type)
        {
        case Node_VariableDeclaration:
        {
            const VarDeclStmt* varDecl = node->ptr;

            LiteralExpr literal = *expr;
            LiteralExpr next = (LiteralExpr){varDecl->identifier, NULL};
            LiteralExpr* last = &literal;
            while (last->next.ptr != NULL)
            {
                assert(last->next.type == Node_Literal);
                last = last->next.ptr;
            }
            last->next = (NodePtr){.ptr = &next, .type = Node_Literal};
            NodePtr node = (NodePtr){&literal, Node_Literal};

            Type memberType;
            ASSERT_ERROR(GetTypeFromToken(varDecl->type, &memberType, false));
            if (memberType.metaType == MetaType_Struct)
            {
                GenerateStructMemberNames(&literal, memberType, beforeText, afterText, reverseOrder);
                last->next = NULL_NODE;
                break;
            }

            if (beforeText != NULL)
                WRITE_TEXT(beforeText);
            Type _;
            ASSERT_ERROR(GenerateExpression(&node, &_, true, false));
            last->next = NULL_NODE;
            if (afterText != NULL)
                WRITE_TEXT(afterText);
            break;
        }
        default:
            assert(0);
        }
    }
}

static void GeneratePushStructVariable(NodePtr expr, const Type exprType)
{
    PushScope();

    LiteralExpr variable;
    if (expr.type == Node_FunctionCall)
    {
        const FuncCallExpr* funcCall = expr.ptr;

        const Token typeToken = (Token){Token_Identifier, funcCall->identifier.lineNumber, (char*)exprType.name};
        const Token identifier = (Token){Token_Identifier, funcCall->identifier.lineNumber, "temp"};
        const VarDeclStmt varDecl = (VarDeclStmt){typeToken, identifier, expr};
        ASSERT_ERROR(GenerateVariableDeclaration(&varDecl, NULL, NULL));

        variable = (LiteralExpr){identifier, NULL};
        expr = (NodePtr){&variable, Node_Literal};
    }
    else if (expr.type == Node_Binary)
    {
        const BinaryExpr* binary = expr.ptr;
        assert(binary->operator.type == Token_Equals);

        Type _;
        ASSERT_ERROR(GenerateExpression(&expr, &_, true, false));
        WRITE_LITERAL(";");

        expr = binary->left;
    }

    assert(expr.type == Node_Literal);
    GenerateStructMemberNames(expr.ptr, exprType, "stack_push(", ");", false);

    PopScope(NULL);
}

Result GeneratePushValue(const NodePtr expr, const Type expectedType, const long lineNumber)
{
    const size_t pos = GetStreamPosition();

    WRITE_LITERAL("stack_push(");
    Type exprType;
    HANDLE_ERROR(GenerateExpression(&expr, &exprType, true, expectedType.id == GetKnownType("int").id));
    WRITE_LITERAL(");");

    HANDLE_ERROR(CheckAssignmentCompatibility(expectedType, exprType, lineNumber));

    if (exprType.metaType == MetaType_Struct)
    {
        SetStreamPosition(pos);
        GeneratePushStructVariable(expr, exprType);
    }

    return SUCCESS_RESULT;
}

Result GeneratePopValue(const VarDeclStmt* varDecl)
{
    int uniqueName;
    HANDLE_ERROR(GenerateVariableDeclaration(varDecl, NULL, &uniqueName));

    Type type;
    ASSERT_ERROR(GetTypeFromToken(varDecl->type, &type, false));
    if (type.metaType == MetaType_Struct)
    {
        const LiteralExpr literal = (LiteralExpr){.value = varDecl->identifier, .next = NULL};
        GenerateStructMemberNames(&literal, type, NULL, "=stack_pop();", true);
        return SUCCESS_RESULT;
    }

    WRITE_LITERAL(VARIABLE_PREFIX);
    WRITE_TEXT(varDecl->identifier.text);
    WriteInteger(uniqueName);
    WRITE_LITERAL("=stack_pop();");

    return SUCCESS_RESULT;
}

Result GenerateLoopControlStatement(const LoopControlStmt* in);

static Result GenerateReturnStatement(const ReturnStmt* in)
{
    const ScopeNode* functionScope = GetScopeType(ScopeType_Function);
    if (functionScope == NULL)
        return ERROR_RESULT("Return statement must be inside a function", in->returnToken.lineNumber);

    WRITE_LITERAL("(");
    WRITE_LITERAL("__hasReturned = 1;");

    if (GetScopeType(ScopeType_Loop) != NULL)
    {
        const LoopControlStmt stmt = {.type = LoopControl_Break, .lineNumber = in->returnToken.lineNumber};
        ASSERT_ERROR(GenerateLoopControlStatement(&stmt));
    }

    const Type returnType = functionScope->functionReturnType;
    const bool isVoid = returnType.id == GetKnownType("void").id;
    if (in->expr.type == Node_Null)
    {
        if (!isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" must return a value", returnType.name),
                in->returnToken.lineNumber);
    }
    else
    {
        if (isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" cannot return a value", returnType.name),
                in->returnToken.lineNumber);

        HANDLE_ERROR(GeneratePushValue(in->expr, returnType, in->returnToken.lineNumber));
    }

    WRITE_LITERAL(");\n");

    return SUCCESS_RESULT;
}

static Result GenerateStructVariableDeclaration(const VarDeclStmt* in, const Type type, const char* prefix)
{
    const SymbolData* symbol = GetKnownSymbol(in->type.text, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData data = symbol->structData;

    PushScope();

    for (int i = 0; i < data.members->length; ++i)
    {
        const NodePtr* node = data.members->array[i];
        switch (node->type)
        {
        case Node_VariableDeclaration:
        {
            if (prefix == NULL)
                prefix = "";
            char newPrefix[strlen(prefix) + strlen(in->identifier.text) + 2];
            if (snprintf(newPrefix, sizeof(newPrefix), "%s%s.", prefix, in->identifier.text) == 0)
                assert(0);
            HANDLE_ERROR(GenerateVariableDeclaration(node->ptr, newPrefix, NULL));
            break;
        }
        default: assert(0);
        }
    }

    Map symbolTable;
    PopScope(&symbolTable);
    HANDLE_ERROR(RegisterVariable(in->identifier, type, &symbolTable, NULL, false, in->public, NULL));

    if (in->initializer.type != Node_Null)
    {
        LiteralExpr left = (LiteralExpr){in->identifier, NULL};
        BinaryExpr expr = (BinaryExpr)
        {
            (NodePtr){&left, Node_Literal},
            (Token){Token_Equals, in->type.lineNumber, NULL},
            in->initializer
        };
        const NodePtr node = (NodePtr){&expr, Node_Binary};
        Type outType;
        HANDLE_ERROR(GenerateExpression(&node, &outType, true, false));
        WRITE_LITERAL(";\n");
    }

    return SUCCESS_RESULT;
}

static Result GenerateExternalVariableDeclaration(const VarDeclStmt* in, const Type type)
{
    if (type.metaType != MetaType_Primitive)
        return ERROR_RESULT("Only primitive types are allowed for external variable declarations", in->type.lineNumber);

    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL, in->externalIdentifier.text, true, in->public, NULL));

    return SUCCESS_RESULT;
}

static Result GenerateVariableDeclaration(const VarDeclStmt* in, const char* prefix, int* outUniqueName)
{
    const ScopeNode* scope = GetCurrentScope();
    const bool globalScope = scope->parent == NULL;

    if (!globalScope && in->public)
        return ERROR_RESULT_TOKEN("Variables with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_Public);
    if (!globalScope && in->external)
        return ERROR_RESULT_TOKEN("Variables with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_External);

    Type type;
    HANDLE_ERROR(GetTypeFromToken(in->type, &type, false));

    if (in->external)
        return GenerateExternalVariableDeclaration(in, type);

    if (globalScope) BeginRead();

    if (type.metaType != MetaType_Primitive && in->public && !type.public)
        return ERROR_RESULT(
            AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", type.name),
            in->identifier.lineNumber);

    if (type.metaType == MetaType_Struct)
    {
        const Result result = GenerateStructVariableDeclaration(in, type, prefix);
        if (globalScope) EndReadMove(SIZE_MAX);
        return result;
    }

    assert(in->identifier.type == Token_Identifier);
    int uniqueName;
    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL, NULL, false, in->public, &uniqueName));

    NodePtr initializer;
    LiteralExpr defaultLiteral;
    if (in->initializer.type == Node_Null)
    {
        if (type.id == GetKnownType("int").id || type.id == GetKnownType("float").id)
            defaultLiteral = (LiteralExpr){(Token){Token_NumberLiteral, in->identifier.lineNumber, "0"}};
        else if (type.id == GetKnownType("bool").id)
            defaultLiteral = (LiteralExpr){(Token){Token_False, in->identifier.lineNumber, NULL}};
        else
            return ERROR_RESULT(AllocateString1Str("Variable of type \"%s\" must be initialized", type.name),
                                in->identifier.lineNumber);

        initializer = (NodePtr){&defaultLiteral, Node_Literal};
    }
    else
        initializer = in->initializer;

    char* fullName = in->identifier.text;
    if (prefix != NULL) fullName = AllocateString2Str("%s%s", prefix, in->identifier.text);

    const bool isInteger = type.id == GetKnownType("int").id;

    WRITE_LITERAL(VARIABLE_PREFIX);
    WRITE_TEXT(fullName);
    WriteInteger(uniqueName);
    WRITE_LITERAL("=");
    Type initializerType;
    HANDLE_ERROR(GenerateExpression(&initializer, &initializerType, true, isInteger));
    WRITE_LITERAL(";\n");

    HANDLE_ERROR(CheckAssignmentCompatibility(type, initializerType, in->identifier.lineNumber));

    if (prefix != NULL) free(fullName);

    if (globalScope) EndReadMove(SIZE_MAX);

    if (outUniqueName != NULL) *outUniqueName = uniqueName;
    return SUCCESS_RESULT;
}

static bool ControlPathsReturn(const NodePtr node, const bool allPathsMustReturn)
{
    switch (node.type)
    {
    case Node_Return:
        return true;
    case Node_If:
    {
        const IfStmt* ifStmt = node.ptr;
        const bool trueReturns = ControlPathsReturn(ifStmt->trueStmt, allPathsMustReturn);
        const bool falseReturns = ControlPathsReturn(ifStmt->falseStmt, allPathsMustReturn);

        if (allPathsMustReturn)
            return trueReturns && falseReturns;
        return trueReturns || falseReturns;
    }
    case Node_Block:
    {
        const BlockStmt* block = node.ptr;
        for (int i = 0; i < block->statements.length; ++i)
        {
            const NodePtr* statement = block->statements.array[i];
            if (ControlPathsReturn(*statement, allPathsMustReturn))
                return true;
        }
        return false;
    }
    case Node_While:
    {
        return !allPathsMustReturn;
    }
    case Node_ExpressionStatement:
    case Node_VariableDeclaration:
    case Node_FunctionDeclaration:
    case Node_Null:
        return false;
    default: assert(0);
    }
}

static Result GenerateFunctionBlock(const BlockStmt* in)
{
    WRITE_LITERAL("(0;\n");

    long returnIfStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(GenerateStatement(node));

        if (ControlPathsReturn(*node, false))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            WRITE_LITERAL("(!__hasReturned) ? (\n");
            returnIfStatementCount++;
        }
    }
    for (int i = 0; i < returnIfStatementCount; ++i)
        WRITE_LITERAL(");");

    WRITE_LITERAL(");\n");

    return SUCCESS_RESULT;
}

static Result ParseParameterArray(const FuncDeclStmt* in, Array* outArray)
{
    Array params = AllocateArray(sizeof(FunctionParameter));
    bool hasOptionalParams = false;
    for (int i = 0; i < in->parameters.length; ++i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == Node_VariableDeclaration);
        const VarDeclStmt* varDecl = node->ptr;

        if (varDecl->initializer.ptr != NULL)
            hasOptionalParams = true;
        else if (hasOptionalParams == true)
            return ERROR_RESULT("Optional parameters must be at end of parameter list", in->identifier.lineNumber);

        FunctionParameter param;
        param.name = varDecl->identifier.text;
        param.defaultValue = varDecl->initializer;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &param.type, false));
        ArrayAdd(&params, &param);

        if (varDecl->initializer.ptr != NULL)
        {
            Type initializerType;
            const size_t pos = GetStreamPosition();
            HANDLE_ERROR(GenerateExpression(&varDecl->initializer, &initializerType, true, false));
            SetStreamPosition(pos);
            HANDLE_ERROR(CheckAssignmentCompatibility(param.type, initializerType, varDecl->identifier.lineNumber));
        }

        if (param.type.metaType != MetaType_Primitive && !param.type.public && in->public)
            return ERROR_RESULT(
                AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", param.type.name),
                varDecl->identifier.lineNumber);
    }

    *outArray = params;
    return SUCCESS_RESULT;
}

static Result GenerateExternalFunctionDeclaration(const FuncDeclStmt* in, const Type returnType)
{
    assert(in->block.type == Node_Null);
    assert(in->identifier.type == Token_Identifier);

    if (returnType.metaType != MetaType_Primitive)
        return ERROR_RESULT("External function declarations must return a primitive type", in->type.lineNumber);

    Array params;
    HANDLE_ERROR(ParseParameterArray(in, &params));

    for (int i = 0; i < params.length; ++i)
    {
        const FunctionParameter* funcParam = params.array[i];
        if (funcParam->type.metaType != MetaType_Primitive)
            return ERROR_RESULT("External function declarations can only have parameters of primitive types",
                                in->identifier.lineNumber);
    }

    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params, in->externalIdentifier.text, true, in->public, NULL));

    return SUCCESS_RESULT;
}

static Result GenerateFunctionDeclaration(const FuncDeclStmt* in)
{
    const ScopeNode* scope = GetCurrentScope();
    const bool globalScope = scope->parent == NULL;
    if (!globalScope && in->public)
        return ERROR_RESULT_TOKEN("Functions with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_Public);
    if (!globalScope && in->external)
        return ERROR_RESULT_TOKEN("Functions with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_External);

    Type returnType;
    HANDLE_ERROR(GetTypeFromToken(in->type, &returnType, true));

    if (returnType.metaType != MetaType_Primitive && in->public && !returnType.public)
        return ERROR_RESULT(
            AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", returnType.name),
            in->identifier.lineNumber);

    if (in->external)
        return GenerateExternalFunctionDeclaration(in, returnType);

    BeginRead();

    PushScope();

    ScopeNode* currentScope = GetCurrentScope();
    currentScope->scopeType = ScopeType_Function;
    currentScope->functionReturnType = returnType;

    assert(in->block.type == Node_Block);

    if (!ControlPathsReturn(in->block, true) && returnType.id != GetKnownType("void").id)
        return ERROR_RESULT("Not all control paths return a value", in->identifier.lineNumber);

    assert(in->identifier.type == Token_Identifier);

    const size_t pos = GetStreamPosition();
    BeginRead();
    WRITE_LITERAL("(0;");
    WRITE_LITERAL("__hasReturned = 0;");

    Array params;
    HANDLE_ERROR(ParseParameterArray(in, &params));

    for (int i = in->parameters.length - 1; i >= 0; --i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == Node_VariableDeclaration);
        const VarDeclStmt* varDecl = node->ptr;

        HANDLE_ERROR(GeneratePopValue(varDecl));
    }

    HANDLE_ERROR(GenerateFunctionBlock(in->block.ptr));
    WRITE_LITERAL(");\n");
    const Buffer functionBlock = EndRead();
    SetStreamPosition(pos);

    PopScope(NULL);

    int uniqueName;
    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params, NULL, false, in->public, &uniqueName));

    WRITE_LITERAL("function ");
    WRITE_LITERAL("func_");
    WRITE_TEXT(in->identifier.text);
    WriteInteger(uniqueName);
    WRITE_LITERAL("()\n");
    Write(functionBlock.buffer, functionBlock.length);

    EndReadMove(SIZE_MAX);

    return SUCCESS_RESULT;
}

static Result GenerateStructDeclaration(const StructDeclStmt* in)
{
    HANDLE_ERROR(RegisterStruct(in->identifier, &in->members, in->public, NULL));

    Type type;
    if (GetTypeFromToken(in->identifier, &type, false).success == false)
        assert(0);

    for (int i = 0; i < in->members.length; ++i)
    {
        const NodePtr* node = in->members.array[i];
        if (node->type != Node_VariableDeclaration)
            continue;

        const VarDeclStmt* varDecl = node->ptr;
        Type varDeclType;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &varDeclType, false));
        if (varDeclType.id == type.id)
            return ERROR_RESULT(
                AllocateString2Str("Member \"%s\" of type \"%s\" causes a cycle in the struct layout",
                    varDecl->identifier.text, type.name),
                varDecl->identifier.lineNumber);
    }

    return SUCCESS_RESULT;
}

static Result GenerateBlockStatement(const BlockStmt* in)
{
    PushScope();

    WRITE_LITERAL("(0;\n");
    for (int i = 0; i < in->statements.length; ++i)
        HANDLE_ERROR(GenerateStatement(in->statements.array[i]));
    WRITE_LITERAL(");\n");

    PopScope(NULL);

    return SUCCESS_RESULT;
}

static Result GenerateSectionStatement(const SectionStmt* in)
{
    BeginRead();
    WRITE_LITERAL("\n@");
    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("\n");
    const Result result = GenerateBlockStatement(in->block.ptr);
    EndReadMove(SIZE_MAX);
    return result;
}

static Result GenerateIfStatement(const IfStmt* in)
{
    Type exprType;
    HANDLE_ERROR(GenerateExpression(&in->expr, &exprType, true, false));
    const Type boolType = GetKnownType("bool");
    if (exprType.id != boolType.id)
        return ERROR_RESULT(
            AllocateString1Str("Expression inside if statement must be evaluate to a \"%s\"", boolType.name),
            in->ifToken.lineNumber);

    WRITE_LITERAL("?(\n");
    HANDLE_ERROR(GenerateStatement(&in->trueStmt));
    WRITE_LITERAL(")");
    if (in->falseStmt.type != Node_Null)
    {
        WRITE_LITERAL(":(\n");
        HANDLE_ERROR(GenerateStatement(&in->falseStmt));
        WRITE_LITERAL(")");
    }
    WRITE_LITERAL(";\n");

    return SUCCESS_RESULT;
}

static bool ControlPathsContainLoopControl(const NodePtr node)
{
    switch (node.type)
    {
    case Node_LoopControl:
    case Node_Return:
        return true;
    case Node_If:
    {
        const IfStmt* ifStmt = node.ptr;
        const bool trueReturns = ControlPathsContainLoopControl(ifStmt->trueStmt);
        const bool falseReturns = ControlPathsContainLoopControl(ifStmt->falseStmt);
        return trueReturns || falseReturns;
    }
    case Node_Block:
    {
        const BlockStmt* block = node.ptr;
        for (int i = 0; i < block->statements.length; ++i)
        {
            const NodePtr* statement = block->statements.array[i];
            if (ControlPathsContainLoopControl(*statement))
                return true;
        }
        return false;
    }
    case Node_While:
        return true;
    case Node_ExpressionStatement:
    case Node_VariableDeclaration:
    case Node_FunctionDeclaration:
    case Node_Null:
        return false;
    default: assert(0);
    }
}

static Result GenerateWhileBlock(const BlockStmt* in)
{
    const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
    assert(loopScope != NULL);

    WRITE_LITERAL("(0;\n");

    long controlStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(GenerateStatement(node));

        if (ControlPathsContainLoopControl(*node))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            WRITE_LITERAL("(!__continue");
            WriteInteger(loopScope->depth);
            if (GetScopeType(ScopeType_Function) != NULL)
                WRITE_LITERAL("&& !__hasReturned");
            WRITE_LITERAL(") ? (\n");
            controlStatementCount++;
        }
    }
    for (int i = 0; i < controlStatementCount; ++i)
        WRITE_LITERAL(");");

    WRITE_LITERAL(");\n");

    return SUCCESS_RESULT;
}

Result GenerateWhileStatement(const WhileStmt* in)
{
    PushScope();
    ScopeNode* scope = GetCurrentScope();
    scope->scopeType = ScopeType_Loop;

    WRITE_LITERAL("__break");
    WriteInteger(scope->depth);
    WRITE_LITERAL("= 0;");

    WRITE_LITERAL("__continue");
    WriteInteger(scope->depth);
    WRITE_LITERAL("= 0;");

    WRITE_LITERAL("while(__break");
    WriteInteger(scope->depth);
    WRITE_LITERAL(" == 0 && (");
    Type exprType;
    HANDLE_ERROR(GenerateExpression(&in->expr, &exprType, true, false));
    WRITE_LITERAL("))\n");

    if (exprType.id != GetKnownType("bool").id)
        return ERROR_RESULT("Expression inside while block must evaluate to a bool type",);

    assert(in->stmt.type == Node_Block);
    HANDLE_ERROR(GenerateWhileBlock(in->stmt.ptr));

    PopScope(NULL);

    return SUCCESS_RESULT;
}

Result GenerateLoopControlStatement(const LoopControlStmt* in)
{
    const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
    if (loopScope == NULL)
    {
        const TokenType token = in->type == LoopControl_Break ? Token_Break : Token_Continue;
        return ERROR_RESULT_TOKEN("Cannot use \"#t\" statement outside of a loop", in->lineNumber, token);
    }

    WRITE_LITERAL("(");
    if (in->type == LoopControl_Break)
    {
        WRITE_LITERAL("__break");
        WriteInteger(loopScope->depth);
        WRITE_LITERAL("= 1;");
    }
    WRITE_LITERAL("__continue");
    WriteInteger(loopScope->depth);
    WRITE_LITERAL("= 1;");
    WRITE_LITERAL(");");

    return SUCCESS_RESULT;
}

Result GenerateStatement(const NodePtr* in)
{
    switch (in->type)
    {
    case Node_Block:
    {
        const ScopeNode* functionScope = GetScopeType(ScopeType_Function);
        if (functionScope != NULL) return GenerateFunctionBlock(in->ptr);

        const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
        if (loopScope != NULL) return GenerateWhileBlock(in->ptr);

        return GenerateBlockStatement(in->ptr);
    }
    case Node_Import: return SUCCESS_RESULT;
    case Node_ExpressionStatement: return GenerateExpressionStatement(in->ptr);
    case Node_Return: return GenerateReturnStatement(in->ptr);
    case Node_Section: return GenerateSectionStatement(in->ptr);
    case Node_VariableDeclaration: return GenerateVariableDeclaration(in->ptr, NULL, NULL);
    case Node_FunctionDeclaration: return GenerateFunctionDeclaration(in->ptr);
    case Node_StructDeclaration: return GenerateStructDeclaration(in->ptr);
    case Node_If: return GenerateIfStatement(in->ptr);
    case Node_While: return GenerateWhileStatement(in->ptr);
    case Node_LoopControl: return GenerateLoopControlStatement(in->ptr);
    default: assert(0);
    }
}
