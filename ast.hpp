#pragma once

#include "parsley/common.hpp"

enum class ExpressionType {
	INT_LITERAL,
	BINARY_EXPRESSION
};

class Expression {
public:
	virtual ~Expression() = default;
	virtual ExpressionType get_type() const = 0;
};

template <class T, class... A> Reference<Expression> make_expr(A&&... a) {
	return Reference<Expression>(new T(std::forward<A>(a)...));
}

template <class T> T* expr_cast(Expression* e) {
	if (e && e->get_type() == T::TYPE) {
		return static_cast<T*>(e);
	}
	else {
		return nullptr;
	}
}

class IntLiteral final: public Expression {
	std::int32_t value;
public:
	IntLiteral(std::int32_t value): value(value) {}
	static constexpr ExpressionType TYPE = ExpressionType::INT_LITERAL;
	ExpressionType get_type() const override {
		return TYPE;
	}
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
	BinaryExpression(BinaryOperation operation, Reference<Expression>&& left, Reference<Expression>&& right): operation(operation), left(std::move(left)), right(std::move(right)) {}
	static constexpr ExpressionType TYPE = ExpressionType::BINARY_EXPRESSION;
	ExpressionType get_type() const override {
		return TYPE;
	}
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
