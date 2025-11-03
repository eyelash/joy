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
