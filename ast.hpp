#pragma once

#include "parsley/common.hpp"
#include "parsley/printer.hpp"

class Error {
	std::string path;
	SourceLocation location;
	std::string message;
public:
	Error(const char* path, const SourceLocation& location, const std::string& message): path(path), location(location), message(message) {}
	template <class C> void print(const C& color, const char* severity) const {
		using namespace printer;
		Context context(std::cerr);
		auto source = read_file(path.c_str());
		print_message(context, path.c_str(), StringView(source.data(), source.size()), location, color, severity, get_printer(message));
		context.print('\n');
	}
};

class Errors {
	std::vector<Error> errors;
	std::vector<Error> warnings;
public:
	explicit operator bool() const {
		return !errors.empty();
	}
	void add_error(const char* path, const SourceLocation& location, const std::string& message) {
		errors.emplace_back(path, location, message);
	}
	void add_warning(const char* path, const SourceLocation& location, const std::string& message) {
		warnings.emplace_back(path, location, message);
	}
	void print() {
		for (const Error& error: warnings) {
			error.print(printer::yellow, "warning");
		}
		for (const Error& error: errors) {
			error.print(printer::red, "error");
		}
	}
};

enum {
	TYPE_ID_VOID_TYPE,
	TYPE_ID_INT_TYPE,
	TYPE_ID_STRUCT_TYPE,
	TYPE_ID_INT_LITERAL,
	TYPE_ID_NAME,
	TYPE_ID_BINARY_EXPRESSION,
	TYPE_ID_ASSIGNMENT,
	TYPE_ID_CALL,
	TYPE_ID_BLOCK_STATEMENT,
	TYPE_ID_EMPTY_STATEMENT,
	TYPE_ID_LET_STATEMENT,
	TYPE_ID_IF_STATEMENT,
	TYPE_ID_WHILE_STATEMENT,
	TYPE_ID_EXPRESSION_STATEMENT,
	TYPE_ID_FUNCTION,
	TYPE_ID_STRUCTURE,
	TYPE_ID_PROGRAM
};

class Type: public Dynamic {
public:
	Type(int type_id): Dynamic(type_id) {}
};

class VoidType final: public Type {
public:
	static constexpr int TYPE_ID = TYPE_ID_VOID_TYPE;
	VoidType(): Type(TYPE_ID) {}
};

class IntType final: public Type {
public:
	static constexpr int TYPE_ID = TYPE_ID_INT_TYPE;
	IntType(): Type(TYPE_ID) {}
};

