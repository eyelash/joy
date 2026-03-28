#pragma once

#include "parsley/common.hpp"
#include "parsley/printer.hpp"

class Error {
	std::string path;
	SourceLocation location;
	std::string message;
public:
	Error(const char* path, const SourceLocation& location, std::string&& message): path(path), location(location), message(std::move(message)) {}
	template <class Type> void print() const {
		using namespace printer;
		Context context(std::cerr);
		if (location) {
			auto source = read_file(path.c_str());
			print_diagnostic<Type>(context, path, StringView(source.data(), source.size()), location, StringView(message));
		}
		else {
			print_diagnostic<Type>(context, path, StringView(message));
		}
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
	template <class P> void add_error(const char* path, const SourceLocation& location, P&& p) {
		errors.emplace_back(path, location, print_to_string(std::forward<P>(p)));
	}
	template <class P> void add_warning(const char* path, const SourceLocation& location, P&& p) {
		warnings.emplace_back(path, location, print_to_string(std::forward<P>(p)));
	}
	void print() {
		for (const Error& error: warnings) {
			error.print<printer::DiagnosticType::Warning>();
		}
		for (const Error& error: errors) {
			error.print<printer::DiagnosticType::Error>();
		}
	}
};

enum {
	TYPE_ID_INVALID,
	TYPE_ID_VOID_TYPE,
	TYPE_ID_INT_TYPE,
	TYPE_ID_STRING_TYPE,
	TYPE_ID_ARRAY_TYPE,
	TYPE_ID_TUPLE_TYPE,
	TYPE_ID_INT_LITERAL,
	TYPE_ID_CHAR_LITERAL,
	TYPE_ID_STRING_LITERAL,
	TYPE_ID_ARRAY_LITERAL,
	TYPE_ID_STRUCT_LITERAL,
	TYPE_ID_NAME,
	TYPE_ID_BINARY_EXPRESSION,
	TYPE_ID_ASSIGNMENT,
	TYPE_ID_CALL,
	TYPE_ID_ACCESSOR,
	TYPE_ID_ENTITY_REFERENCE,
	TYPE_ID_BLOCK_STATEMENT,
	TYPE_ID_EMPTY_STATEMENT,
	TYPE_ID_LET_STATEMENT,
	TYPE_ID_IF_STATEMENT,
	TYPE_ID_WHILE_STATEMENT,
	TYPE_ID_RETURN_STATEMENT,
	TYPE_ID_EXPRESSION_STATEMENT,
	TYPE_ID_FUNCTION,
	TYPE_ID_STRUCTURE,
	TYPE_ID_BUILTIN_FUNCTION,
	TYPE_ID_FUNCTION_INSTANTIATION,
	TYPE_ID_STRUCTURE_INSTANTIATION,
	TYPE_ID_PROGRAM
};

class Entity: public Dynamic {
	unsigned int id;
public:
	Entity(int type_id): Dynamic(type_id), id(0) {}
	void set_id(unsigned int id) {
		this->id = id;
	}
	unsigned int get_id() const {
		return id;
	}
};

class Type: public Entity {
public:
	Type(int type_id): Entity(type_id) {}
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

class StringType final: public Type {
public:
	static constexpr int TYPE_ID = TYPE_ID_STRING_TYPE;
	StringType(): Type(TYPE_ID) {}
};

class ArrayType final: public Type {
	const Type* element_type;
public:
	static constexpr int TYPE_ID = TYPE_ID_ARRAY_TYPE;
	ArrayType(const Type* element_type): Type(TYPE_ID), element_type(element_type) {}
	const Type* get_element_type() const {
		return element_type;
	}
};

class TupleType final: public Type {
	std::vector<const Type*> element_types;
public:
	static constexpr int TYPE_ID = TYPE_ID_TUPLE_TYPE;
	TupleType(std::vector<const Type*>&& element_types): Type(TYPE_ID), element_types(std::move(element_types)) {}
	const std::vector<const Type*>& get_element_types() const {
		return element_types;
	}
};

class Expression: public Dynamic {
	SourceLocation location;
	const Type* type;
public:
	Expression(int type_id): Dynamic(type_id), type(nullptr) {}
	Expression(int type_id, const Type* type): Dynamic(type_id), type(type) {}
	void set_location(const SourceLocation& location) {
		this->location = location;
	}
	const SourceLocation& get_location() const {
		return location;
	}
	void set_type(const Type* type) {
		this->type = type;
	}
	const Type* get_type() const {
		return type;
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

class ArrayLiteral final: public Expression {
	std::vector<Reference<Expression>> elements;
public:
	static constexpr int TYPE_ID = TYPE_ID_ARRAY_LITERAL;
	ArrayLiteral(std::vector<Reference<Expression>>&& elements): Expression(TYPE_ID), elements(std::move(elements)) {}
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
	const Entity* entity;
public:
	static constexpr int TYPE_ID = TYPE_ID_ENTITY_REFERENCE;
	EntityReference(const Entity* entity): Expression(TYPE_ID), entity(entity) {}
	const Entity* get_entity() const {
		return entity;
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
	Block(Reference<Statement>&& statement) {
		if (statement) {
			statements.push_back(std::move(statement));
		}
	}
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
	StringView get_name() const {
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
	Block then_block;
	Block else_block;
public:
	static constexpr int TYPE_ID = TYPE_ID_IF_STATEMENT;
	IfStatement(Reference<Expression>&& condition, Block&& then_block, Block&& else_block): Statement(TYPE_ID), condition(std::move(condition)), then_block(std::move(then_block)), else_block(std::move(else_block)) {}
	IfStatement(Reference<Expression>&& condition, Reference<Statement>&& then_statement, Reference<Statement>&& else_statement): Statement(TYPE_ID), condition(std::move(condition)), then_block(std::move(then_statement)), else_block(std::move(else_statement)) {}
	const Expression* get_condition() const {
		return condition;
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
	WhileStatement(Reference<Expression>&& condition, Reference<Statement>&& statement): Statement(TYPE_ID), condition(std::move(condition)), block(std::move(statement)) {}
	const Expression* get_condition() const {
		return condition;
	}
	const Block* get_block() const {
		return &block;
	}
};

class ReturnStatement final: public Statement {
	Reference<Expression> expression;
public:
	static constexpr int TYPE_ID = TYPE_ID_RETURN_STATEMENT;
	ReturnStatement(Reference<Expression>&& expression): Statement(TYPE_ID), expression(std::move(expression)) {}
	ReturnStatement(): Statement(TYPE_ID) {}
	const Expression* get_expression() const {
		return expression;
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
		StringView get_name() const {
			return name;
		}
		const Expression* get_type() const {
			return type;
		}
	};
private:
	std::string name;
	std::vector<std::string> template_arguments;
	std::vector<Argument> arguments;
	Reference<Expression> return_type;
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_FUNCTION;
	Function(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Argument>&& arguments, Reference<Expression>&& return_type, Block&& block): Dynamic(TYPE_ID), name(std::move(name)), template_arguments(std::move(template_arguments)), arguments(std::move(arguments)), return_type(std::move(return_type)), block(std::move(block)) {
		if (this->return_type == nullptr) {
			this->return_type = new Name("Void");
		}
	}
	StringView get_name() const {
		return name;
	}
	const std::vector<std::string>& get_template_arguments() const {
		return template_arguments;
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

class Structure final: public Dynamic {
public:
	class Member {
		std::string name;
		Reference<Expression> type;
	public:
		Member(std::string&& name, Reference<Expression>&& type): name(std::move(name)), type(std::move(type)) {}
		StringView get_name() const {
			return name;
		}
		const Expression* get_type() const {
			return type;
		}
	};
private:
	std::string name;
	std::vector<std::string> template_arguments;
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCTURE;
	Structure(std::string&& name, std::vector<std::string>&& template_arguments, std::vector<Member>&& members): Dynamic(TYPE_ID), name(std::move(name)), template_arguments(std::move(template_arguments)), members(std::move(members)) {}
	StringView get_name() const {
		return name;
	}
	const std::vector<std::string>& get_template_arguments() const {
		return template_arguments;
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class BuiltinFunction final: public Entity {
	const char* name;
public:
	static constexpr int TYPE_ID = TYPE_ID_BUILTIN_FUNCTION;
	BuiltinFunction(const char* name): Entity(TYPE_ID), name(name) {}
	StringView get_name() const {
		return name;
	}
};

class FunctionInstantiation final: public Entity {
public:
	class Argument {
		StringView name;
		const Type* type;
	public:
		Argument(const StringView& name, const Type* type): name(name), type(type) {}
		StringView get_name() const {
			return name;
		}
		const Type* get_type() const {
			return type;
		}
	};
private:
	const Function* function;
	std::vector<const Type*> template_arguments;
	std::vector<Argument> arguments;
	const Type* return_type;
	Block block;
public:
	static constexpr int TYPE_ID = TYPE_ID_FUNCTION_INSTANTIATION;
	FunctionInstantiation(const Function* function): Entity(TYPE_ID), function(function) {}
	const Function* get_function() const {
		return function;
	}
	void add_template_argument(const Type* type) {
		template_arguments.push_back(type);
	}
	const std::vector<const Type*>& get_template_arguments() const {
		return template_arguments;
	}
	void add_argument(const StringView& name, const Type* type) {
		arguments.emplace_back(name, type);
	}
	const std::vector<Argument>& get_arguments() const {
		return arguments;
	}
	void set_return_type(const Type* return_type) {
		this->return_type = return_type;
	}
	const Type* get_return_type() const {
		return return_type;
	}
	void set_block(Block&& block) {
		this->block = std::move(block);
	}
	const Block* get_block() const {
		return &block;
	}
};

class StructureInstantiation final: public Type {
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
	const Structure* structure;
	std::vector<const Type*> template_arguments;
	std::vector<Member> members;
public:
	static constexpr int TYPE_ID = TYPE_ID_STRUCTURE_INSTANTIATION;
	StructureInstantiation(const Structure* structure): Type(TYPE_ID), structure(structure) {}
	const Structure* get_structure() const {
		return structure;
	}
	void add_template_argument(const Type* type) {
		template_arguments.push_back(type);
	}
	const std::vector<const Type*>& get_template_arguments() const {
		return template_arguments;
	}
	void add_member(const StringView& name, const Type* type) {
		members.emplace_back(name, type);
	}
	const std::vector<Member>& get_members() const {
		return members;
	}
};

class Program final: public Dynamic {
	std::string path;
	std::vector<Reference<Function>> functions;
	std::vector<Reference<Structure>> structures;
	std::vector<Reference<Entity>> entities;
	unsigned int current_id = 0;
	const Entity* main_function = nullptr;
public:
	static constexpr int TYPE_ID = TYPE_ID_PROGRAM;
	Program(): Dynamic(TYPE_ID) {}
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
	void add_entity(Reference<Entity>&& entity) {
		entities.push_back(std::move(entity));
	}
	const std::vector<Reference<Entity>>& get_entities() const {
		return entities;
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

template <class P, class T> class PrintCommaSeparated {
	const std::vector<T>& v;
public:
	constexpr PrintCommaSeparated(const std::vector<T>& v): v(v) {}
	void print(printer::Context& context) const {
		auto i = v.begin();
		auto end = v.end();
		if (i != end) {
			print_impl(P(*i), context);
			++i;
			while (i != end) {
				print_impl(", ", context);
				print_impl(P(*i), context);
				++i;
			}
		}
	}
};
template <class P, class T> constexpr PrintCommaSeparated<P, T> comma_separated(const std::vector<T>& v) {
	return PrintCommaSeparated<P, T>(v);
}

class PrintTypeName {
	const Type* type;
public:
	constexpr PrintTypeName(const Type* type): type(type) {}
	void print(printer::Context& context) const {
		if (as<VoidType>(type)) {
			print_impl("Void", context);
		}
		else if (as<IntType>(type)) {
			print_impl("Int", context);
		}
		else if (as<StringType>(type)) {
			print_impl("String", context);
		}
		else if (auto* t = as<ArrayType>(type)) {
			print_impl(printer::format("Array<%>", PrintTypeName(t->get_element_type())), context);
		}
		else if (auto* t = as<TupleType>(type)) {
			print_impl(printer::format("Tuple<%>", comma_separated<PrintTypeName>(t->get_element_types())), context);
		}
		else if (auto* s = as<StructureInstantiation>(type)) {
			const StringView name = s->get_structure()->get_name();
			const std::vector<const Type*>& template_arguments = s->get_template_arguments();
			print_impl(name, context);
			if (!template_arguments.empty()) {
				print_impl(printer::format("<%>", comma_separated<PrintTypeName>(template_arguments)), context);
			}
		}
	}
};
