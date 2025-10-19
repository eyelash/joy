#pragma once

#include "parsley/common.hpp"

enum {
	TYPE_ID_INT_LITERAL,
	TYPE_ID_BINARY_EXPRESSION,
	TYPE_ID_BLOCK_STATEMENT,
	TYPE_ID_EMPTY_STATEMENT,
	TYPE_ID_EXPRESSION_STATEMENT
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

class Statement: public Dynamic {
public:
	Statement(int type_id): Dynamic(type_id) {}
};

class Block {
	std::vector<Reference<Statement>> statements;
public:
	Block() {}
	Block(std::vector<Reference<Statement>>&& statements): statements(std::move(statements)) {}
	const std::vector<Reference<Statement>>& get_statements() const {
		return statements;
	}
};

class BlockStatement final: public Statement {
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_BLOCK_STATEMENT;
	BlockStatement(Block&& block): Statement(TYPE_ID), block(std::move(block)) {}
};

class EmptyStatement final: public Statement {
public:
	static constexpr int TYPE_ID = TYPE_ID_EMPTY_STATEMENT;
	EmptyStatement(): Statement(TYPE_ID) {}
};

class ExpressionStatement final: public Statement {
	Reference<Expression> expression;
public:
	static constexpr int TYPE_ID = TYPE_ID_EXPRESSION_STATEMENT;
	ExpressionStatement(Reference<Expression>&& expression): Statement(TYPE_ID), expression(std::move(expression)) {}
	const Expression* get_expression() const {
		return expression;
	}
};

class Function {
	std::string name;
	Block block;
public:
	Function(std::string&& name, Block&& block): name(std::move(name)), block(std::move(block)) {}
	const std::string& get_name() const {
		return name;
	}
	const Block& get_block() const {
		return block;
	}
};

class Program {
	std::vector<Function> functions;
public:
	Program() {}
	Program(std::vector<Function>&& functions): functions(std::move(functions)) {}
	const std::vector<Function>& get_functions() const {
		return functions;
	}
};
