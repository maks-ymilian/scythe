#include "Common.h"

#include "StringUtils.h"

StructTypeInfo GetStructTypeInfoFromType(Type type)
{
	ASSERT(type.expr.ptr != NULL);
	ASSERT(type.modifier == TypeModifier_None ||
		   type.modifier == TypeModifier_Pointer);

	StructDeclStmt* structDecl = NULL;
	switch (type.expr.type)
	{
	case Node_MemberAccess:
	{
		const MemberAccessExpr* memberAccess = type.expr.ptr;
		structDecl = memberAccess->typeReference;
		ASSERT(structDecl != NULL);
		break;
	}
	case Node_Literal:
		break;
	default: INVALID_VALUE(type.expr.type);
	}

	return (StructTypeInfo){
		.effectiveType = type.modifier == TypeModifier_Pointer ? NULL : structDecl,
		.pointerType = type.modifier == TypeModifier_Pointer ? structDecl : NULL,
		.isPointer = type.modifier == TypeModifier_Pointer,
	};
}

StructTypeInfo GetStructTypeInfoFromExpr(NodePtr node)
{
	const StructTypeInfo nullTypeInfo =
		(StructTypeInfo){
			.effectiveType = NULL,
			.pointerType = NULL,
			.isPointer = false,
		};

	switch (node.type)
	{
	case Node_Literal:
		return nullTypeInfo;
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node.ptr;
		return GetStructTypeInfoFromType(blockExpr->type);
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		FuncDeclStmt* funcDecl = memberAccess->funcReference;
		ASSERT(funcDecl != NULL);

		if (funcDecl->oldType.expr.ptr == NULL)
			return GetStructTypeInfoFromType(funcDecl->type);

		StructTypeInfo typeInfo = GetStructTypeInfoFromType(funcDecl->oldType);
		ASSERT(typeInfo.effectiveType);
		return typeInfo;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		VarDeclStmt* varDecl = memberAccess->varReference;
		if (varDecl == NULL)
			return nullTypeInfo;
		return GetStructTypeInfoFromType(varDecl->type);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = node.ptr;
		if (subscript->typeBeforeCollapse.expr.ptr)
			return GetStructTypeInfoFromType(subscript->typeBeforeCollapse);
		else
		{
			StructTypeInfo typeInfo = GetStructTypeInfoFromExpr(subscript->baseExpr);
			if (typeInfo.isPointer)
				return (StructTypeInfo){
					.effectiveType = typeInfo.pointerType,
					.pointerType = NULL,
					.isPointer = false,
				};

			return typeInfo;
		}
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;

		if (binary->operatorType == Binary_Assignment ||
			binary->operatorType == Binary_AddAssign ||
			binary->operatorType == Binary_SubtractAssign ||
			binary->operatorType == Binary_MultiplyAssign ||
			binary->operatorType == Binary_DivideAssign ||
			binary->operatorType == Binary_ModuloAssign ||
			binary->operatorType == Binary_ExponentAssign ||
			binary->operatorType == Binary_BitAndAssign ||
			binary->operatorType == Binary_BitOrAssign ||
			binary->operatorType == Binary_XORAssign)
			return GetStructTypeInfoFromExpr(binary->left);

		if (binary->operatorType == Binary_Add ||
			binary->operatorType == Binary_Subtract)
		{
			StructTypeInfo left = GetStructTypeInfoFromExpr(binary->left);
			StructTypeInfo right = GetStructTypeInfoFromExpr(binary->right);

			StructDeclStmt* type = NULL;
			if (left.isPointer && left.pointerType)
				type = left.pointerType;

			if (right.isPointer && right.pointerType)
				type = right.pointerType;

			// different pointer types are assignable to each other so
			// if both are struct pointers and theyre different types
			// then there is no type
			if (left.isPointer && left.pointerType &&
				right.isPointer && right.pointerType &&
				left.pointerType != right.pointerType)
				return nullTypeInfo;

			if (!type)
				return nullTypeInfo;

			return (StructTypeInfo){
				.effectiveType = NULL,
				.pointerType = type,
				.isPointer = true,
			};
		}

		return nullTypeInfo;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		return GetStructTypeInfoFromExpr(unary->expression);
	}

	default: INVALID_VALUE(node.type);
	}
}

