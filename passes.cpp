#include "passes.hpp"
#include <algorithm>
#include <map>

class ScopeMap {
	ScopeMap* parent;
	std::map<StringView, const Type*> map;
public:
	ScopeMap(ScopeMap* parent = nullptr): parent(parent) {}
	ScopeMap* get_parent() const {
		return parent;
	}
	void insert(const StringView& name, const Type* value) {
		if (value) {
			map.emplace(name, value);
		}
	}
	const Type* look_up(const StringView& name) const {
		auto iterator = map.find(name);
		if (iterator != map.end()) {
			return iterator->second;
		}
		if (parent) {
			return parent->look_up(name);
		}
		return nullptr;
	}
};

template <class T> class FlatMap {
	using Entry = std::pair<StringView, const T*>;
	std::vector<Entry> entries;
	struct Compare {
		constexpr Compare() {}
		bool operator ()(const StringView& name, const Entry& entry) const {
			return name < entry.first;
		}
		bool operator ()(const Entry& entry, const StringView& name) const {
			return entry.first < name;
		}
	};
public:
	void add_entry(const StringView& name, const T* value) {
		entries.emplace_back(name, value);
	}
	void sort() {
		std::sort(entries.begin(), entries.end());
	}
	const T* look_up(const StringView& name) const {
		const auto pair = std::equal_range(entries.begin(), entries.end(), name, Compare());
		if (pair.second - pair.first == 1) {
			return pair.first->second;
		}
		return nullptr;
	}
};

class Interner {
public:
	class Key {
		std::pair<StringView, std::vector<const Type*>> key;
	public:
		Key(const StringView& name): key(name, std::vector<const Type*>()) {}
		Key(const char* name): Key(StringView(name)) {}
		Key(const std::string& name): Key(StringView(name)) {}
		bool operator <(const Key& key) const {
			return this->key < key.key;
		}
		StringView get_name() const {
			return key.first;
		}
		const std::vector<const Type*>& get_arguments() const {
			return key.second;
		}
		void add_argument(const Type* argument) {
			key.second.push_back(argument);
		}
	};
private:
	std::map<Key, const Type*> types;
public:
	void insert(Key&& key, const Type* type) {
		// TODO: does this move actually work?
		types.emplace(std::move(key), type);
	}
	const Type* look_up(const Key& key) const {
		auto iterator = types.find(key);
		if (iterator != types.end()) {
			return iterator->second;
		}
		return nullptr;
	}
};

template <class T, class TI = T> class Instantiations {
public:
	class Key {
		std::pair<const T*, std::vector<const Type*>> key;
	public:
		// TODO: does this move actually work?
		Key(const T* t, std::vector<const Type*>&& arguments): key(t, std::move(arguments)) {}
		bool operator <(const Key& key) const {
			return this->key < key.key;
		}
		const std::vector<const Type*>& get_arguments() const {
			return key.second;
		}
	};
private:
	std::map<Key, const TI*> instantiations;
public:
	void insert(Key&& key, const TI* t) {
		// TODO: does this move actually work?
		instantiations.emplace(std::move(key), t);
	}
	const TI* look_up(const Key& key) const {
		auto iterator = instantiations.find(key);
		if (iterator != instantiations.end()) {
			return iterator->second;
		}
		return nullptr;
	}
};

