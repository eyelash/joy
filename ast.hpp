#pragma once

#include "parsley/common.hpp"
#include "parsley/printer.hpp"
#include <algorithm>

enum {
	TYPE_ID_INVALID,
	TYPE_ID_INT_LITERAL,
	TYPE_ID_CHAR_LITERAL,
	TYPE_ID_STRING_LITERAL,
	TYPE_ID_TUPLE_LITERAL,
	TYPE_ID_STRUCT_LITERAL,
	TYPE_ID_NAME,
	TYPE_ID_VARIABLE,
	TYPE_ID_ASSIGNMENT,
	TYPE_ID_SPREAD,
	TYPE_ID_CALL,
	TYPE_ID_ACCESSOR,
	TYPE_ID_ENTITY_REFERENCE,
	TYPE_ID_BLOCK_STATEMENT,
	TYPE_ID_LET_STATEMENT,
	TYPE_ID_IF_STATEMENT,
	TYPE_ID_WHILE_STATEMENT,
	TYPE_ID_FOR_STATEMENT,
	TYPE_ID_RETURN_STATEMENT,
	TYPE_ID_BREAK_STATEMENT,
	TYPE_ID_CONTINUE_STATEMENT,
	TYPE_ID_EXPRESSION_STATEMENT,
	TYPE_ID_DESTROY_STATEMENT,
	TYPE_ID_IMPORT,
	TYPE_ID_BUILTIN_FUNCTION,
	TYPE_ID_FUNCTION,
	TYPE_ID_BUILTIN_TYPE,
	TYPE_ID_STRUCTURE,
	TYPE_ID_ENUMERATION,
	TYPE_ID_PROGRAM
};

class Entity: public Dynamic {
	unsigned int id;
	Path path;
public:
	Entity(int type_id): Dynamic(type_id), id(0) {}
	void set_id(unsigned int id) {
		this->id = id;
	}
	unsigned int get_id() const {
		return id;
	}
	void set_path(const char* path) {
		this->path = path;
	}
	const Path& get_path() const {
		return path;
	}
};

class Expression: public Dynamic {
	SourceLocation location;
public:
	Expression(int type_id): Dynamic(type_id) {}
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
	const std::int32_t& get_value() const {
		return value;
	}
};

class CharLiteral final: public Expression {
	std::string string;
public:
	static constexpr int TYPE_ID = TYPE_ID_CHAR_LITERAL;
	CharLiteral(std::string&& string): Expression(TYPE_ID), string(std::move(string)) {}
	StringView get_string() const {
		return string;
	}
};

class StringLiteral final: public Expression {
	std::string string;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRING_LITERAL;
	StringLiteral(std::string&& string): Expression(TYPE_ID), string(std::move(string)) {}
	StringView get_string() const {
		return string;
	}
};

class TupleLiteral final: public Expression {
	std::vector<Reference<Expression>> elements;
public:
	static constexpr int TYPE_ID = TYPE_ID_TUPLE_LITERAL;
	TupleLiteral(std::vector<Reference<Expression>>&& elements): Expression(TYPE_ID), elements(std::move(elements)) {}
	const std::vector<Reference<Expression>>& get_elements() const {
		return elements;
	}
};

