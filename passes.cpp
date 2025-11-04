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
		map.emplace(name, value);
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

template <class T> class Instantiations {
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
	std::map<Key, const T*> instantiations;
public:
	void insert(Key&& key, const T* t) {
		// TODO: does this move actually work?
		instantiations.emplace(std::move(key), t);
	}
	const T* look_up(const Key& key) const {
		auto iterator = instantiations.find(key);
		if (iterator != instantiations.end()) {
			return iterator->second;
		}
		return nullptr;
	}
};

class TypeChecking {
	Program* program;
	Errors* errors;
	FlatMap<Function>* functions;
	FlatMap<Structure>* structures;
	Interner* type_interner;
	ScopeMap* scope;
	template <class P> void add_error(const Expression* expression, P&& p) {
		errors->add_error(program->get_path().c_str(), expression->get_location(), print_to_string(std::forward<P>(p)));
	}
	const Type* get_type(Interner::Key key, const Expression* expression = nullptr) {
		if (const Type* type = type_interner->look_up(key)) {
			return type;
		}
		if (key.get_name() == "Void") {
			VoidType* type = program->add_type<VoidType>();
			type_interner->insert("Void", type);
			return type;
		}
		else if (key.get_name() == "Int") {
			IntType* type = program->add_type<IntType>();
			type_interner->insert("Int", type);
			return type;
		}
		else if (const Structure* structure = structures->look_up(key.get_name())) {
			StructType* type = program->add_type<StructType>();
			type_interner->insert(std::move(key), type);
			for (const Structure::Member& member: structure->get_members()) {
				type->add_member(member.get_name(), handle_type(member.get_type()));
			}
			return type;
		}
		else {
			using namespace printer;
			add_error(expression, format("undefined type \"%\"", key.get_name()));
			return nullptr;
		}
	}
	const Type* handle_type(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		if (auto* e = as<Name>(expression)) {
			return get_type(e->get_name(), expression);
		}
		else if (auto* e = as<Call>(expression)) {
			add_error(expression, "templates are not yet implemented");
		}
		return nullptr;
	}
	const Type* handle_expression(const Expression* expression, const Type* expected_type = nullptr) {
		if (auto* e = as<IntLiteral>(expression)) {
			return get_type("Int");
		}
		if (auto* e = as<Name>(expression)) {
			const Type* type = scope->look_up(e->get_name());
			if (type == nullptr) {
				using namespace printer;
				add_error(expression, format("undefined variable \"%\"", e->get_name()));
				return nullptr;
			}
			return type;
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			const Type* left_type = handle_expression(e->get_left());
			const Type* right_type = handle_expression(e->get_right());
			if (left_type == nullptr || right_type == nullptr) {
				return nullptr;
			}
			if (left_type != right_type) {
				add_error(expression, "invalid binary expression");
				return nullptr;
			}
			return left_type;
		}
		else if (auto* e = as<Assignment>(expression)) {
			const Type* type = handle_expression(e->get_right());
			auto* name = as<Name>(e->get_left());
			if (name == nullptr) {
				add_error(e->get_left(), "invalid assignment");
				return nullptr;
			}
			const Type* expected_type = scope->look_up(name->get_name());
			if (expected_type == nullptr) {
				using namespace printer;
				add_error(e->get_left(), format("undefined variable \"%\"", name->get_name()));
				return nullptr;
			}
			if (type == nullptr) {
				return nullptr;
			}
			if (type != expected_type) {
				add_error(e->get_right(), "invalid type");
				return nullptr;
			}
			return type;
		}
		else if (auto* e = as<Call>(expression)) {
			std::vector<const Type*> argument_types;
			for (const Expression* argument: e->get_arguments()) {
				argument_types.push_back(handle_expression(argument));
			}
			auto* name = as<Name>(e->get_expression());
			if (name == nullptr) {
				add_error(e->get_expression(), "invalid call");
				return nullptr;
			}
			const Function* function = functions->look_up(name->get_name());
			if (function == nullptr) {
				using namespace printer;
				add_error(e->get_expression(), format("undefined function \"%\"", name->get_name()));
				return nullptr;
			}
			if (argument_types.size() != function->get_arguments().size()) {
				using namespace printer;
				add_error(expression, format("invalid number of arguments, expected %", print_plural("argument", function->get_arguments().size())));
				return nullptr;
			}
			for (const Type* argument_type: argument_types) {
				if (argument_type == nullptr) {
					return nullptr;
				}
			}
			for (std::size_t i = 0; i < argument_types.size(); ++i) {
				if (argument_types[i] != handle_type(function->get_arguments()[i].get_type())) {
					add_error(e->get_arguments()[i], "invalid argument type");
				}
			}
			return handle_type(function->get_return_type());
		}
		return nullptr;
	}
	void handle_block(const Block* block) {
		ScopeMap new_scope(scope);
		scope = &new_scope;
		for (const Statement* statement: block->get_statements()) {
			handle_statement(statement);
		}
		scope = scope->get_parent();
	}
	void handle_statement(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			handle_block(s->get_block());
		}
		else if (auto* s = as<LetStatement>(statement)) {
			const Type* type = handle_expression(s->get_expression());
			const Type* expected_type = handle_type(s->get_type());
			if (expected_type == nullptr) {
				expected_type = type;
			}
			if (scope->look_up(s->get_name())) {
				using namespace printer;
				add_error(s->get_expression(), format("variable \"%\" already defined", s->get_name()));
			}
			else if (expected_type != nullptr) {
				scope->insert(s->get_name(), expected_type);
			}
			if (type == nullptr) {
				return;
			}
			if (type != expected_type) {
				add_error(s->get_expression(), "invalid type");
			}
		}
		else if (auto* s = as<IfStatement>(statement)) {
			const Type* condition_type = handle_expression(s->get_condition(), get_type("Int"));
			if (condition_type && condition_type != get_type("Int")) {
				add_error(s->get_condition(), "invalid type");
			}
			handle_statement(s->get_then_statement());
			handle_statement(s->get_else_statement());
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			const Type* condition_type = handle_expression(s->get_condition(), get_type("Int"));
			if (condition_type && condition_type != get_type("Int")) {
				add_error(s->get_condition(), "invalid type");
			}
			handle_statement(s->get_statement());
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			handle_expression(s->get_expression());
		}
	}
