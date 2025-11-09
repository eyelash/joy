#include "codegen_c.hpp"
#include "parsley/printer.hpp"

using namespace printer;

class PrintType {
	const Type* type;
public:
	PrintType(const Type* type): type(type) {}
	PrintType(const Expression* expression): type(expression->get_type()) {}
	void print(Context& context) const {
		print_impl(format("t%", print_number(type->get_id())), context);
	}
};

class PrintExpression {
	static const char* print_operation(BinaryOperation operation) {
		if (operation == BinaryOperation::ADD) return "+";
		if (operation == BinaryOperation::SUB) return "-";
		if (operation == BinaryOperation::MUL) return "*";
		if (operation == BinaryOperation::DIV) return "/";
		if (operation == BinaryOperation::REM) return "%";
		if (operation == BinaryOperation::EQ) return "==";
		if (operation == BinaryOperation::NE) return "!=";
		if (operation == BinaryOperation::LT) return "<";
		if (operation == BinaryOperation::LE) return "<=";
		if (operation == BinaryOperation::GT) return ">";
		if (operation == BinaryOperation::GE) return ">=";
		return "";
	}
	const Expression* expression;
public:
	PrintExpression(const Expression* expression): expression(expression) {}
	void print(Context& context) const {
		if (auto* int_literal = as<IntLiteral>(expression)) {
			print_impl(print_number(int_literal->get_value()), context);
		}
		else if (auto* name = as<Name>(expression)) {
			print_impl(name->get_name(), context);
		}
		else if (auto* binary_expression = as<BinaryExpression>(expression)) {
			print_impl(format("(% % %)", PrintExpression(binary_expression->get_left()), print_operation(binary_expression->get_operation()), PrintExpression(binary_expression->get_right())), context);
		}
		else if (auto* assignment = as<Assignment>(expression)) {
			print_impl(format("(% = %)", PrintExpression(assignment->get_left()), PrintExpression(assignment->get_right())), context);
		}
		else if (auto* call = as<Call>(expression)) {
			print_impl(format("f%(%)", print_number(call->get_function_id()), comma_separated<PrintExpression>(call->get_arguments())), context);
		}
	}
};

class PrintStatement {
	const Statement* statement;
public:
	PrintStatement(const Statement* statement): statement(statement) {}
	void print(Context& context) const;
};

class PrintBlock {
	const Block* block;
public:
	PrintBlock(const Block* block): block(block) {}
	void print(Context& context) const {
		print_impl(ln('{'), context);
		context.increase_indentation();
		for (const Statement* statement: block->get_statements()) {
			print_impl(ln(PrintStatement(statement)), context);
		}
		context.decrease_indentation();
		print_impl('}', context);
	}
};

void PrintStatement::print(Context& context) const {
	if (auto* block_statement = as<BlockStatement>(statement)) {
		print_impl(PrintBlock(block_statement->get_block()), context);
	}
	else if (auto* empty_statement = as<EmptyStatement>(statement)) {
		print_impl(';', context);
	}
	else if (auto* let_statement = as<LetStatement>(statement)) {
		print_impl(format("% % = %;", PrintType(let_statement->get_type()), let_statement->get_name(), PrintExpression(let_statement->get_expression())), context);
	}
	else if (auto* if_statement = as<IfStatement>(statement)) {
		print_impl(format("if (%) %", PrintExpression(if_statement->get_condition()), PrintStatement(if_statement->get_then_statement())), context);
		if (auto* else_statement = if_statement->get_else_statement()) {
			print_impl(format(" else %", PrintStatement(else_statement)), context);
		}
	}
	else if (auto* while_statement = as<WhileStatement>(statement)) {
		print_impl(format("while (%) %", PrintExpression(while_statement->get_condition()), PrintStatement(while_statement->get_statement())), context);
	}
	else if (auto* return_statement = as<ReturnStatement>(statement)) {
		const Expression* expression = return_statement->get_expression();
		if (expression) {
			print_impl(format("return %;", PrintExpression(expression)), context);
		}
		else {
			print_impl("return;", context);
		}
	}
	else if (auto* expression_statement = as<ExpressionStatement>(statement)) {
		print_impl(format("%;", PrintExpression(expression_statement->get_expression())), context);
	}
}