class Copy {
public:
	static Reference<Expression> copy_expression_(const Expression* expression) {
		if (expression == nullptr) {
			return Reference<Expression>();
		}
		if (auto* e = as<IntLiteral>(expression)) {
			return new IntLiteral(e->get_value());
		}
		else if (auto* e = as<Name>(expression)) {
			return new Name(e->get_name().to_string());
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			return new BinaryExpression(e->get_operation(), copy_expression(e->get_left()), copy_expression(e->get_right()));
		}
		else if (auto* e = as<Assignment>(expression)) {
			return new Assignment(copy_expression(e->get_left()), copy_expression(e->get_right()));
		}
		else if (auto* e = as<Call>(expression)) {
			return new Call(copy_expression(e->get_expression()), copy_expressions(e->get_arguments()));
		}
		else if (auto* e = as<MemberAccess>(expression)) {
			return new MemberAccess(copy_expression(e->get_expression()), e->get_member_name().to_string());
		}
	}
	static Reference<Expression> copy_expression(const Expression* expression) {
		Reference<Expression> new_expression = copy_expression_(expression);
		new_expression->set_location(expression->get_location());
		new_expression->set_type(expression->get_type());
		return new_expression;
	}
	static std::vector<Reference<Expression>> copy_expressions(const std::vector<Reference<Expression>>& expressions) {
		std::vector<Reference<Expression>> new_expressions;
		for (const Expression* expression: expressions) {
			new_expressions.push_back(copy_expression(expression));
		}
		return new_expressions;
	}
	static Block copy_block(const Block* block) {
		std::vector<Reference<Statement>> statements;
		for (const Statement* statement: block->get_statements()) {
			statements.push_back(copy_statement(statement));
		}
		return Block(std::move(statements));
	}
	static Reference<Statement> copy_statement(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			return new BlockStatement(copy_block(s->get_block()));
		}
		else if (auto* s = as<EmptyStatement>(statement)) {
			return new EmptyStatement();
		}
		else if (auto* s = as<LetStatement>(statement)) {
			return new LetStatement(s->get_name().to_string(), copy_expression(s->get_type()), copy_expression(s->get_expression()));
		}
		else if (auto* s = as<IfStatement>(statement)) {
			Reference<Expression> condition = copy_expression(s->get_condition());
			Reference<Statement> then_statement = copy_statement(s->get_then_statement());
			Reference<Statement> else_statement = copy_statement(s->get_else_statement());
			return new IfStatement(std::move(condition), std::move(then_statement), std::move(else_statement));
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			Reference<Expression> condition = copy_expression(s->get_condition());
			Reference<Statement> statement = copy_statement(s->get_statement());
			return new WhileStatement(std::move(condition), std::move(statement));
		}
		else if (auto* s = as<ReturnStatement>(statement)) {
			Reference<Expression> expression = copy_expression(s->get_expression());
			return new ReturnStatement(std::move(expression));
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			return new ExpressionStatement(copy_expression(s->get_expression()));
		}
	}
};