public:
	TypeChecking(Program* program, Errors* errors): program(program), errors(errors), functions(nullptr), structures(nullptr), type_interner(nullptr), scope(nullptr) {}
	void run() {
		FlatMap<Function> functions;
		FlatMap<Structure> structures;
		for (const Function* function: program->get_functions()) {
			functions.add_entry(function->get_name(), function);
		}
		functions.sort();
		this->functions = &functions;
		for (const Structure* structure: program->get_structures()) {
			structures.add_entry(structure->get_name(), structure);
		}
		structures.sort();
		this->structures = &structures;
		Interner type_interner;
		this->type_interner = &type_interner;
		for (const Function* function: program->get_functions()) {
			ScopeMap scope;
			this->scope = &scope;
			for (const Function::Argument& argument: function->get_arguments()) {
				scope.insert(argument.get_name(), handle_type(argument.get_type()));
			}
			handle_type(function->get_return_type());
			handle_block(function->get_block());
		}
		for (const Structure* structure: program->get_structures()) {
			for (const Structure::Member& member: structure->get_members()) {
				handle_type(member.get_type());
			}
		}
	}
};

void type_checking(Program* program, Errors& errors) {
	TypeChecking(program, &errors).run();
}

class Pass1 {
	const Program* program;
	Errors* errors;
	Program* new_program;
	const Type* void_type = nullptr;
	const Type* int_type = nullptr;
	Instantiations<Structure>* structure_instantiations;
	Instantiations<Function>* function_instantiations;
	ScopeMap* variables = nullptr;
	ScopeMap* type_variables = nullptr;
	unsigned int current_id = 0;
	template <class P> void add_error(const Expression* expression, P&& p) {
		errors->add_error(program->get_path().c_str(), expression->get_location(), print_to_string(std::forward<P>(p)));
	}
	unsigned int get_next_id() {
		++current_id;
		return current_id;
	}
	const Structure* instantiate_structure(const Structure* structure, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != structure->get_template_arguments().size()) {
			return nullptr;
		}
		Instantiations<Structure>::Key key(structure, std::move(template_arguments));
		if (const Structure* new_structure = structure_instantiations->look_up(key)) {
			return new_structure;
		}
		Structure* new_structure = new Structure();
		new_structure->set_id(get_next_id());
		new_structure->set_name(structure->get_name());
		// template arguments
		ScopeMap type_variables;
		for (std::size_t i = 0; i < key.get_arguments().size(); ++i) {
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
		new_program->add_type(new_structure);
		return new_structure;
	}
	const Function* instantiate_function(const Function* function, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != function->get_template_arguments().size()) {
			return nullptr;
		}
		Instantiations<Function>::Key key(function, std::move(template_arguments));
		if (const Function* new_function = function_instantiations->look_up(key)) {
			return new_function;
		}
		Function* new_function = new Function();
		new_function->set_id(get_next_id());
		new_function->set_name(function->get_name());
		// template arguments
		ScopeMap type_variables;
		for (std::size_t i = 0; i < key.get_arguments().size(); ++i) {
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
		new_program->add_function(new_function);
		return new_function;
	}
	template <class T> const Type* get_builtin_type(const Type*& type) {
		if (type == nullptr) {
			T* t = new T();
			t->set_id(get_next_id());
			new_program->add_type(t);
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
	const Type* get_type(const StringView& name, std::vector<const Type*>&& arguments) {
		if (name == "Void" && arguments.empty()) {
			return get_void_type();
		}
		if (name == "Int" && arguments.empty()) {
			return get_int_type();
		}
		// TODO: optimize
		for (const Structure* structure: program->get_structures()) {
			if (structure->get_name() == name && structure->get_template_arguments().size() == arguments.size()) {
				return instantiate_structure(structure, std::move(arguments));
			}
		}
		return nullptr;
	}
	const Function* get_function(const StringView& name, std::vector<const Type*>&& arguments) {
		// TODO: optimize
		for (const Function* function: program->get_functions()) {
			// TODO: unification
			if (function->get_name() == name && function->get_arguments().size() == arguments.size()) {
				return instantiate_function(function, std::vector<const Type*>());
			}
		}
		return nullptr;
	}
	const Function* get_function(const StringView& name, const std::vector<Reference<Expression>>& arguments) {
		std::vector<const Type*> argument_types;
		for (const Expression* expression: arguments) {
			if (expression == nullptr) {
				return nullptr;
			}
			argument_types.push_back(expression->get_type());
		}
		return get_function(name, std::move(argument_types));
	}
	StringView get_name(const Expression* expression) {
		if (auto* e = as<Name>(expression)) {
			return e->get_name();
		}
		else {
			add_error(expression, "invalid expression, expected a name");
			return StringView();
		}
	}
	const Type* handle_type(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		if (auto* e = as<Name>(expression)) {
			if (const Type* type = type_variables->look_up(e->get_name())) {
				return type;
			}
			return get_type(e->get_name(), std::vector<const Type*>());
		}
		else if (auto* e = as<Call>(expression)) {
			StringView name = get_name(e->get_expression());
			std::vector<const Type*> arguments;
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(handle_type(argument));
			}
			return get_type(name, std::move(arguments));
		}
		return nullptr;
	}
	static std::string to_string(const StringView& s) {
		return std::string(s.data(), s.size());
	}
	static Reference<Expression> with_type(Reference<Expression>&& expression, const Type* type) {
		expression->set_type(type);
		return std::move(expression);
	}
	Reference<Expression> handle_expression(const Expression* expression, const Type* expected_type = nullptr) {
		if (auto* e = as<IntLiteral>(expression)) {
			return with_type(new IntLiteral(e->get_value()), get_int_type());
		}
		if (auto* e = as<Name>(expression)) {
			const Type* type = variables->look_up(e->get_name());
			if (type == nullptr) {
				using namespace printer;
				add_error(expression, format("undefined variable \"%\"", e->get_name()));
				return nullptr;
			}
			return with_type(new Name(to_string(e->get_name())), type);
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			// TODO
		}
		else if (auto* e = as<Assignment>(expression)) {
			// TODO
		}
		else if (auto* e = as<Call>(expression)) {
			StringView name = get_name(e->get_expression());
			std::vector<Reference<Expression>> arguments;
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(handle_expression(argument));
			}
			const Function* function = get_function(name, arguments);
			if (function == nullptr) {
				return Reference<Expression>();
			}
			return new Call(function->get_id(), std::move(arguments));
		}
		return Reference<Expression>();
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
		else if (auto* s = as<LetStatement>(statement)) {
			const Type* type = handle_type(s->get_type());
			Reference<Expression> expression = handle_expression(s->get_expression());
			if (expression == nullptr) {
				return Reference<Statement>();
			}
			if (type == nullptr) {
				type = expression->get_type();
			}
			variables->insert(s->get_name(), type);
			return new LetStatement(to_string(s->get_name()), new Expression(type), std::move(expression));
		}
		else if (auto* s = as<IfStatement>(statement)) {
			// TODO
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			// TODO
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			return new ExpressionStatement(handle_expression(s->get_expression()));
		}
		return Reference<Statement>();
	}
public:
	Pass1(const Program* program, Errors* errors) {
		this->program = program;
		this->errors = errors;
	}
	Reference<Program> run() {
		Reference<Program> new_program = new Program();
		this->new_program = new_program;
		Instantiations<Structure> structure_instantiations;
		this->structure_instantiations = &structure_instantiations;
		Instantiations<Function> function_instantiations;
		this->function_instantiations = &function_instantiations;
		get_function("main", std::vector<const Type*>());
		return new_program;
	}
};

Reference<Program> pass1(const Program* program, Errors& errors) {
	return Pass1(program, &errors).run();
}
