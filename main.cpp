#include "parser.hpp"

class PrintExpression {
	static const char* print_operation(BinaryOperation operation) {
		switch (operation) {
		case BinaryOperation::ADD:
			return "+";
		case BinaryOperation::SUB:
			return "-";
		case BinaryOperation::MUL:
			return "*";
		case BinaryOperation::DIV:
			return "/";
		case BinaryOperation::REM:
			return "%";
		default:
			return "";
		}
	}
	const Expression* expression;
public:
	PrintExpression(const Expression* expression): expression(expression) {}
	void print(printer::Context& context) const {
		using namespace printer;
		if (expression == nullptr) {
			return;
		}
		switch (expression->get_type_id()) {
		case TYPE_ID_INT_LITERAL:
			{
				const auto* e = static_cast<const IntLiteral*>(expression);
				print_impl(print_number(e->get_value()), context);
			}
			break;
		case TYPE_ID_BINARY_EXPRESSION:
			{
				const auto* e = static_cast<const BinaryExpression*>(expression);
				print_impl(format("(% % %)", PrintExpression(e->get_left()), print_operation(e->get_operation()), PrintExpression(e->get_right())), context);
			}
			break;
		}
	}
};

int main(int argc, const char** argv) {
	using namespace parser;
	using namespace printer;
	if (argc <= 1) {
		return 1;
	}
	const char* path = argv[1];
	auto source = read_file(path);
	parser::Context context(source);
	Program program;
	const Result result = parse_program(context, program);
	if (result == ERROR) {
		print_error(path, context.get_source(), context.get_position(), context.get_error());
		return 1;
	}
	if (result == FAILURE) {
		print(ln(bold(yellow("failure"))));
		return 1;
	}
	print(ln(bold(green("success"))));
	//print(ln(PrintExpression(expression)));
}
