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
		else if (lhs_type_id == TupleType::TYPE_ID) {
			return get_key<TupleType>(lhs) < get_key<TupleType>(rhs);
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
	std::set<const Entity*, EntityCompare> set;
public:
	void insert(const Entity* entity) {
		set.insert(entity);
	}
	template <class T> const T* look_up(const EntityKey<T>& key) const {
		auto iterator = set.find(key);
		if (iterator != set.end()) {
			return static_cast<const T*>(*iterator);
		}
		return nullptr;
	}
	template <class T, class... A> const T* look_up(A&&... a) const {
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
		else if (auto* e = as<ArrayLiteral>(expression)) {
			return new ArrayLiteral(copy_expressions(e->get_elements()));
		}
		else if (auto* e = as<Name>(expression)) {
			return new Name(e->get_name().to_string());
		}
		else if (auto* e = as<Variable>(expression)) {
			return new Variable(e->get_index());
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
		else if (auto* e = as<Accessor>(expression)) {
			return new Accessor(copy_expression(e->get_left()), copy_expression(e->get_right()));
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
	}
};

class UnificationVariables {
	const std::string* names;
	std::vector<const Type*> variables;
public:
	UnificationVariables(const std::vector<std::string>& names): names(names.data()), variables(names.size()) {}
	UnificationVariables(): names(nullptr) {}
	Index look_up(const StringView& name) const {
		for (std::size_t i = 0; i < variables.size(); ++i) {
			if (name == names[i]) {
				return i;
			}
		}
		return Index();
	}
	bool set(std::size_t i, const Type* type) {
		if (variables[i]) {
			return variables[i] == type;
		}
		variables[i] = type;
		return true;
	}
	bool check() const {
		for (const Type* variable: variables) {
			if (variable == nullptr) {
				return false;
			}
		}
		return true;
	}
	std::vector<const Type*> take() {
		return std::move(variables);
	}
};

static bool match(UnificationVariables& variables, const Expression* expression, const Type* type);
static bool match(UnificationVariables& variables, const std::vector<Reference<Expression>>& expressions, const std::vector<const Type*>& types) {
	if (expressions.size() != types.size()) {
		return false;
	}
	for (std::size_t i = 0; i < expressions.size(); ++i) {
		if (!match(variables, expressions[i], types[i])) {
			return false;
		}
	}
	return true;
}
static bool match(UnificationVariables& variables, const Expression* expression, const Type* type) {
	if (expression == nullptr || type == nullptr) {
		return false;
	}
	if (auto* e = as<Name>(expression)) {
		StringView name = e->get_name();
		if (const Index i = variables.look_up(name)) {
			return variables.set(*i, type);
		}
		if (as<VoidType>(type)) {
			return name == "Void";
		}
		else if (as<IntType>(type)) {
			return name == "Int" || name == "Bool";
		}
		else if (as<StringType>(type)) {
			return name == "String";
		}
		else if (auto* t = as<TupleType>(type)) {
			return name == "Tuple" && t->get_element_types().empty();
		}
		else if (auto* s = as<StructureInstantiation>(type)) {
			return name == s->get_structure()->get_name() && s->get_template_arguments().empty();
		}
		return false;
	}
	else if (auto* e = as<Call>(expression)) {
		StringView name = as<Name>(e->get_expression())->get_name();
		if (as<VoidType>(type)) {
			return name == "Void" && e->get_arguments().empty();
		}
		else if (as<IntType>(type)) {
			return (name == "Int" || name == "Bool") && e->get_arguments().empty();
		}
		else if (as<StringType>(type)) {
			return name == "String" && e->get_arguments().empty();
		}
		else if (auto* s = as<ArrayType>(type)) {
			if (name != "Array") {
				return false;
			}
			if (e->get_arguments().size() != 1) {
				return false;
			}
			return match(variables, e->get_arguments()[0], s->get_element_type());
		}
		else if (auto* t = as<TupleType>(type)) {
			if (name != "Tuple") {
				return false;
			}
			return match(variables, e->get_arguments(), t->get_element_types());
		}
		else if (auto* s = as<StructureInstantiation>(type)) {
			if (name != s->get_structure()->get_name()) {
				return false;
			}
			return match(variables, e->get_arguments(), s->get_template_arguments());
		}
	}
	return false;
}

// name resolution, type checking, and template instantiation
class Pass1 {
	static SourceLocation get_location(const Expression* expression) {
		if (expression == nullptr) {
			return SourceLocation();
		}
		return expression->get_location();
	}
	static const Type* get_type(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		return expression->get_type();
	}
	static bool is_empty_statement(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			return s->get_block()->get_statements().empty();
		}
		return false;
	}
	static bool is_final_statement(const Block* block) {
		if (block->get_statements().empty()) {
			return false;
		}
		return is_final_statement(block->get_statements().back());
	}
	static bool is_final_statement(const Statement* statement) {
		if (as<ReturnStatement>(statement)) {
			return true;
		}
		else if (as<BreakStatement>(statement)) {
			return true;
		}
		else if (as<ContinueStatement>(statement)) {
			return true;
		}
		else if (auto* s = as<BlockStatement>(statement)) {
			return is_final_statement(s->get_block());
		}
		else if (auto* s = as<IfStatement>(statement)) {
			return is_final_statement(s->get_then_block()) && is_final_statement(s->get_else_block());
		}
		return false;
	}
	static bool unification(const Function* function, const std::vector<Reference<Expression>>& arguments, const Type* return_type, UnificationVariables& variables) {
		if (function->get_arguments().size() != arguments.size()) {
			return false;
		}
		for (std::size_t i = 0; i < arguments.size(); ++i) {
			if (!match(variables, function->get_arguments()[i].get_type(), get_type(arguments[i]))) {
				return false;
			}
		}
		if (function->get_return_type() && return_type) {
			if (!match(variables, function->get_return_type(), return_type)) {
				return false;
			}
		}
		return variables.check();
	}
	Program* program;
	Errors* errors;
	Interner interner;
	ScopeMap<Index>* variables = nullptr;
	ScopeMap<const Type*>* type_variables = nullptr;
	FunctionInstantiation* current_function = nullptr;
	const WhileStatement* current_loop = nullptr;
	template <class... T> void add_error(const Expression* expression, const char* s, T... t) {
		errors->add_error(program->get_path().c_str(), get_location(expression), printer::format(s, t...));
	}
	template <class... T> void add_warning(const Expression* expression, const char* s, T... t) {
		errors->add_warning(program->get_path().c_str(), get_location(expression), printer::format(s, t...));
	}
	const Type* instantiate_structure(const Structure* structure, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != structure->get_template_arguments().size()) {
			return nullptr;
		}
		if (const StructureInstantiation* new_structure = interner.look_up<StructureInstantiation>(structure, template_arguments)) {
			return new_structure;
		}
		StructureInstantiation* new_structure = new StructureInstantiation(structure, std::move(template_arguments));
		auto previous_type_variables = this->type_variables;
		// template arguments
		ScopeMap<const Type*> type_variables;
		for (std::size_t i = 0; i < structure->get_template_arguments().size(); ++i) {
			type_variables.set(structure->get_template_arguments()[i], new_structure->get_template_arguments()[i]);
		}
		this->type_variables = &type_variables;
		// members
		interner.insert(new_structure);
		for (const Structure::Member& member: structure->get_members()) {
			new_structure->add_member(member.get_name(), handle_type(member.get_type()));
		}
		this->type_variables = previous_type_variables;
		program->add_entity(new_structure);
		return new_structure;
	}
	const FunctionInstantiation* instantiate_function(const Function* function, std::vector<const Type*>&& template_arguments) {
		if (template_arguments.size() != function->get_template_arguments().size()) {
			return nullptr;
		}
		if (const FunctionInstantiation* new_function = interner.look_up<FunctionInstantiation>(function, template_arguments)) {
			return new_function;
		}
		FunctionInstantiation* new_function = new FunctionInstantiation(function, std::move(template_arguments));
		auto previous_variables = this->variables;
		auto previous_type_variables = this->type_variables;
		auto previous_current_function = this->current_function;
		// template arguments
		ScopeMap<const Type*> type_variables;
		for (std::size_t i = 0; i < function->get_template_arguments().size(); ++i) {
			type_variables.set(function->get_template_arguments()[i], new_function->get_template_arguments()[i]);
		}
		this->type_variables = &type_variables;
		// arguments
		ScopeMap<Index> variables;
		for (const Function::Argument& argument: function->get_arguments()) {
			const Type* argument_type = handle_type(argument.get_type());
			const unsigned int index = new_function->add_argument(argument_type);
			variables.set(argument.get_name(), index);
		}
		this->variables = &variables;
		// return type
		if (function->get_return_type()) {
			new_function->set_return_type(handle_type(function->get_return_type()));
		}
		// block
		this->current_function = new_function;
		interner.insert(new_function);
		new_function->set_block(handle_block(function->get_block()));
		if (!is_final_statement(new_function->get_block())) {
			if (new_function->get_return_type() == nullptr) {
				new_function->set_return_type(get_void_type());
			}
			if (as<VoidType>(new_function->get_return_type())) {
				new_function->get_block()->add_statement(new ReturnStatement());
			}
			else {
				add_error(Reference<Expression>(), "missing return in function \"%\"", function->get_name());
			}
		}
		this->variables = previous_variables;
		this->type_variables = previous_type_variables;
		this->current_function = previous_current_function;
		program->add_entity(new_function);
		return new_function;
	}
	template <class T, class... A> const T* get_builtin_entity(A&&... a) {
		if (const T* t = interner.look_up<T>(a...)) {
			return t;
		}
		T* t = new T(std::forward<A>(a)...);
		program->add_entity(t);
		interner.insert(t);
		return t;
	}
	const Type* get_void_type() {
		return get_builtin_entity<VoidType>();
	}
	const Type* get_int_type() {
		return get_builtin_entity<IntType>();
	}
	const Type* get_string_type() {
		return get_builtin_entity<StringType>();
	}
	const Type* get_array_type(const Type* element_type) {
		return get_builtin_entity<ArrayType>(element_type);
	}
	const Type* get_tuple_type(std::vector<const Type*>&& element_types) {
		return get_builtin_entity<TupleType>(std::move(element_types));
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
		else if ((name == "Int" || name == "Bool") && arguments.empty()) {
			return get_int_type();
		}
		else if (name == "String" && arguments.empty()) {
			return get_string_type();
		}
		else if (name == "Array" && arguments.size() == 1) {
			return get_array_type(arguments[0]);
		}
		else if (name == "Tuple") {
			return get_tuple_type(std::move(arguments));
		}
		const Structure* match_structure = nullptr;
		unsigned int match_count = 0;
		// TODO: optimize
		for (const Entity* entity: program->get_source_entities()) {
			if (const Structure* structure = as<Structure>(entity)) {
				if (structure->get_name() == name) {
					if (structure->get_template_arguments().size() == arguments.size()) {
						match_structure = structure;
						++match_count;
					}
				}
			}
		}
		if (match_count != 1) {
			if (match_count == 0) {
				add_error(expression, "no matching struct \"%\" found", name);
			}
			else {
				add_error(expression, "% matching structs \"%\" found", printer::print_number(match_count), name);
			}
			return nullptr;
		}
		return instantiate_structure(match_structure, std::move(arguments));
	}
	const Entity* get_function(const StringView& name, const std::vector<Reference<Expression>>& arguments, const Type* return_type, const Expression* expression = nullptr) {
		if (!name) {
			return nullptr;
		}
		for (const Expression* argument: arguments) {
			if (get_type(argument) == nullptr) {
				return nullptr;
			}
		}
		if (name == "__builtin_joy_print_int") {
			if (arguments.size() != 1) {
				add_error(expression, "invalid number of arguments, expected 1 argument");
				return nullptr;
			}
			const Type* argument_type = get_type(arguments[0]);
			if (!as<IntType>(argument_type)) {
				add_error(expression, "invalid argument type %, expected type Int", PrintTypeName(argument_type));
				return nullptr;
			}
			return get_builtin_entity<BuiltinFunction>("__builtin_joy_print_int");
		}
		const Function* match_function = nullptr;
		std::vector<const Type*> match_template_arguments;
		unsigned int match_count = 0;
		// TODO: optimize
		for (const Entity* entity: program->get_source_entities()) {
			if (const Function* function = as<Function>(entity)) {
				if (function->get_name() == name) {
					UnificationVariables unification_variables(function->get_template_arguments());
					if (unification(function, arguments, return_type, unification_variables)) {
						match_function = function;
						match_template_arguments = unification_variables.take();
						++match_count;
					}
				}
			}
		}
		if (match_count != 1) {
			if (match_count == 0) {
				add_error(expression, "no matching function \"%\" found", name);
			}
			else {
				add_error(expression, "% matching functions \"%\" found", printer::print_number(match_count), name);
			}
			return nullptr;
		}
		return instantiate_function(match_function, std::move(match_template_arguments));
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
	const Type* get_return_type(const Entity* function) {
		if (auto* f = as<BuiltinFunction>(function)) {
			if (f->get_name() == "__builtin_joy_print_int") {
				return get_void_type();
			}
		}
		else if (auto* f = as<FunctionInstantiation>(function)) {
			return f->get_return_type();
		}
		return nullptr;
	}
	StringView get_constant_string(const Expression* expression) {
		if (expression == nullptr) {
			return StringView();
		}
		const StringLiteral* string = as<StringLiteral>(expression);
		if (string == nullptr) {
			add_error(expression, "invalid expression, expected a string literal");
			return StringView();
		}
		return string->get_string();
	}
	const std::int32_t* get_constant_int(const Expression* expression) {
		if (expression == nullptr) {
			return nullptr;
		}
		const IntLiteral* int_literal = as<IntLiteral>(expression);
		if (int_literal == nullptr) {
			add_error(expression, "invalid expression, expected an integer literal");
			return nullptr;
		}
		return &int_literal->get_value();
	}
	const Type* get_member_type(const Type* struct_type, const StringView& member_name, const Expression* expression) {
		if (struct_type == nullptr || !member_name) {
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
	bool check_type(const Expression* expression, const Type* expected_type) {
		if (expression && expected_type) {
			if (expression->get_type() != expected_type) {
				add_error(expression, "invalid type %, expected type %", PrintTypeName(expression->get_type()), PrintTypeName(expected_type));
				return false;
			}
		}
		return true;
	}
	Reference<Expression> handle_name(const Name* e) {
		StringView name = e->get_name();
		const Index index = variables->look_up(name);
		if (!index) {
			add_error(e, "undefined variable \"%\"", name);
			return Reference<Expression>();
		}
		const Type* type = current_function->get_variable(*index);
		if (type == nullptr) {
			return Reference<Expression>();
		}
		return new Variable(*index, type);
	}
	Reference<Expression> handle_name(const Expression* expression) {
		const Name* name = as<Name>(expression);
		if (name == nullptr) {
			add_error(expression, "invalid expression, expected a name");
			return Reference<Expression>();
		}
		return handle_name(name);
	}
	Reference<Expression> handle_expression_(const Expression* expression, const Type* expected_type) {
		if (auto* e = as<IntLiteral>(expression)) {
			return new IntLiteral(e->get_value(), get_int_type());
		}
		else if (auto* e = as<CharLiteral>(expression)) {
			StringView string_view = e->get_string();
			const std::int32_t value = next_code_point(string_view);
			if (!string_view.empty()) {
				add_error(expression, "char literal contains more than one code point");
				return Reference<Expression>();
			}
			return new IntLiteral(value, get_int_type());
		}
		else if (auto* e = as<StringLiteral>(expression)) {
			add_error(expression, "strings are not yet supported");
			return Reference<Expression>();
		}
		else if (auto* e = as<StructLiteral>(expression)) {
			const Type* type;
			if (e->get_type()) {
				type = handle_type(e->get_type());
			}
			else {
				type = expected_type;
			}
			if (type == nullptr) {
				add_error(expression, "type of struct literal cannot be determined");
				return Reference<Expression>();
			}
			auto* structure_instantiation = as<StructureInstantiation>(type);
			if (structure_instantiation == nullptr) {
				add_error(expression, "invalid type % for struct literal", PrintTypeName(type));
				return Reference<Expression>();
			}
			const std::size_t field_count = structure_instantiation->get_members().size();
			if (e->get_members().size() != field_count) {
				add_error(expression, "invalid number of fields, expected %", printer::print_plural("field", field_count));
				return Reference<Expression>();
			}
			std::vector<StructLiteral::Member> members;
			for (std::size_t i = 0; i < field_count; ++i) {
				const StructLiteral::Member& member = e->get_members()[i];
				const StructureInstantiation::Member& expected_member = structure_instantiation->get_members()[i];
				if (member.get_name() != expected_member.get_name()) {
					add_error(expression, "invalid field name \"%\", expected \"%\"", member.get_name(), expected_member.get_name());
					return Reference<Expression>();
				}
				Reference<Expression> member_expression = handle_expression(member.get_expression(), expected_member.get_type());
				if (member_expression == nullptr) {
					return Reference<Expression>();
				}
				if (member_expression->get_type() != expected_member.get_type()) {
					add_error(member.get_expression(), "invalid type % for field \"%\", expected type %", PrintTypeName(member_expression->get_type()), member.get_name(), PrintTypeName(expected_member.get_type()));
					return Reference<Expression>();
				}
				members.emplace_back(member.get_name().to_string(), std::move(member_expression));
			}
			return new StructLiteral(Reference<Expression>(), std::move(members), type);
		}
		else if (auto* e = as<ArrayLiteral>(expression)) {
			if (auto* s = as<TupleType>(expected_type)) {
				const std::size_t element_count = s->get_element_types().size();
				if (e->get_elements().size() != element_count) {
					add_error(expression, "invalid number of elements, expected %", printer::print_plural("element", element_count));
					return Reference<Expression>();
				}
				std::vector<Reference<Expression>> elements;
				for (std::size_t i = 0; i < element_count; ++i) {
					const Expression* element = e->get_elements()[i];
					const Type* expected_element_type = s->get_element_types()[i];
					Reference<Expression> new_element = handle_expression(element, expected_element_type);
					if (new_element == nullptr) {
						return Reference<Expression>();
					}
					if (new_element->get_type() != expected_element_type) {
						add_error(element, "invalid type %, expected type %", PrintTypeName(new_element->get_type()), PrintTypeName(expected_element_type));
						return Reference<Expression>();
					}
					elements.push_back(std::move(new_element));
				}
				return new ArrayLiteral(std::move(elements), expected_type);
			}
			else {
				std::vector<const Type*> element_types;
				std::vector<Reference<Expression>> elements;
				for (const Expression* element: e->get_elements()) {
					Reference<Expression> new_element = handle_expression(element);
					if (new_element == nullptr) {
						return Reference<Expression>();
					}
					element_types.push_back(new_element->get_type());
					elements.push_back(std::move(new_element));
				}
				const Type* type = get_tuple_type(std::move(element_types));
				return new ArrayLiteral(std::move(elements), type);
			}
		}
		else if (auto* e = as<Name>(expression)) {
			return handle_name(e);
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			Reference<Expression> left = handle_expression(e->get_left());
			Reference<Expression> right = handle_expression(e->get_right());
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			if (as<IntType>(left->get_type()) && as<IntType>(right->get_type())) {
				return new BinaryExpression(e->get_operation(), std::move(left), std::move(right), get_int_type());
			}
			else {
				add_error(expression, "invalid binary expression");
				return Reference<Expression>();
			}
		}
		else if (auto* e = as<Assignment>(expression)) {
			Reference<Expression> left = handle_name(e->get_left());
			const Type* type = get_type(left);
			Reference<Expression> right = handle_expression(e->get_right(), type);
			if (left == nullptr || right == nullptr) {
				return Reference<Expression>();
			}
			if (!check_type(right, type)) {
				return Reference<Expression>();
			}
			return new Assignment(std::move(left), std::move(right), type);
		}
		else if (auto* e = as<Call>(expression)) {
			StringView name;
			std::vector<Reference<Expression>> arguments;
			// uniform function call syntax
			if (auto* accessor = as<Accessor>(e->get_expression())) {
				name = get_constant_string(accessor->get_right());
				arguments.push_back(handle_expression(accessor->get_left()));
			}
			else {
				name = get_name(e->get_expression());
			}
			for (const Expression* argument: e->get_arguments()) {
				arguments.push_back(handle_expression(argument));
			}
			const Entity* function = get_function(name, arguments, expected_type, expression);
			if (function == nullptr) {
				return Reference<Expression>();
			}
			const Type* return_type = get_return_type(function);
			if (return_type == nullptr) {
				add_error(expression, "recursive call to function with deduced return type");
				return Reference<Expression>();
			}
			return new Call(new EntityReference(function), std::move(arguments), return_type);
		}
		else if (auto* e = as<Accessor>(expression)) {
			Reference<Expression> left = handle_expression(e->get_left());
			const Type* left_type = get_type(left);
			if (left_type == nullptr) {
				return Reference<Expression>();
			}
			if (as<StructureInstantiation>(left_type)) {
				StringView member_name = get_constant_string(e->get_right());
				const Type* type = get_member_type(left_type, member_name, expression);
				if (type == nullptr) {
					return Reference<Expression>();
				}
				return new Accessor(std::move(left), new StringLiteral(member_name.to_string()), type);
			}
			else if (auto* t = as<TupleType>(left_type)) {
				const std::int32_t* index = get_constant_int(e->get_right());
				if (index == nullptr) {
					return Reference<Expression>();
				}
				if (*index < 0 || *index >= t->get_element_types().size()) {
					add_error(expression, "index out of bounds");
					return Reference<Expression>();
				}
				const Type* type = t->get_element_types()[*index];
				return new Accessor(std::move(left), new IntLiteral(*index), type);
			}
			else {
				add_error(expression, "invalid accessor");
				return Reference<Expression>();
			}
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
		ScopeMap<Index> new_scope(variables);
		variables = &new_scope;
		for (std::size_t i = 0; i < block->get_statements().size(); ++i) {
			const Statement* statement = block->get_statements()[i];
			Reference<Statement> new_statement = handle_statement(statement);
			if (new_statement == nullptr || is_empty_statement(new_statement)) {
				continue;
			}
			const bool is_final = is_final_statement(new_statement);
			statements.push_back(std::move(new_statement));
			if (is_final) {
				if (i + 1 < block->get_statements().size()) {
					add_warning(Reference<Expression>(), "unreachable code in function \"%\"", current_function->get_name());
				}
				break;
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
			const StringView name = as<Name>(s->get_variable())->get_name();
			const Type* type = handle_type(s->get_type());
			Reference<Expression> expression = handle_expression(s->get_expression(), type);
			if (s->get_type() == nullptr) {
				type = get_type(expression);
			}
			// add the variable even if type is null
			const unsigned int index = current_function->add_variable(type);
			variables->set(name, index);
			if (type == nullptr || expression == nullptr) {
				return Reference<Statement>();
			}
			if (!check_type(expression, type)) {
				return Reference<Statement>();
			}
			return new LetStatement(new Variable(index), Reference<Expression>(), std::move(expression));
		}
		else if (auto* s = as<IfStatement>(statement)) {
			Reference<Expression> condition = handle_expression(s->get_condition(), get_int_type());
			Block then_block = handle_block(s->get_then_block());
			Block else_block = handle_block(s->get_else_block());
			if (condition == nullptr) {
				return Reference<Statement>();
			}
			if (!check_type(condition, get_int_type())) {
				return Reference<Statement>();
			}
			return new IfStatement(std::move(condition), std::move(then_block), std::move(else_block));
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			auto previous_current_loop = this->current_loop;
			this->current_loop = s;
			Reference<Expression> condition = handle_expression(s->get_condition(), get_int_type());
			Block block = handle_block(s->get_block());
			this->current_loop = previous_current_loop;
			if (condition == nullptr) {
				return Reference<Statement>();
			}
			if (!check_type(condition, get_int_type())) {
				return Reference<Statement>();
			}
			return new WhileStatement(std::move(condition), std::move(block));
		}
		else if (auto* s = as<ReturnStatement>(statement)) {
			const Type* return_type = current_function->get_return_type();
			Reference<Expression> expression = handle_expression(s->get_expression(), return_type);
			if (s->get_expression()) {
				if (expression == nullptr) {
					return Reference<Statement>();
				}
				if (return_type == nullptr) {
					return_type = expression->get_type();
					current_function->set_return_type(return_type);
				}
				if (!check_type(expression, return_type)) {
					return Reference<Statement>();
				}
			}
			else {
				if (return_type == nullptr) {
					return_type = get_void_type();
					current_function->set_return_type(return_type);
				}
				if (!as<VoidType>(return_type)) {
					add_error(Reference<Expression>(), "return without value in function \"%\" with return type %", current_function->get_name(), PrintTypeName(return_type));
					return Reference<Statement>();
				}
			}
			return new ReturnStatement(std::move(expression));
		}
		else if (auto* s = as<BreakStatement>(statement)) {
			if (current_loop == nullptr) {
				add_error(Reference<Expression>(), "break statement outside of a loop in function \"%\"", current_function->get_name());
				return Reference<Statement>();
			}
			return new BreakStatement();
		}
		else if (auto* s = as<ContinueStatement>(statement)) {
			if (current_loop == nullptr) {
				add_error(Reference<Expression>(), "continue statement outside of a loop in function \"%\"", current_function->get_name());
				return Reference<Statement>();
			}
			return new ContinueStatement();
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			Reference<Expression> expression = handle_expression(s->get_expression());
			if (expression == nullptr) {
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
		const Entity* main_function = get_function("main", {}, get_void_type());
		if (main_function == nullptr) {
			return;
		}
		program->set_main_function(main_function);
	}
};

void pass1(Program* program, Errors& errors) {
	Pass1(program, &errors).run();
}

class MemoryManagement {
	Program* program;
	std::size_t loop_depth = 0;
	void handle_function(FunctionInstantiation* function) {
		Block* block = function->get_block();
		for (std::size_t i = 0; i < function->get_arguments().size(); ++i) {
			const unsigned int variable = i;
			destroy_variable(block, 0, variable, true);
		}
		find_variables(block);
	}
	void find_variables(Block* block) {
		for (std::size_t i = 0; i < block->get_statements().size(); ++i) {
			Statement* statement = block->get_statements()[i];
			if (auto* s = as<BlockStatement>(statement)) {
				find_variables(s->get_block());
			}
			else if (auto* s = as<LetStatement>(statement)) {
				const unsigned int variable = as<Variable>(s->get_variable())->get_index();
				destroy_variable(block, i + 1, variable, true);
			}
			else if (auto* s = as<IfStatement>(statement)) {
				find_variables(s->get_then_block());
				find_variables(s->get_else_block());
			}
			else if (auto* s = as<WhileStatement>(statement)) {
				find_variables(s->get_block());
			}
		}
	}
	void destroy_variable(Block* block, std::size_t i, unsigned int variable, bool destroy) {
		for (; i < block->get_statements().size(); ++i) {
			Statement* statement = block->get_statements()[i];
			if (auto* s = as<BlockStatement>(statement)) {
				destroy_variable(s->get_block(), 0, variable, false);
			}
			else if (auto* s = as<IfStatement>(statement)) {
				destroy_variable(s->get_then_block(), 0, variable, false);
				destroy_variable(s->get_else_block(), 0, variable, false);
			}
			else if (auto* s = as<WhileStatement>(statement)) {
				++loop_depth;
				destroy_variable(s->get_block(), 0, variable, false);
				--loop_depth;
			}
		}
		Statement* last_statement = block->get_last_statement();
		if (as<ReturnStatement>(last_statement)) {
			destroy = true;
		}
		else if (as<BreakStatement>(last_statement) && loop_depth == 0) {
			destroy = true;
		}
		else if (as<ContinueStatement>(last_statement) && loop_depth == 0) {
			destroy = true;
		}
		if (destroy) {
			if (ReturnStatement* return_statement = as<ReturnStatement>(last_statement)) {
				return_statement->add_destroy_variable(variable);
			}
			else {
				block->add_statement(new DestroyStatement(variable));
			}
		}
	}
public:
	MemoryManagement(Program* program): program(program) {}
	void run() {
		for (Entity* entity: program->get_entities()) {
			if (FunctionInstantiation* function = as<FunctionInstantiation>(entity)) {
				handle_function(function);
			}
		}
	}
};

void memory_management(Program* program) {
	MemoryManagement(program).run();
}