class StructLiteral final: public Expression {
public:
	class Member {
		std::string name;
		Reference<Expression> expression;
	public:
		Member(std::string&& name, Reference<Expression>&& expression): name(std::move(name)), expression(std::move(expression)) {}
		StringView get_name() const {
			return name;
		}
		const Expression* get_expression() const {
			return expression;
		}
	};
private:
	Reference<Expression> type;
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCT_LITERAL;
	StructLiteral(Reference<Expression>&& type, std::vector<Member>&& members): Expression(TYPE_ID), type(std::move(type)), members(std::move(members)) {}
	const Expression* get_type() const {
		return type;
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Name final: public Expression {
	std::string name;
public:
	static constexpr int TYPE_ID = TYPE_ID_NAME;
	Name(std::string&& name): Expression(TYPE_ID), name(std::move(name)) {}
	StringView get_name() const {
		return name;
	}
};

class Variable final: public Expression {
	unsigned int index;
public:
	static constexpr int TYPE_ID = TYPE_ID_VARIABLE;
	Variable(unsigned int index): Expression(TYPE_ID), index(index) {}
	unsigned int get_index() const {
		return index;
	}
};

#define DEFINE_OPERATOR(name) struct Operator_##name { static constexpr const char* function_name = #name; };

DEFINE_OPERATOR(add)
DEFINE_OPERATOR(subtract)
DEFINE_OPERATOR(multiply)
DEFINE_OPERATOR(divide)
DEFINE_OPERATOR(remainder)
DEFINE_OPERATOR(negate)
DEFINE_OPERATOR(equal)
DEFINE_OPERATOR(not_equal)
DEFINE_OPERATOR(less_than)
DEFINE_OPERATOR(less_than_or_equal)
DEFINE_OPERATOR(greater_than)
DEFINE_OPERATOR(greater_than_or_equal)

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

class Spread final: public Expression {
	Reference<Expression> expression;
public:
	static constexpr int TYPE_ID = TYPE_ID_SPREAD;
	Spread(Reference<Expression>&& expression): Expression(TYPE_ID), expression(std::move(expression)) {}
	const Expression* get_expression() const {
		return expression;
	}
};

class Call final: public Expression {
	Reference<Expression> expression;
	std::vector<Reference<Expression>> arguments;
public:
	static constexpr int TYPE_ID = TYPE_ID_CALL;
	Call(Reference<Expression>&& expression, std::vector<Reference<Expression>>&& arguments): Expression(TYPE_ID), expression(std::move(expression)), arguments(std::move(arguments)) {}
	Call(const char* name, std::vector<Reference<Expression>>&& arguments): Expression(TYPE_ID), expression(new Name(name)), arguments(std::move(arguments)) {}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<Reference<Expression>>& get_arguments() const {
		return arguments;
	}
};

class Accessor final: public Expression {
	Reference<Expression> left;
	Reference<Expression> right;
public:
	static constexpr int TYPE_ID = TYPE_ID_ACCESSOR;
	Accessor(Reference<Expression>&& left, Reference<Expression>&& right): Expression(TYPE_ID), left(std::move(left)), right(std::move(right)) {}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
};

class EntityReference final: public Expression {
	Entity* entity;
public:
	static constexpr int TYPE_ID = TYPE_ID_ENTITY_REFERENCE;
	EntityReference(Entity* entity): Expression(TYPE_ID), entity(entity) {}
	Entity* get_entity() const {
		return entity;
	}
};

class Statement: public Dynamic {
	SourceLocation location;
public:
	Statement(int type_id): Dynamic(type_id) {}
	void set_location(const SourceLocation& location) {
		this->location = location;
	}
	const SourceLocation& get_location() const {
		return location;
	}
};

class Block {
	std::vector<Reference<Statement>> statements;
public:
	Block() {}
	Block(std::vector<Reference<Statement>>&& statements): statements(std::move(statements)) {}
	Block(Reference<Statement>&& statement) {
		if (statement) {
			statements.push_back(std::move(statement));
		}
	}
	const std::vector<Reference<Statement>>& get_statements() const {
		return statements;
	}
	void add_statement(Reference<Statement>&& statement) {
		statements.push_back(std::move(statement));
	}
	Statement* get_last_statement() const {
		if (statements.empty()) {
			return nullptr;
		}
		return statements.back();
	}
};

class BlockStatement final: public Statement {
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_BLOCK_STATEMENT;
	BlockStatement(): Statement(TYPE_ID) {}
	BlockStatement(Block&& block): Statement(TYPE_ID), block(std::move(block)) {}
	Block* get_block() {
		return &block;
	}
	const Block* get_block() const {
		return &block;
	}
};

class LetStatement final: public Statement {
	Reference<Expression> variable;
	Reference<Expression> type;
	Reference<Expression> expression;
public:
	static constexpr int TYPE_ID = TYPE_ID_LET_STATEMENT;
	LetStatement(Reference<Expression>&& variable, Reference<Expression>&& type, Reference<Expression>&& expression): Statement(TYPE_ID), variable(std::move(variable)), type(std::move(type)), expression(std::move(expression)) {}
	const Expression* get_variable() const {
		return variable;
	}
	Reference<Expression>& get_type() {
		return type;
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
	Block then_block;
	Block else_block;
public:
	static constexpr int TYPE_ID = TYPE_ID_IF_STATEMENT;
	IfStatement(Reference<Expression>&& condition, Block&& then_block, Block&& else_block): Statement(TYPE_ID), condition(std::move(condition)), then_block(std::move(then_block)), else_block(std::move(else_block)) {}
	const Expression* get_condition() const {
		return condition;
	}
	Block* get_then_block() {
		return &then_block;
	}
	Block* get_else_block() {
		return &else_block;
	}
	const Block* get_then_block() const {
		return &then_block;
	}
	const Block* get_else_block() const {
		return &else_block;
	}
};

class WhileStatement final: public Statement {
	Reference<Expression> condition;
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_WHILE_STATEMENT;
	WhileStatement(Reference<Expression>&& condition, Block&& block): Statement(TYPE_ID), condition(std::move(condition)), block(std::move(block)) {}
	const Expression* get_condition() const {
		return condition;
	}
	Block* get_block() {
		return &block;
	}
	const Block* get_block() const {
		return &block;
	}
};

class ForStatement final: public Statement {
	std::string variable;
	Reference<Expression> expression;
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_FOR_STATEMENT;
	ForStatement(std::string&& variable, Reference<Expression>&& expression, Block&& block): Statement(TYPE_ID), variable(std::move(variable)), expression(std::move(expression)), block(std::move(block)) {}
	StringView get_variable() const {
		return variable;
	}
	const Expression* get_expression() const {
		return expression;
	}
	const Block* get_block() const {
		return &block;
	}
};

class ReturnStatement final: public Statement {
	Reference<Expression> expression;
	std::vector<unsigned int> destroy_variables;
public:
	static constexpr int TYPE_ID = TYPE_ID_RETURN_STATEMENT;
	ReturnStatement(Reference<Expression>&& expression): Statement(TYPE_ID), expression(std::move(expression)) {}
	ReturnStatement(): Statement(TYPE_ID) {}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<unsigned int>& get_destroy_variables() const {
		return destroy_variables;
	}
	void add_destroy_variable(unsigned int variable) {
		destroy_variables.push_back(variable);
	}
};

class BreakStatement final: public Statement {
public:
	static constexpr int TYPE_ID = TYPE_ID_BREAK_STATEMENT;
	BreakStatement(): Statement(TYPE_ID) {}
};

class ContinueStatement final: public Statement {
public:
	static constexpr int TYPE_ID = TYPE_ID_CONTINUE_STATEMENT;
	ContinueStatement(): Statement(TYPE_ID) {}
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

class DestroyStatement final: public Statement {
	unsigned int index;
public:
	static constexpr int TYPE_ID = TYPE_ID_DESTROY_STATEMENT;
	DestroyStatement(unsigned int index): Statement(TYPE_ID), index(index) {}
	unsigned int get_index() const {
		return index;
	}
};

class Import final: public Entity {
	Path path;
public:
	static constexpr int TYPE_ID = TYPE_ID_IMPORT;
	Import(std::string&& path): Entity(TYPE_ID), path(std::move(path)) {}
	const Path& get_path() const {
		return path;
	}
};

class Argument {
	std::string name;
	Reference<Expression> type;
public:
	Argument(std::string&& name, Reference<Expression>&& type): name(std::move(name)), type(std::move(type)) {}
	StringView get_name() const {
		return name;
	}
	Reference<Expression>& get_type() {
		return type;
	}
	const Expression* get_type() const {
		return type;
	}
};

class SignatureEntity: public Entity {
	std::string name;
	std::vector<std::string> template_arguments;
	std::vector<Argument> arguments;
	Reference<Expression> return_type;
public:
	SignatureEntity(int type_id, std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type): Entity(type_id), name(std::move(name)), template_arguments(std::move(template_arguments)), arguments(std::move(arguments)), return_type(std::move(return_type)) {}
	StringView get_name() const {
		return name;
	}
	const std::vector<std::string>& get_template_arguments() const {
		return template_arguments;
	}
	std::vector<Argument>& get_arguments() {
		return arguments;
	}
	const std::vector<Argument>& get_arguments() const {
		return arguments;
	}
	Reference<Expression>& get_return_type() {
		return return_type;
	}
	const Expression* get_return_type() const {
		return return_type;
	}
};

class BuiltinFunction final: public SignatureEntity {
public:
	static constexpr int TYPE_ID = TYPE_ID_BUILTIN_FUNCTION;
	BuiltinFunction(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type): SignatureEntity(TYPE_ID, std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type)) {}
};

class Function final: public SignatureEntity {
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_FUNCTION;
	Function(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type, Block&& block): SignatureEntity(TYPE_ID, std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type)), block(std::move(block)) {}
	const Block* get_block() const {
		return &block;
	}
};

class BuiltinType final: public SignatureEntity {
public:
	static constexpr int TYPE_ID = TYPE_ID_BUILTIN_TYPE;
	BuiltinType(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type): SignatureEntity(TYPE_ID, std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type)) {}
};

class Member {
	std::string name;
	Reference<Expression> type;
public:
	Member(std::string&& name, Reference<Expression>&& type): name(std::move(name)), type(std::move(type)) {}
	StringView get_name() const {
		return name;
	}
	Reference<Expression>& get_type() {
		return type;
	}
	const Expression* get_type() const {
		return type;
	}
};

class Structure final: public SignatureEntity {
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCTURE;
	Structure(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type, std::vector<Member>&& members): SignatureEntity(TYPE_ID, std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type)), members(std::move(members)) {}
	std::vector<Member>& get_members() {
		return members;
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Enumeration final: public SignatureEntity {
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_ENUMERATION;
	Enumeration(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type, std::vector<Member>&& members): SignatureEntity(TYPE_ID, std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type)), members(std::move(members)) {}
	std::vector<Member>& get_members() {
		return members;
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Program final: public Dynamic {
	std::vector<Reference<Entity>> entities;
	unsigned int current_id = 0;
	const Entity* main_function = nullptr;
	class IdCompare {
	public:
		constexpr IdCompare() {}
		template <class T> bool operator ()(const Reference<T>& t, unsigned int id) const {
			return t->get_id() < id;
		}
	};
public:
	static constexpr int TYPE_ID = TYPE_ID_PROGRAM;
	Program(): Dynamic(TYPE_ID) {}
	void add_entity(Reference<Entity>&& entity) {
		entity->set_id(get_next_id());
		entities.push_back(std::move(entity));
	}
	const std::vector<Reference<Entity>>& get_entities() const {
		return entities;
	}
	const Entity* get_entity_by_id(unsigned int id) const {
		auto iter = std::lower_bound(entities.begin(), entities.end(), id, IdCompare());
		if (iter != entities.end() && (*iter)->get_id() == id) {
			return *iter;
		}
		return nullptr;
	}
	unsigned int get_next_id() {
		++current_id;
		return current_id;
	}
	void set_main_function(const Entity* main_function) {
		this->main_function = main_function;
	}
	const Entity* get_main_function() const {
		return main_function;
	}
};

template <class P, class I, class = P> class PrintCommaSeparated {
	I first;
	I last;
public:
	PrintCommaSeparated(I first, I last): first(first), last(last) {}
	void print(printer::Context& context) const {
		I i = first;
		if (i != last) {
			print_impl(P(*i), context);
			++i;
			while (i != last) {
				print_impl(", ", context);
				print_impl(P(*i), context);
				++i;
			}
		}
	}
};
template <class P, class I> class PrintCommaSeparated<P, I, decltype(P(*std::declval<I>(), std::declval<unsigned int>()))> {
	I first;
	I last;
public:
	PrintCommaSeparated(I first, I last): first(first), last(last) {}
	void print(printer::Context& context) const {
		I i = first;
		unsigned int index = 0;
		if (i != last) {
			print_impl(P(*i, index), context);
			++i;
			++index;
			while (i != last) {
				print_impl(", ", context);
				print_impl(P(*i, index), context);
				++i;
				++index;
			}
		}
	}
};
template <class P, class I> PrintCommaSeparated<P, I> comma_separated(I first, I last) {
	return PrintCommaSeparated<P, I>(first, last);
}
template <class P, class I> PrintCommaSeparated<P, I> comma_separated(const Range<I>& range) {
	return PrintCommaSeparated<P, I>(range.begin(), range.end());
}
template <class P, class T> auto comma_separated(const std::vector<T>& v) {
	return comma_separated<P>(v.begin(), v.end());
}