class Pass1 {
	static SourceLocation get_location(const Expression* expression) {
		if (expression == nullptr) {
			return SourceLocation(0, 0);
		}
		return expression->get_location();
	}
	static const Type* get_type(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		return expression->get_type();
	}
	class Unification {
		Pass1* pass1;
		const Function* function;
		const std::vector<Reference<Expression>>& arguments;
		const Type* return_type;
		std::vector<const Type*>& template_arguments;
		bool is_variable(const StringView& name) {
			for (StringView template_argument_name: function->get_template_arguments()) {
				if (name == template_argument_name) {
					return true;
				}
			}
			return false;
		}
		bool set_variable(const StringView& name, const Type* type) {
			for (std::size_t i = 0; i < template_arguments.size(); ++i) {
				if (name == function->get_template_arguments()[i]) {
					if (template_arguments[i]) {
						// variable already set
						return template_arguments[i] == type;
					}
					template_arguments[i] = type;
					return true;
				}
			}
			return false;
		}
		bool match(const Expression* function_argument, const Type* argument) {
			if (argument == nullptr) {
				return false;
			}
			if (auto* e = as<Name>(function_argument)) {
				if (is_variable(e->get_name())) {
					return set_variable(e->get_name(), argument);
				}
				else {
					const Type* type = pass1->get_type(e->get_name(), std::vector<const Type*>(), function_argument);
					return type && type == argument;
				}
			}
			else if (auto* e = as<Call>(function_argument)) {
				StringView name = pass1->get_name(e->get_expression());
				if (auto* s = as<StructureInstantiation>(argument)) {
					if (name != s->get_structure()->get_name()) {
						return false;
					}
					if (e->get_arguments().size() != s->get_template_arguments().size()) {
						return false;
					}
					for (std::size_t i = 0; i < e->get_arguments().size(); ++i) {
						if (!match(e->get_arguments()[i], s->get_template_arguments()[i])) {
							return false;
						}
					}
					return true;
				}
			}
			return false;
		}
	public:
		Unification(Pass1* pass1, const Function* function, const std::vector<Reference<Expression>>& arguments, const Type* return_type, std::vector<const Type*>& template_arguments): pass1(pass1), function(function), arguments(arguments), return_type(return_type), template_arguments(template_arguments) {}
		bool run() {
			if (function->get_arguments().size() != arguments.size()) {
				return false;
			}
			template_arguments.resize(function->get_template_arguments().size());
			std::fill(template_arguments.begin(), template_arguments.end(), nullptr);
			for (std::size_t i = 0; i < arguments.size(); ++i) {
				const Expression* function_argument = function->get_arguments()[i].get_type();
				if (!match(function_argument, get_type(arguments[i]))) {
					return false;
				}
			}
			if (return_type) {
				if (!match(function->get_return_type(), return_type)) {
					return false;
				}
			}
			for (const Type* argument: template_arguments) {
				if (argument == nullptr) {
					// not all arguments could be determined
					return false;
				}
			}
			return true;
		}
	};
	Program* program;
	Errors* errors;
	const Type* void_type = nullptr;
	const Type* int_type = nullptr;
	Instantiations<Structure, StructureInstantiation>* structure_instantiations;
	Instantiations<Function, FunctionInstantiation>* function_instantiations;
	ScopeMap* variables = nullptr;
	ScopeMap* type_variables = nullptr;
	template <class... T> void add_error(const Expression* expression, const char* s, T... t) {
		errors->add_error(program->get_path().c_str(), get_location(expression), printer::format(s, t...));
	}
	const Type* instantiate_structure(const Structure* structure, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != structure->get_template_arguments().size()) {
			return nullptr;
		}
		Instantiations<Structure, StructureInstantiation>::Key key(structure, std::move(template_arguments));
		if (const StructureInstantiation* new_structure = structure_instantiations->look_up(key)) {
			return new_structure;
		}
		StructureInstantiation* new_structure = new StructureInstantiation(structure);
		new_structure->set_id(program->get_next_id());
		// template arguments
		ScopeMap type_variables;
		for (std::size_t i = 0; i < key.get_arguments().size(); ++i) {
			new_structure->add_template_argument(key.get_arguments()[i]);
			type_variables.insert(structure->get_template_arguments()[i], key.get_arguments()[i]);
		}
		ScopeMap* previous_type_variables = this->type_variables;
		this->type_variables = &type_variables;
		// members
		structure_instantiations->insert(std::move(key), new_structure);
		for (const Structure::Member& member: structure->get_members()) {
			new_structure->add_member(member.get_name(), handle_type(member.get_type()));
		}
		this->type_variables = previous_type_variables;
		program->add_type(new_structure);
		return new_structure;
	}
	const FunctionInstantiation* instantiate_function(const Function* function, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != function->get_template_arguments().size()) {
			return nullptr;
		}
		Instantiations<Function, FunctionInstantiation>::Key key(function, std::move(template_arguments));
		if (const FunctionInstantiation* new_function = function_instantiations->look_up(key)) {
			return new_function;
		}
		FunctionInstantiation* new_function = new FunctionInstantiation(function);
		new_function->set_id(program->get_next_id());
		// template arguments
		ScopeMap type_variables;
		for (std::size_t i = 0; i < key.get_arguments().size(); ++i) {
			new_function->add_template_argument(key.get_arguments()[i]);
			type_variables.insert(function->get_template_arguments()[i], key.get_arguments()[i]);
		}
		ScopeMap* previous_type_variables = this->type_variables;
		this->type_variables = &type_variables;
		// arguments
		ScopeMap variables;
		for (const Function::Argument& argument: function->get_arguments()) {
			const Type* argument_type = handle_type(argument.get_type());
			new_function->add_argument(argument.get_name(), argument_type);
			variables.insert(argument.get_name(), argument_type);
		}
		ScopeMap* previous_variables = this->variables;
		this->variables = &variables;
		// return type
		new_function->set_return_type(handle_type(function->get_return_type()));
		// block
		function_instantiations->insert(std::move(key), new_function);
		new_function->set_block(handle_block(function->get_block()));
		this->variables = previous_variables;
		this->type_variables = previous_type_variables;
		program->add_function_instantiation(new_function);
		return new_function;
	}
	template <class T> const Type* get_builtin_type(const Type*& type) {
		if (type == nullptr) {
			T* t = new T();
			t->set_id(program->get_next_id());
			program->add_type(t);
			type = t;
		}
		return type;
	}
	const Type* get_void_type() {
		return get_builtin_type<VoidType>(void_type);
	}
	const Type* get_int_type() {
		return get_builtin_type<IntType>(int_type);
	}
	const Type* get_type(const StringView& name, std::vector<const Type*>&& arguments, const Expression* expression = nullptr) {
		if (!name) {
			return nullptr;
		}
		for (const Type* argument: arguments) {
			if (argument == nullptr) {
				return nullptr;
			}
		}
		if (name == "Void" && arguments.empty()) {
			return get_void_type();
		}
		if (name == "Int" && arguments.empty()) {
			return get_int_type();
		}
		const Structure* match_structure = nullptr;
		unsigned int match_count = 0;
		// TODO: optimize
		for (const Structure* structure: program->get_structures()) {
			if (structure->get_name() == name) {
				match_structure = structure;
				++match_count;
			}
		}
		if (match_count == 0) {
			add_error(expression, "struct \"%\" not found", name);
			return nullptr;
		}
		if (match_count > 1) {
			add_error(expression, "% structs named \"%\" found", printer::print_number(match_count), name);
			return nullptr;
		}
		if (match_structure->get_template_arguments().size() != arguments.size()) {
			add_error(expression, "invalid number of template arguments for struct \"%\", expected %", name, printer::print_plural("template argument", match_structure->get_template_arguments().size()));
			return nullptr;
		}
		return instantiate_structure(match_structure, std::move(arguments));
	}
	const FunctionInstantiation* get_function(const StringView& name, const std::vector<Reference<Expression>>& arguments, const Type* return_type, const Expression* expression = nullptr) {
		if (!name) {
			return nullptr;
		}
		const Function* match_function = nullptr;
		std::vector<const Type*> match_template_arguments;
		unsigned int match_count = 0;
		// TODO: optimize
		for (const Function* function: program->get_functions()) {
			if (function->get_name() == name) {
				std::vector<const Type*> template_arguments;
				if (Unification(this, function, arguments, return_type, template_arguments).run()) {
					match_function = function;
					match_template_arguments = std::move(template_arguments);
					++match_count;
				}
			}
		}
		if (match_count == 1) {
			return instantiate_function(match_function, std::move(match_template_arguments));
		}
		if (match_count == 0) {
			add_error(expression, "no matching function \"%\" found", name);
		}
		else {
			add_error(expression, "% matching functions \"%\" found", printer::print_number(match_count), name);
		}
		return nullptr;
	}
	StringView get_name(const Expression* expression) {
		if (expression == nullptr) {
			return StringView();
		}
		const Name* name = as<Name>(expression);
		if (name == nullptr) {
			add_error(expression, "invalid expression, expected a name");
			return StringView();
		}
		return name->get_name();
	}
	const Type* get_member_type(const Type* struct_type, const StringView& member_name, const Expression* expression) {
		if (struct_type == nullptr) {
			return nullptr;
		}
		auto* s = as<StructureInstantiation>(struct_type);
		if (s == nullptr) {
			add_error(expression, "invalid type %, expected a struct type", PrintTypeName(struct_type));
			return nullptr;
		}
		for (const StructureInstantiation::Member& member: s->get_members()) {
			if (member.get_name() == member_name) {
				return member.get_type();
			}
		}
		add_error(expression, "struct % does not have a field named \"%\"", PrintTypeName(struct_type), member_name);
		return nullptr;
	}
	const Type* handle_type(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		if (auto* e = as<Name>(expression)) {
			if (const Type* type = type_variables->look_up(e->get_name())) {
				return type;
			}
			return get_type(e->get_name(), std::vector<const Type*>(), expression);
		}
		else if (auto* e = as<Call>(expression)) {
			StringView name = get_name(e->get_expression());
			std::vector<const Type*> arguments;
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(handle_type(argument));
			}
			return get_type(name, std::move(arguments), expression);
		}
		return nullptr;
	}
	static Reference<Expression> with_type(Reference<Expression>&& expression, const Type* type) {
		expression->set_type(type);
		return std::move(expression);
	}
	void check_type(const Expression* expression, const Type* expected_type, bool& error) {
		if (expression && expected_type) {
			if (expression->get_type() != expected_type) {
				add_error(expression, "invalid type %, expected type %", PrintTypeName(expression->get_type()), PrintTypeName(expected_type));
				error = true;
			}
		}
	}
	void check_name(const Expression* expression, bool& error) {
		if (expression) {
			if (!as<Name>(expression)) {
				add_error(expression, "invalid expression, expected a name");
				error = true;
			}
		}
	}
	Reference<Expression> handle_expression_(const Expression* expression, const Type* expected_type) {
		if (auto* e = as<IntLiteral>(expression)) {
			return with_type(new IntLiteral(e->get_value()), get_int_type());
		}
		else if (auto* e = as<Name>(expression)) {
			StringView name = e->get_name();
			const Type* type = variables->look_up(name);
			if (type == nullptr) {
				add_error(expression, "undefined variable \"%\"", name);
				return Reference<Expression>();
			}
			return with_type(new Name(name.to_string()), type);
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			Reference<Expression> left = handle_expression(e->get_left());
			Reference<Expression> right = handle_expression(e->get_right());
			bool error = left == nullptr || right == nullptr;
			if (left && right) {
				if (!(left->get_type() == get_int_type() && right->get_type() == get_int_type())) {
					add_error(expression, "invalid binary expression");
					error = true;
				}
			}
			if (error) {
				return Reference<Expression>();
			}
			const Type* type = get_type(left);
			return with_type(new BinaryExpression(e->get_operation(), std::move(left), std::move(right)), type);
		}
		else if (auto* e = as<Assignment>(expression)) {
			Reference<Expression> left = handle_expression(e->get_left());
			Reference<Expression> right = handle_expression(e->get_right());
			const Type* type = get_type(left);
			bool error = left == nullptr || right == nullptr;
			check_name(left, error);
			check_type(right, type, error);
			if (error) {
				return Reference<Expression>();
			}
			return with_type(new Assignment(std::move(left), std::move(right)), type);
		}
		else if (auto* e = as<Call>(expression)) {
			StringView name;
			std::vector<Reference<Expression>> arguments;
			// uniform function call syntax
			if (auto* member_access = as<MemberAccess>(e->get_expression())) {
				name = member_access->get_member_name();
				arguments.push_back(handle_expression(member_access->get_expression()));
			}
			else {
				name = get_name(e->get_expression());
			}
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(handle_expression(argument));
			}
			const FunctionInstantiation* function = get_function(name, arguments, expected_type, expression);
			if (function == nullptr) {
				return Reference<Expression>();
			}
			return with_type(new Call(function->get_id(), std::move(arguments)), function->get_return_type());
		}
		else if (auto* e = as<MemberAccess>(expression)) {
			Reference<Expression> left = handle_expression(e->get_expression());
			StringView member_name = e->get_member_name();
			const Type* type = get_member_type(get_type(left), member_name, expression);
			if (left == nullptr || type == nullptr) {
				return Reference<Expression>();
			}
			return with_type(new MemberAccess(std::move(left), member_name.to_string()), type);
		}
		return Reference<Expression>();
	}
	Reference<Expression> handle_expression(const Expression* expression, const Type* expected_type = nullptr) {
		if (expression == nullptr) {
			return Reference<Expression>();
		}
		Reference<Expression> new_expression = handle_expression_(expression, expected_type);
		if (new_expression == nullptr) {
			return Reference<Expression>();
		}
		new_expression->set_location(expression->get_location());
		return new_expression;
	}
	Block handle_block(const Block* block) {
		std::vector<Reference<Statement>> statements;
		ScopeMap new_scope(variables);
		variables = &new_scope;
		for (const Statement* statement: block->get_statements()) {
			Reference<Statement> new_statement = handle_statement(statement);
			if (new_statement) {
				statements.push_back(std::move(new_statement));
			}
		}
		variables = variables->get_parent();
		return Block(std::move(statements));
	}
	Reference<Statement> handle_statement(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			return new BlockStatement(handle_block(s->get_block()));
		}
		else if (auto* s = as<EmptyStatement>(statement)) {
			return new EmptyStatement();
		}
		else if (auto* s = as<LetStatement>(statement)) {
			const Type* type = handle_type(s->get_type());
			Reference<Expression> expression = handle_expression(s->get_expression(), type);
			if (type == nullptr) {
				type = get_type(expression);
			}
			bool error = type == nullptr || expression == nullptr;
			check_type(expression, type, error);
			variables->insert(s->get_name(), type);
			if (error) {
				return Reference<Statement>();
			}
			return new LetStatement(s->get_name().to_string(), new Expression(type), std::move(expression));
		}
		else if (auto* s = as<IfStatement>(statement)) {
			Reference<Expression> condition = handle_expression(s->get_condition(), get_int_type());
			Reference<Statement> then_statement = handle_statement(s->get_then_statement());
			Reference<Statement> else_statement = handle_statement(s->get_else_statement());
			bool error = condition == nullptr || then_statement == nullptr || else_statement == nullptr;
			check_type(condition, get_int_type(), error);
			if (error) {
				return Reference<Statement>();
			}
			return new IfStatement(std::move(condition), std::move(then_statement), std::move(else_statement));
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			Reference<Expression> condition = handle_expression(s->get_condition(), get_int_type());
			Reference<Statement> statement = handle_statement(s->get_statement());
			bool error = condition == nullptr || statement == nullptr;
			check_type(condition, get_int_type(), error);
			if (error) {
				return Reference<Statement>();
			}
			return new WhileStatement(std::move(condition), std::move(statement));
		}
		else if (auto* s = as<ReturnStatement>(statement)) {
			Reference<Expression> expression = handle_expression(s->get_expression());
			return new ReturnStatement(std::move(expression));
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			Reference<Expression> expression = handle_expression(s->get_expression());
			bool error = expression == nullptr;
			if (error) {
				return Reference<Statement>();
			}
			return new ExpressionStatement(std::move(expression));
		}
		return Reference<Statement>();
	}
public:
	Pass1(Program* program, Errors* errors) {
		this->program = program;
		this->errors = errors;
	}
	void run() {
		Instantiations<Structure, StructureInstantiation> structure_instantiations;
		this->structure_instantiations = &structure_instantiations;
		Instantiations<Function, FunctionInstantiation> function_instantiations;
		this->function_instantiations = &function_instantiations;
		const FunctionInstantiation* main_function = get_function("main", {}, get_void_type());
		if (main_function == nullptr) {
			return;
		}
		program->set_main_function_id(main_function->get_id());
	}
};

void pass1(Program* program, Errors& errors) {
	Pass1(program, &errors).run();
}