class StructType final: public Type {
public:
	class Member {
		StringView name;
		const Type* type;
	public:
		Member(const StringView& name, const Type* type): name(name), type(type) {}
		StringView get_name() const {
			return name;
		}
		const Type* get_type() const {
			return type;
		}
	};
private:
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCT_TYPE;
	StructType(): Type(TYPE_ID) {}
	void add_member(const StringView& name, const Type* type) {
		members.emplace_back(name, type);
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Expression: public Dynamic {
	SourceLocation location;
public:
	Expression(int type_id): Dynamic(type_id), location(0, 0) {}
	void set_location(const SourceLocation& location) {
		this->location = location;
	}
	const SourceLocation& get_location() const {
		return location;
	}
};

class IntLiteral final: public Expression {
	std::int32_t value;
public:
	static constexpr int TYPE_ID = TYPE_ID_INT_LITERAL;
	IntLiteral(std::int32_t value): Expression(TYPE_ID), value(value) {}
	std::int32_t get_value() const {
		return value;
	}
};

class Name final: public Expression {
	std::string name;
public:
	static constexpr int TYPE_ID = TYPE_ID_NAME;
	Name(std::string&& name): Expression(TYPE_ID), name(std::move(name)) {}
	const std::string& get_name() const {
		return name;
	}
};

enum class BinaryOperation {
	ADD,
	SUB,
	MUL,
	DIV,
	REM,
	EQ,
	NE,
	LT,
	LE,
	GT,
	GE
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

class Assignment final: public Expression {
	Reference<Expression> left;
	Reference<Expression> right;
public:
	static constexpr int TYPE_ID = TYPE_ID_ASSIGNMENT;
	Assignment(Reference<Expression>&& left, Reference<Expression>&& right): Expression(TYPE_ID), left(std::move(left)), right(std::move(right)) {}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
};

class Call final: public Expression {
	Reference<Expression> expression;
	std::vector<Reference<Expression>> arguments;
public:
	static constexpr int TYPE_ID = TYPE_ID_CALL;
	Call(Reference<Expression>&& expression, std::vector<Reference<Expression>>&& arguments): Expression(TYPE_ID), expression(std::move(expression)), arguments(std::move(arguments)) {}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<Reference<Expression>>& get_arguments() const {
		return arguments;
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
	const Block* get_block() const {
		return &block;
	}
};

class EmptyStatement final: public Statement {
public:
	static constexpr int TYPE_ID = TYPE_ID_EMPTY_STATEMENT;
	EmptyStatement(): Statement(TYPE_ID) {}
};

class LetStatement final: public Statement {
	std::string name;
	Reference<Expression> type;
	Reference<Expression> expression;
public:
	static constexpr int TYPE_ID = TYPE_ID_LET_STATEMENT;
	LetStatement(std::string&& name, Reference<Expression>&& type, Reference<Expression>&& expression): Statement(TYPE_ID), name(std::move(name)), type(std::move(type)), expression(std::move(expression)) {}
	const std::string& get_name() const {
		return name;
	}
	const Expression* get_type() const {
		return type;
	}
	const Expression* get_expression() const {
		return expression;
	}
};

class IfStatement final: public Statement {
	Reference<Expression> condition;
	Reference<Statement> then_statement;
	Reference<Statement> else_statement;
public:
	static constexpr int TYPE_ID = TYPE_ID_IF_STATEMENT;
	IfStatement(Reference<Expression>&& condition, Reference<Statement>&& then_statement, Reference<Statement>&& else_statement): Statement(TYPE_ID), condition(std::move(condition)), then_statement(std::move(then_statement)), else_statement(std::move(else_statement)) {}
	const Expression* get_condition() const {
		return condition;
	}
	const Statement* get_then_statement() const {
		return then_statement;
	}
	const Statement* get_else_statement() const {
		return else_statement;
	}
};

class WhileStatement final: public Statement {
	Reference<Expression> condition;
	Reference<Statement> statement;
public:
	static constexpr int TYPE_ID = TYPE_ID_WHILE_STATEMENT;
	WhileStatement(Reference<Expression>&& condition, Reference<Statement>&& statement): Statement(TYPE_ID), condition(std::move(condition)), statement(std::move(statement)) {}
	const Expression* get_condition() const {
		return condition;
	}
	const Statement* get_statement() const {
		return statement;
	}
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

class Function final: public Dynamic {
public:
	class Argument {
		std::string name;
		Reference<Expression> type;
	public:
		Argument(std::string&& name, Reference<Expression>&& type): name(std::move(name)), type(std::move(type)) {}
		const std::string& get_name() const {
			return name;
		}
		const Expression* get_type() const {
			return type;
		}
	};
private:
	std::string name;
	std::vector<Argument> arguments;
	Reference<Expression> return_type;
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_FUNCTION;
	Function(std::string&& name, std::vector<Argument>&& arguments, Reference<Expression>&& return_type, Block&& block): Dynamic(TYPE_ID), name(std::move(name)), arguments(std::move(arguments)), return_type(std::move(return_type)), block(std::move(block)) {}
	const std::string& get_name() const {
		return name;
	}
	const std::vector<Argument>& get_arguments() const {
		return arguments;
	}
	const Expression* get_return_type() const {
		return return_type;
	}
	const Block* get_block() const {
		return &block;
	}
};

class Structure final: public Type {
public:
	class Member {
		std::string name;
		Reference<Expression> type;
	public:
		Member(std::string&& name, Reference<Expression>&& type): name(std::move(name)), type(std::move(type)) {}
		const std::string& get_name() const {
			return name;
		}
		const Expression* get_type() const {
			return type;
		}
	};
private:
	std::string name;
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCTURE;
	Structure(std::string&& name, std::vector<Member>&& members): Type(TYPE_ID), name(std::move(name)), members(std::move(members)) {}
	const std::string& get_name() const {
		return name;
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Program final: public Dynamic {
	std::string path;
	std::vector<Reference<Function>> functions;
	std::vector<Reference<Structure>> structures;
	std::vector<Reference<Type>> types;
public:
	static constexpr int TYPE_ID = TYPE_ID_PROGRAM;
	Program(std::vector<Reference<Function>>&& functions, std::vector<Reference<Structure>>&& structures): Dynamic(TYPE_ID), functions(std::move(functions)), structures(std::move(structures)) {}
	void set_path(const char* path) {
		this->path = path;
	}
	const std::string& get_path() const {
		return path;
	}
	const std::vector<Reference<Function>>& get_functions() const {
		return functions;
	}
	const std::vector<Reference<Structure>>& get_structures() const {
		return structures;
	}
	template <class T, class... A> T* add_type(A&&... a) {
		T* type = new T(std::forward<A>(a)...);
		types.emplace_back(type);
		return type;
	}
	const std::vector<Reference<Type>>& get_types() const {
		return types;
	}
};
