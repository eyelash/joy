#include "codegen_c.hpp"
#include "parsley/printer.hpp"

using namespace printer;

class PrintExpression {
	static const char* print_operation(BinaryOperation operation) {
		if (operation == BinaryOperation::ADD) return "+";
		if (operation == BinaryOperation::SUB) return "-";
		if (operation == BinaryOperation::MUL) return "*";
		if (operation == BinaryOperation::DIV) return "/";
		if (operation == BinaryOperation::REM) return "%";
		return "";
	}
	const Expression* expression;
public:
	PrintExpression(const Expression* expression): expression(expression) {}
	void print(Context& context) const {
		if (auto* int_literal = as<IntLiteral>(expression)) {
			print_impl(print_number(int_literal->get_value()), context);
		}
		else if (auto* binary_expression = as<BinaryExpression>(expression)) {
			print_impl(format("(% % %)", PrintExpression(binary_expression->get_left()), print_operation(binary_expression->get_operation()), PrintExpression(binary_expression->get_right())), context);
		}
	}
};

class PrintFunction {
	const Function* function;
public:
	PrintFunction(const Function* function): function(function) {}
	void print(Context& context) const {
		print_impl(ln(format("void %(void) {", function->get_name())), context);
		context.increase_indentation();
		for (const Statement* statement: function->get_block().get_statements()) {
			if (auto* expression_statement = as<ExpressionStatement>(statement)) {
				print_impl(ln(format("%;", PrintExpression(expression_statement->get_expression()))), context);
			}
		}
		context.decrease_indentation();
		print_impl(ln('}'), context);
	}
};

void codegen_c(std::ostream& ostream, const Program& program) {
	Context context(ostream);
	for (const Function& function: program.get_functions()) {
		print(context, PrintFunction(&function));
	}
}
