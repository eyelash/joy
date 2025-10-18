#pragma once

#include "parsley/common.hpp"

enum {
	TYPE_ID_INT_LITERAL,
	TYPE_ID_BINARY_EXPRESSION
};

enum class ExpressionType {
	INT_LITERAL,
	BINARY_EXPRESSION
};

class Expression: public Dynamic {
public:
	Expression(int type_id): Dynamic(type_id) {}
};

template <class T, class... A> Reference<Expression> make_expr(A&&... a) {
	return Reference<Expression>(new T(std::forward<A>(a)...));
}

class IntLiteral final: public Expression {
	std::int32_t value;
public:
	static constexpr int TYPE_ID = TYPE_ID_INT_LITERAL;
	IntLiteral(std::int32_t value): Expression(TYPE_ID), value(value) {}
	std::int32_t get_value() const {
		return value;
	}
};

enum class BinaryOperation {
	ADD,
	SUB,
	MUL,
	DIV,
	REM
};

class BinaryExpression final: public Expression {
	BinaryOperation operation;
	Reference<Expression> left;
	Reference<Expression> right;
public:
	static constexpr int TYPE_ID = TYPE_ID_BINARY_EXPRESSION;
	BinaryExpression(BinaryOperation operation, Reference<Expression>&& left, Reference<Expression>&& right): Expression(TYPE_ID), operation(operation), left(std::move(left)), right(std::move(right)) {}
	BinaryOperation get_operation() const {
		return operation;
	}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
};