PrimitiveTypeInfo GetPrimitiveTypeInfoFromType(Type type)
{
	bool isPointer = type.modifier == TypeModifier_Pointer;

	if (type.expr.type == Node_Literal)
	{
		const LiteralExpr* literal = type.expr.ptr;
		ASSERT(literal->type == Literal_PrimitiveType);

		PrimitiveType pp = literal->primitiveType;
		(void)pp;

		return (PrimitiveTypeInfo){
			.effectiveType = isPointer ? Primitive_Int : literal->primitiveType,
			.pointerType = literal->primitiveType,
			.isPointer = isPointer,
		};
	}
	else
	{
		ASSERT(isPointer);
		return (PrimitiveTypeInfo){
			.effectiveType = Primitive_Int,
			.pointerType = Primitive_Any,
			.pointerTypeIsStruct = true,
			.isPointer = true,
		};
	}
}

static PrimitiveTypeInfo NonPointerType(PrimitiveType type)
{
	return (PrimitiveTypeInfo){
		.effectiveType = type,
		.pointerType = type,
		.isPointer = false,
	};
}

PrimitiveTypeInfo GetPrimitiveTypeInfoFromExpr(NodePtr node)
{
	switch (node.type)
	{
	case Node_Literal:
	{
		LiteralExpr* literal = node.ptr;
		switch (literal->type)
		{
		case Literal_Number:
			return NonPointerType(Primitive_Float);
		case Literal_String:
		case Literal_Char:
			return NonPointerType(Primitive_Int);
		case Literal_Boolean:
			return NonPointerType(Primitive_Bool);
		default: INVALID_VALUE(literal->type);
		}
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node.ptr;
		ASSERT(funcCall->baseExpr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		FuncDeclStmt* funcDecl = memberAccess->funcReference;
		ASSERT(funcDecl);
		return GetPrimitiveTypeInfoFromType(funcDecl->type);
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node.ptr;
		return GetPrimitiveTypeInfoFromType(blockExpr->type);
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node.ptr;
		VarDeclStmt* varDecl = memberAccess->varReference;
		ASSERT(varDecl);
		return GetPrimitiveTypeInfoFromType(varDecl->type);
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node.ptr;
		PrimitiveTypeInfo typeInfo = GetPrimitiveTypeInfoFromExpr(subscript->baseExpr);
		if (subscript->typeBeforeCollapse.expr.ptr)
			return GetPrimitiveTypeInfoFromType(subscript->typeBeforeCollapse);
		else
		{
			if (typeInfo.isPointer)
				return NonPointerType(typeInfo.pointerType);
			else
				return NonPointerType(Primitive_Any);
		}
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node.ptr;
		switch (unary->operatorType)
		{
		case Unary_Decrement:
		case Unary_Increment:
		case Unary_Minus:
		case Unary_Plus:
			return GetPrimitiveTypeInfoFromExpr(unary->expression);

		case Unary_Negate:
			return NonPointerType(Primitive_Bool);

		default: INVALID_VALUE(unary->operatorType);
		}
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node.ptr;
		switch (binary->operatorType)
		{
		case Binary_IsEqual:
		case Binary_NotEqual:
		case Binary_BoolAnd:
		case Binary_BoolOr:
		case Binary_GreaterThan:
		case Binary_GreaterOrEqual:
		case Binary_LessThan:
		case Binary_LessOrEqual:
			return NonPointerType(Primitive_Bool);

		case Binary_Add:
		case Binary_Subtract:
		case Binary_Multiply:
		case Binary_Divide:
		case Binary_Exponentiation:
		{
			if (GetPrimitiveTypeInfoFromExpr(binary->left).effectiveType == Primitive_Int &&
				GetPrimitiveTypeInfoFromExpr(binary->right).effectiveType == Primitive_Int &&
				binary->operatorType != Binary_Divide &&
				binary->operatorType != Binary_Exponentiation)
				return NonPointerType(Primitive_Int);
			else
				return NonPointerType(Primitive_Float);
		}

		case Binary_Modulo:
		case Binary_LeftShift:
		case Binary_RightShift:
		case Binary_BitAnd:
		case Binary_BitOr:
		case Binary_XOR:
			return NonPointerType(Primitive_Int);

		case Binary_Assignment:
		case Binary_AddAssign:
		case Binary_SubtractAssign:
		case Binary_MultiplyAssign:
		case Binary_DivideAssign:
		case Binary_ModuloAssign:
		case Binary_ExponentAssign:
		case Binary_BitAndAssign:
		case Binary_BitOrAssign:
		case Binary_XORAssign:
			return GetPrimitiveTypeInfoFromExpr(binary->left);

		default: INVALID_VALUE(binary->operatorType);
		}
	}
	default: INVALID_VALUE(node.type);
	}
}

Type AllocTypeFromExpr(NodePtr node, int lineNumber)
{
	StructTypeInfo structType = GetStructTypeInfoFromExpr(node);
	StructDeclStmt* type = structType.isPointer ? structType.pointerType : structType.effectiveType;
	if (type)
	{
		return (Type){
			.expr = AllocASTNode(
				&(MemberAccessExpr){
					.lineNumber = lineNumber,
					.typeReference = type,
				},
				sizeof(MemberAccessExpr), Node_MemberAccess),
			.modifier = structType.isPointer ? TypeModifier_Pointer : TypeModifier_None,
		};
	}
	else
	{
		PrimitiveTypeInfo primitiveType = GetPrimitiveTypeInfoFromExpr(node);
		ASSERT(!primitiveType.pointerTypeIsStruct);
		return (Type){
			.expr = AllocPrimitiveType(primitiveType.isPointer ? primitiveType.pointerType : primitiveType.effectiveType, lineNumber),
			.modifier = primitiveType.isPointer ? TypeModifier_Pointer : TypeModifier_None,
		};
	}
}

VarDeclStmt* GetPtrMember(StructDeclStmt* type)
{
	ASSERT(type->isArrayType);
	ASSERT(type->members.length == ARRAY_STRUCT_MEMBER_COUNT);
	NodePtr* node = type->members.array[ARRAY_STRUCT_PTR_MEMBER_INDEX];
	ASSERT(node->type == Node_VariableDeclaration);
	return node->ptr;
}

NodePtr AllocIdentifier(VarDeclStmt* varDecl, int lineNumber)
{
	ASSERT(varDecl != NULL);
	return AllocASTNode(
		&(MemberAccessExpr){
			.lineNumber = lineNumber,
			.start = NULL_NODE,
			.identifiers = (Array){.array = NULL},
			.funcReference = NULL,
			.typeReference = NULL,
			.varReference = varDecl,
		},
		sizeof(MemberAccessExpr), Node_MemberAccess);
}

NodePtr AllocAssignmentStatement(NodePtr left, NodePtr right, int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.left = left,
					.right = right,
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

NodePtr AllocSetVariable(VarDeclStmt* varDecl, NodePtr value, int lineNumber)
{
	return AllocAssignmentStatement(AllocIdentifier(varDecl, lineNumber), value, lineNumber);
}

NodePtr AllocIntConversion(NodePtr expr, int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_BitOr,
			.right = expr,
			.left = AllocUInt64Integer(0, lineNumber),
		},
		sizeof(BinaryExpr), Node_Binary);
}

NodePtr AllocPrimitiveType(PrimitiveType primitiveType, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_PrimitiveType,
			.primitiveType = primitiveType,
		},
		sizeof(LiteralExpr), Node_Literal);
}

NodePtr AllocUInt64Integer(uint64_t value, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_Number,
			.number = AllocUInt64ToString(value),
		},
		sizeof(LiteralExpr), Node_Literal);
}

NodePtr AllocSizeInteger(size_t value, int lineNumber)
{
	return AllocASTNode(
		&(LiteralExpr){
			.lineNumber = lineNumber,
			.type = Literal_Number,
			.number = AllocUInt64ToString(value),
		},
		sizeof(LiteralExpr), Node_Literal);
}