class PrintTypeDefinition {
	const Type* type;
public:
	PrintTypeDefinition(const Type* type): type(type) {}
	void print(Context& context) const {
		if (as<VoidType>(type)) {
			print_impl(format("typedef void t%;", print_number(type->get_id())), context);
		}
		else if (as<IntType>(type)) {
			print_impl(format("typedef int t%;", print_number(type->get_id())), context);
		}
		else if (auto* structure = as<StructureInstantiation>(type)) {
			print_impl(ln(format("typedef struct t% t%;", print_number(type->get_id()), print_number(type->get_id()))), context);
			print_impl(ln(format("struct t% {", print_number(type->get_id()))), context);
			context.increase_indentation();
			for (const StructureInstantiation::Member& member: structure->get_members()) {
				print_impl(ln(format("% %;", PrintType(member.get_type()), member.get_name())), context);
			}
			context.decrease_indentation();
			print_impl("};", context);
		}
	}
};

class PrintFunctionArgument {
	const FunctionInstantiation::Argument* argument;
public:
	PrintFunctionArgument(const FunctionInstantiation::Argument& argument): argument(&argument) {}
	void print(Context& context) const {
		print_impl(format("% %", PrintType(argument->get_type()), argument->get_name()), context);
	}
};

class PrintFunctionArguments {
	const FunctionInstantiation* function;
public:
	PrintFunctionArguments(const FunctionInstantiation* function): function(function) {}
	PrintFunctionArguments(const FunctionInstantiation& function): function(&function) {}
	void print(Context& context) const {
		if (function->get_arguments().empty()) {
			print_impl("void", context);
		}
		else {
			print_impl(comma_separated<PrintFunctionArgument>(function->get_arguments()), context);
		}
	}
};

class PrintFunctionDeclaration {
	const FunctionInstantiation* function;
public:
	PrintFunctionDeclaration(const FunctionInstantiation* function): function(function) {}
	void print(Context& context) const {
		print_impl(format("static % f%(%);", PrintType(function->get_return_type()), print_number(function->get_id()), PrintFunctionArguments(function)), context);
	}
};

class PrintFunctionDefinition {
	const FunctionInstantiation* function;
	bool is_builtin_print_int() const {
		return
			function->get_function()->get_name() == "print_int" &&
			function->get_function()->get_template_arguments().empty() &&
			function->get_arguments().size() == 1 &&
			as<IntType>(function->get_arguments()[0].get_type()) &&
			as<VoidType>(function->get_return_type()) &&
			function->get_block()->get_statements().empty();
	}
public:
	PrintFunctionDefinition(const FunctionInstantiation* function): function(function) {}
	void print(Context& context) const {
		print_impl(ln(format("// %", function->get_function()->get_name())), context);
		if (is_builtin_print_int()) {
			print_impl(ln("int printf(const char*, ...);"), context);
			print_impl(ln(format("static % f%(%) {", PrintType(function->get_return_type()), print_number(function->get_id()), PrintFunctionArguments(function))), context);
			print_impl(indented(ln(format("printf(\"%%d\\n\", %);", function->get_arguments()[0].get_name()))), context);
			print_impl('}', context);
		}
		else {
			print_impl(format("static % f%(%) %", PrintType(function->get_return_type()), print_number(function->get_id()), PrintFunctionArguments(function), PrintBlock(function->get_block())), context);
		}
	}
};

class PrintProgram {
	const Program* program;
public:
	PrintProgram(const Program* program): program(program) {}
	void print(Context& context) const {
		for (const Type* type: program->get_types()) {
			print_impl(ln(PrintTypeDefinition(type)), context);
		}
		for (const FunctionInstantiation* function: program->get_function_instantiations()) {
			print_impl(ln(PrintFunctionDeclaration(function)), context);
		}
		for (const FunctionInstantiation* function: program->get_function_instantiations()) {
			print_impl(ln(PrintFunctionDefinition(function)), context);
		}
		print_impl(ln("int main(void) {"), context);
		context.increase_indentation();
		print_impl(ln(format("f%();", print_number(program->get_main_function_id()))), context);
		print_impl(ln("return 0;"), context);
		context.decrease_indentation();
		print_impl(ln('}'), context);
	}
};

void codegen_c(std::ostream& ostream, const Program* program) {
	Context context(ostream);
	print(context, PrintProgram(program));
}
