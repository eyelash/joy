#include "passes.hpp"
#include <functional>
#include <set>
#include <map>

template <class Key, class T> class Map {
	std::map<Key, const T*> map;
public:
	void insert(const Key& key, const T* value) {
		map.emplace(key, value);
	}
	void insert(Key&& key, const T* value) {
		map.emplace(std::move(key), value);
	}
	const T* look_up(const Key& key) const {
		auto iterator = map.find(key);
		if (iterator != map.end()) {
			return iterator->second;
		}
		return nullptr;
	}
};

template <class T> class ScopeMap {
	ScopeMap* parent;
	std::map<StringView, T> map;
public:
	ScopeMap(ScopeMap* parent = nullptr): parent(parent) {}
	ScopeMap* get_parent() const {
		return parent;
	}
	void set(const StringView& name, T value) {
		if (value) {
			map[name] = value;
		}
	}
	T look_up(const StringView& name) const {
		auto iterator = map.find(name);
		if (iterator != map.end()) {
			return iterator->second;
		}
		if (parent) {
			return parent->look_up(name);
		}
		return T();
	}
};

class VariableMap {
	VariableMap* parent;
	std::map<StringView, Reference<Expression>> map;
public:
	VariableMap(VariableMap* parent = nullptr): parent(parent) {}
	VariableMap* get_parent() const {
		return parent;
	}
	void set(const StringView& name, Reference<Expression>&& value) {
		if (value) {
			map[name] = std::move(value);
		}
	}
	bool update(const StringView& name, Reference<Expression>&& value) {
		auto iterator = map.find(name);
		if (iterator != map.end()) {
			iterator->second = std::move(value);
			return true;
		}
		if (parent) {
			return parent->update(name, std::move(value));
		}
		return false;
	}
	const Expression* look_up(const StringView& name) const {
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

template <class T> class EntityKey {
	typename T::Key key;
public:
	template <class... A> EntityKey(A&&... a): key(std::forward<A>(a)...) {}
	const typename T::Key& get_key() const {
		return key;
	}
};

class EntityCompare {
	template <class T> static typename T::Key get_key(const Entity* entity) {
		return static_cast<const T*>(entity)->get_key();
	}
	template <class T> static const typename T::Key& get_key(const EntityKey<T>& key) {
		return key.get_key();
	}
public:
	constexpr EntityCompare() {}
	bool operator ()(const Entity* lhs, const Entity* rhs) const {
		const int lhs_type_id = lhs->get_type_id();
		const int rhs_type_id = rhs->get_type_id();
		if (lhs_type_id != rhs_type_id) {
			return lhs_type_id < rhs_type_id;
		}
		if (lhs_type_id == VoidType::TYPE_ID) {
			return get_key<VoidType>(lhs) < get_key<VoidType>(rhs);
		}
		else if (lhs_type_id == IntType::TYPE_ID) {
			return get_key<IntType>(lhs) < get_key<IntType>(rhs);
		}
		else if (lhs_type_id == StringType::TYPE_ID) {
			return get_key<StringType>(lhs) < get_key<StringType>(rhs);
		}
		else if (lhs_type_id == ArrayType::TYPE_ID) {
			return get_key<ArrayType>(lhs) < get_key<ArrayType>(rhs);
		}
		else if (lhs_type_id == ArrayTypeInstantiation::TYPE_ID) {
			return get_key<ArrayTypeInstantiation>(lhs) < get_key<ArrayTypeInstantiation>(rhs);
		}
		else if (lhs_type_id == TupleType::TYPE_ID) {
			return get_key<TupleType>(lhs) < get_key<TupleType>(rhs);
		}
		else if (lhs_type_id == TupleTypeInstantiation::TYPE_ID) {
			return get_key<TupleTypeInstantiation>(lhs) < get_key<TupleTypeInstantiation>(rhs);
		}
		else if (lhs_type_id == FunctionInstantiation::TYPE_ID) {
			return get_key<FunctionInstantiation>(lhs) < get_key<FunctionInstantiation>(rhs);
		}
		else if (lhs_type_id == StructureInstantiation::TYPE_ID) {
			return get_key<StructureInstantiation>(lhs) < get_key<StructureInstantiation>(rhs);
		}
		return false;
	}
	template <class T> bool operator ()(const Entity* lhs, const EntityKey<T>& rhs) const {
		const int lhs_type_id = lhs->get_type_id();
		constexpr int rhs_type_id = T::TYPE_ID;
		if (lhs_type_id != rhs_type_id) {
			return lhs_type_id < rhs_type_id;
		}
		return get_key<T>(lhs) < get_key<T>(rhs);
	}
	template <class T> bool operator ()(const EntityKey<T>& lhs, const Entity* rhs) const {
		constexpr int lhs_type_id = T::TYPE_ID;
		const int rhs_type_id = rhs->get_type_id();
		if (lhs_type_id != rhs_type_id) {
			return lhs_type_id < rhs_type_id;
		}
		return get_key<T>(lhs) < get_key<T>(rhs);
	}
	using is_transparent = std::true_type;
};

class Interner {
	std::set<Entity*, EntityCompare> set;
public:
	void insert(Entity* entity) {
		set.insert(entity);
	}
	template <class T> T* look_up(const EntityKey<T>& key) const {
		auto iterator = set.find(key);
		if (iterator != set.end()) {
			return static_cast<T*>(*iterator);
		}
		return nullptr;
	}
	template <class T, class... A> T* look_up(A&&... a) const {
		return look_up(EntityKey<T>(std::forward<A>(a)...));
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
		else if (auto* e = as<StringLiteral>(expression)) {
			return new StringLiteral(e->get_string().to_string());
		}
		else if (auto* e = as<StructLiteral>(expression)) {
			std::vector<StructLiteral::Member> new_members;
			for (const StructLiteral::Member& member: e->get_members()) {
				new_members.emplace_back(member.get_name().to_string(), copy_expression(member.get_expression()));
			}
			return new StructLiteral(copy_expression(e->get_type()), std::move(new_members));
		}
		else if (auto* e = as<TupleLiteral>(expression)) {
			return new TupleLiteral(copy_expressions(e->get_elements()));
		}
		else if (auto* e = as<Name>(expression)) {
			return new Name(e->get_name().to_string());
		}
		else if (auto* e = as<Variable>(expression)) {
			return new Variable(e->get_index());
		}
		else if (auto* e = as<Assignment>(expression)) {
			return new Assignment(copy_expression(e->get_left()), copy_expression(e->get_right()));
		}
		else if (auto* e = as<Spread>(expression)) {
			return new Spread(copy_expression(e->get_expression()));
		}
		else if (auto* e = as<Call>(expression)) {
			return new Call(copy_expression(e->get_expression()), copy_expressions(e->get_arguments()));
		}
		else if (auto* e = as<Accessor>(expression)) {
			return new Accessor(copy_expression(e->get_left()), copy_expression(e->get_right()));
		}
		else if (auto* e = as<EntityReference>(expression)) {
			return new EntityReference(e->get_entity());
		}
		return Reference<Expression>();
	}
	static Reference<Expression> copy_expression(const Expression* expression) {
		Reference<Expression> new_expression = copy_expression_(expression);
		if (new_expression) {
			new_expression->set_location(expression->get_location());
			new_expression->set_type(expression->get_type());
		}
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
	static Reference<Statement> copy_statement_(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			return new BlockStatement(copy_block(s->get_block()));
		}
		else if (auto* s = as<LetStatement>(statement)) {
			return new LetStatement(copy_expression(s->get_variable()), copy_expression(s->get_type()), copy_expression(s->get_expression()));
		}
		else if (auto* s = as<IfStatement>(statement)) {
			Reference<Expression> condition = copy_expression(s->get_condition());
			Block then_block = copy_block(s->get_then_block());
			Block else_block = copy_block(s->get_else_block());
			return new IfStatement(std::move(condition), std::move(then_block), std::move(else_block));
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			Reference<Expression> condition = copy_expression(s->get_condition());
			Block block = copy_block(s->get_block());
			return new WhileStatement(std::move(condition), std::move(block));
		}
		else if (auto* s = as<ReturnStatement>(statement)) {
			Reference<Expression> expression = copy_expression(s->get_expression());
			return new ReturnStatement(std::move(expression));
		}
		else if (auto* s = as<BreakStatement>(statement)) {
			return new BreakStatement();
		}
		else if (auto* s = as<ContinueStatement>(statement)) {
			return new ContinueStatement();
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			return new ExpressionStatement(copy_expression(s->get_expression()));
		}
		return Reference<Statement>();
	}
	static Reference<Statement> copy_statement(const Statement* statement) {
		Reference<Statement> new_statement = copy_statement_(statement);
		if (new_statement) {
			new_statement->set_location(statement->get_location());
		}
		return new_statement;
	}
};

class Pass1 {
	enum class Result {
		OK,
		RETURN,
		BREAK,
		CONTINUE
	};
	Program* program;
	Diagnostics& diagnostics;
	VariableMap* variables = nullptr;
	Entity* current_entity = nullptr;
	Reference<Expression> return_value;
	static StringView get_name(const Expression* expression) {
		if (expression == nullptr) {
			return StringView();
		}
		const Name* name = as<Name>(expression);
		if (name == nullptr) {
			return StringView();
		}
		return name->get_name();
	}
	static Reference<Expression> copy_value(const Expression* expression) {
		if (auto* e = as<IntLiteral>(expression)) {
			return new IntLiteral(e->get_value());
		}
		else if (auto* e = as<StringLiteral>(expression)) {
			return new StringLiteral(e->get_string().to_string());
		}
		return Reference<Expression>();
	}
	static bool is_truthy(const Expression* expression) {
		if (auto* e = as<IntLiteral>(expression)) {
			return e->get_value() != 0;
		}
		return false;
	}
	template <class... T> void add_error(const Expression* expression, const char* s, T... t) {
		diagnostics.add_error(current_entity->get_path(), expression->get_location(), printer::format(s, t...));
	}
	template <class... T> void add_error(const char* s, T... t) {
		diagnostics.add_error(printer::format(s, t...));
	}
	Entity* find_function(const StringView& name) {
		for (Entity* entity: program->get_entities()) {
			if (BuiltinFunction* function = as<BuiltinFunction>(entity)) {
				if (function->get_name() == name) {
					return function;
				}
			}
			else if (Function* function = as<Function>(entity)) {
				if (function->get_name() == name) {
					return function;
				}
			}
		}
		add_error("function \"%\" not found", name);
		return nullptr;
	}
	Reference<Expression> evaluate_builtin_function(BuiltinFunction* function, std::vector<Reference<Expression>>&& arguments) {
		if (function->get_name() == "add") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() + right->get_value());
		}
		else if (function->get_name() == "subtract") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() - right->get_value());
		}
		else if (function->get_name() == "multiply") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() * right->get_value());
		}
		else if (function->get_name() == "divide") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() / right->get_value());
		}
		else if (function->get_name() == "remainder") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() % right->get_value());
		}
		else if (function->get_name() == "equal") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() == right->get_value());
		}
		else if (function->get_name() == "not_equal") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() != right->get_value());
		}
		else if (function->get_name() == "less_than") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() < right->get_value());
		}
		else if (function->get_name() == "less_than_or_equal") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() <= right->get_value());
		}
		else if (function->get_name() == "greater_than") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() > right->get_value());
		}
		else if (function->get_name() == "greater_than_or_equal") {
			if (arguments.size() != 2) {
				return Reference<Expression>();
			}
			IntLiteral* left = as<IntLiteral>(arguments[0]);
			IntLiteral* right = as<IntLiteral>(arguments[1]);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			return new IntLiteral(left->get_value() >= right->get_value());
		}
		else if (function->get_name() == "print") {
			using namespace printer;
			for (std::size_t i = 0; i < arguments.size(); ++i) {
				if (i > 0) {
					print(' ');
				}
				const Expression* argument = arguments[i];
				if (auto* e = as<IntLiteral>(argument)) {
					print(print_number(e->get_value()));
				}
				else if (auto* e = as<StringLiteral>(argument)) {
					print(e->get_string());
				}
				else {
					print("undefined");
				}
			}
			print(ln());
		}
		else {
			add_error("invalid builtin function \"%\"", function->get_name());
		}
		return Reference<Expression>();
	}
	Reference<Expression> evaluate_function(Function* function, std::vector<Reference<Expression>>&& arguments) {
		if (function->get_arguments().size() != arguments.size()) {
			return Reference<Expression>();
		}
		VariableMap* previous_variables = variables;
		Entity* previous_current_entity = current_entity;
		VariableMap new_variables;
		variables = &new_variables;
		for (std::size_t i = 0; i < arguments.size(); ++i) {
			const StringView argument_name = function->get_arguments()[i].get_name();
			new_variables.set(argument_name, std::move(arguments[i]));
		}
		current_entity = function;
		evaluate(function->get_block());
		variables = previous_variables;
		current_entity = previous_current_entity;
		return std::move(return_value);
	}
	Reference<Expression> evaluate(const Expression* expression) {
		if (expression == nullptr) {
			return Reference<Expression>();
		}
		if (auto* e = as<IntLiteral>(expression)) {
			return new IntLiteral(e->get_value());
		}
		else if (auto* e = as<StringLiteral>(expression)) {
			return new StringLiteral(e->get_string().to_string());
		}
		else if (auto* e = as<Name>(expression)) {
			const StringView name = e->get_name();
			const Expression* expression = variables->look_up(name);
			if (expression == nullptr) {
				add_error(e, "undefined variable \"%\"", name);
				return Reference<Expression>();
			}
			return copy_value(expression);
		}
		else if (auto* e = as<Assignment>(expression)) {
			const StringView name = get_name(e->get_left());
			Reference<Expression> expression = evaluate(e->get_right());
			if (!variables->update(name, copy_value(expression))) {
				add_error(e->get_left(), "undefined variable \"%\"", name);
			}
			return expression;
		}
		else if (auto* e = as<Call>(expression)) {
			const StringView name = get_name(e->get_expression());
			Entity* entity = find_function(name);
			std::vector<Reference<Expression>> arguments;
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(evaluate(argument));
			}
			if (Function* function = as<Function>(entity)) {
				return evaluate_function(function, std::move(arguments));
			}
			else if (BuiltinFunction* function = as<BuiltinFunction>(entity)) {
				return evaluate_builtin_function(function, std::move(arguments));
			}
		}
		return Reference<Expression>();
	}
	Result evaluate(const Block* block) {
		VariableMap* previous_variables = variables;
		VariableMap new_variables(variables);
		variables = &new_variables;
		for (const Statement* statement: block->get_statements()) {
			if (auto* s = as<BlockStatement>(statement)) {
				const Result result = evaluate(s->get_block());
				if (result != Result::OK) {
					variables = previous_variables;
					return result;
				}
			}
			else if (auto* s = as<LetStatement>(statement)) {
				StringView name = get_name(s->get_variable());
				variables->set(name, evaluate(s->get_expression()));
			}
			else if (auto* s = as<IfStatement>(statement)) {
				Reference<Expression> condition = evaluate(s->get_condition());
				const Block* block;
				if (is_truthy(condition)) {
					block = s->get_then_block();
				}
				else {
					block = s->get_else_block();
				}
				const Result result = evaluate(block);
				if (result != Result::OK) {
					variables = previous_variables;
					return result;
				}
			}
			else if (auto* s = as<WhileStatement>(statement)) {
				while (true) {
					Reference<Expression> condition = evaluate(s->get_condition());
					if (!is_truthy(condition)) {
						break;
					}
					const Result result = evaluate(s->get_block());
					if (result == Result::BREAK) {
						break;
					}
					if (result == Result::CONTINUE) {
						continue;
					}
					if (result != Result::OK) {
						variables = previous_variables;
						return result;
					}
				}
			}
			else if (auto* s = as<ReturnStatement>(statement)) {
				return_value = evaluate(s->get_expression());
				variables = previous_variables;
				return Result::RETURN;
			}
			else if (auto* s = as<BreakStatement>(statement)) {
				variables = previous_variables;
				return Result::BREAK;
			}
			else if (auto* s = as<ContinueStatement>(statement)) {
				variables = previous_variables;
				return Result::CONTINUE;
			}
			else if (auto* s = as<ExpressionStatement>(statement)) {
				evaluate(s->get_expression());
			}
		}
		variables = previous_variables;
		return Result::OK;
	}
public:
	Pass1(Program* program, Diagnostics& diagnostics): program(program), diagnostics(diagnostics) {}
	void run() {
		Function* main_function = as<Function>(find_function("main"));
		if (main_function == nullptr) {
			return;
		}
		evaluate_function(main_function, {});
	}
};

void pass1(Program* program, Diagnostics& diagnostics) {
	Pass1(program, diagnostics).run();
}
