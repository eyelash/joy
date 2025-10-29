#include "passes.hpp"
#include <set>

class Scope {
	Scope* parent;
	std::set<StringView> variables;
public:
	Scope(Scope* parent = nullptr): parent(parent) {}
	Scope* get_parent() const {
		return parent;
	}
	void add_variable(const StringView& name) {
		variables.emplace(name);
	}
	bool look_up(const StringView& name) {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return true;
		}
		if (parent) {
			return parent->look_up(name);
		}
		return false;
	}
};

class NameResolution {
	const Program* program;
	Errors* errors;
	Scope* functions;
	Scope* scope;
	template <class P> void add_error(const Expression* expression, P&& p) {
		errors->add_error(program->get_path().c_str(), expression->get_location(), print_to_string(std::forward<P>(p)));
	}
	void handle_expression(const Expression* expression) {
		if (auto* e = as<Name>(expression)) {
			if (!scope->look_up(e->get_name())) {
				using namespace printer;
				add_error(expression, format("undefined variable \"%\"", e->get_name()));
			}
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			handle_expression(e->get_left());
			handle_expression(e->get_right());
		}
		else if (auto* e = as<Assignment>(expression)) {
			if (auto* name = as<Name>(e->get_left())) {
				if (!scope->look_up(name->get_name())) {
					using namespace printer;
					add_error(e->get_left(), format("undefined variable \"%\"", name->get_name()));
				}
			}
			else {
				add_error(e->get_left(), "invalid assignment");
			}
			handle_expression(e->get_right());
		}
		else if (auto* e = as<Call>(expression)) {
			if (auto* name = as<Name>(e->get_expression())) {
				if (!functions->look_up(name->get_name())) {
					using namespace printer;
					add_error(e->get_expression(), format("undefined function \"%\"", name->get_name()));
				}
			}
			else {
				add_error(e->get_expression(), "invalid call");
			}
		}
	}
	void handle_block(const Block& block) {
		Scope new_scope(scope);
		scope = &new_scope;
		for (const Statement* statement: block.get_statements()) {
			handle_statement(statement);
		}
		scope = scope->get_parent();
	}
	void handle_statement(const Statement* statement) {
		if (auto* s = as<BlockStatement>(statement)) {
			handle_block(s->get_block());
		}
		else if (auto* s = as<LetStatement>(statement)) {
			scope->add_variable(s->get_name());
			handle_expression(s->get_expression());
		}
		else if (auto* s = as<IfStatement>(statement)) {
			handle_expression(s->get_condition());
			handle_statement(s->get_then_statement());
			handle_statement(s->get_else_statement());
		}
		else if (auto* s = as<WhileStatement>(statement)) {
			handle_expression(s->get_condition());
			handle_statement(s->get_statement());
		}
		else if (auto* s = as<ExpressionStatement>(statement)) {
			handle_expression(s->get_expression());
		}
	}
public:
	NameResolution(const Program* program, Errors* errors): program(program), errors(errors), functions(nullptr), scope(nullptr) {}
	void run() {
		Scope functions;
		this->functions = &functions;
		for (const Function& function: program->get_functions()) {
			functions.add_variable(function.get_name());
		}
		for (const Function& function: program->get_functions()) {
			handle_block(function.get_block());
		}
	}
};

void name_resolution(const Program& program, Errors& errors) {
	NameResolution(&program, &errors).run();
}
