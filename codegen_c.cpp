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

class PrintStructLiteralMember {
	const StructLiteral::Member* member;
public:
	PrintStructLiteralMember(const StructLiteral::Member* member): member(member) {}
	PrintStructLiteralMember(const StructLiteral::Member& member): member(&member) {}
	void print(Context& context) const;
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
		if (auto* e = as<IntLiteral>(expression)) {
			print_impl(print_number(e->get_value()), context);
		}
		else if (auto* e = as<ArrayLiteral>(expression)) {
			print_impl(format("((%){%})", PrintType(expression), comma_separated<PrintExpression>(e->get_elements())), context);
		}
		else if (auto* e = as<StructLiteral>(expression)) {
			print_impl(format("((%){%})", PrintType(expression), comma_separated<PrintStructLiteralMember>(e->get_members())), context);
		}
		else if (auto* e = as<Name>(expression)) {
			print_impl(e->get_name(), context);
		}
		else if (auto* e = as<Variable>(expression)) {
			print_impl(format("v%", print_number(e->get_index())), context);
		}
		else if (auto* e = as<BinaryExpression>(expression)) {
			print_impl(format("(% % %)", PrintExpression(e->get_left()), print_operation(e->get_operation()), PrintExpression(e->get_right())), context);
		}
		else if (auto* e = as<Assignment>(expression)) {
			print_impl(format("(% = %)", PrintExpression(e->get_left()), PrintExpression(e->get_right())), context);
		}
		else if (auto* e = as<Call>(expression)) {
			const Entity* function = as<EntityReference>(e->get_expression())->get_entity();
			print_impl(format("f%(%)", print_number(function->get_id()), comma_separated<PrintExpression>(e->get_arguments())), context);
		}
		else if (auto* e = as<Accessor>(expression)) {
			const Type* left_type = e->get_left()->get_type();
			if (as<StructureInstantiation>(e->get_left()->get_type())) {
				const StringView member_name = as<StringLiteral>(e->get_right())->get_string();
				print_impl(format("%.%", PrintExpression(e->get_left()), member_name), context);
			}
			else if (as<TupleType>(left_type)) {
				const std::int32_t index = as<IntLiteral>(e->get_right())->get_value();
				print_impl(format("%.v%", PrintExpression(e->get_left()), print_number(index)), context);
			}
		}
	}
};

void PrintStructLiteralMember::print(Context& context) const {
	print_impl(PrintExpression(member->get_expression()), context);
}

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
	if (auto* s = as<BlockStatement>(statement)) {
		print_impl(PrintBlock(s->get_block()), context);
	}
	else if (auto* s = as<EmptyStatement>(statement)) {
		print_impl(';', context);
	}
	else if (auto* s = as<LetStatement>(statement)) {
		print_impl(format("% % = %;", PrintType(s->get_expression()), PrintExpression(s->get_variable()), PrintExpression(s->get_expression())), context);
	}
	else if (auto* s = as<IfStatement>(statement)) {
		print_impl(format("if (%) % else %", PrintExpression(s->get_condition()), PrintBlock(s->get_then_block()), PrintBlock(s->get_else_block())), context);
	}
	else if (auto* s = as<WhileStatement>(statement)) {
		print_impl(format("while (%) %", PrintExpression(s->get_condition()), PrintBlock(s->get_block())), context);
	}
	else if (auto* s = as<ReturnStatement>(statement)) {
		const Expression* expression = s->get_expression();
		if (expression) {
			print_impl(format("return %;", PrintExpression(expression)), context);
		}
		else {
			print_impl("return;", context);
		}
	}
	else if (auto* s = as<ExpressionStatement>(statement)) {
		print_impl(format("%;", PrintExpression(s->get_expression())), context);
	}
}

/*class PrintFunctionArgument {
	const FunctionInstantiation::Argument* argument;
public:
	PrintFunctionArgument(const FunctionInstantiation::Argument& argument): argument(&argument) {}
	void print(Context& context) const {
		print_impl(format("% %", PrintType(argument->get_type()), argument->get_name()), context);
	}
};*/

class PrintFunctionArguments {
	const FunctionInstantiation* function;
public:
	PrintFunctionArguments(const FunctionInstantiation* function): function(function) {}
	PrintFunctionArguments(const FunctionInstantiation& function): function(&function) {}
	void print(Context& context) const {
		if (function->get_arguments() == 0) {
			print_impl("void", context);
		}
		else {
			for (unsigned int i = 0; i < function->get_arguments(); ++i) {
				if (i > 0) {
					print_impl(", ", context);
				}
				print_impl(format("% v%", PrintType(function->get_variable(i)), print_number(i)), context);
			}
		}
	}
};

class PrintDeclaration {
	const Entity* entity;
public:
	PrintDeclaration(const Entity* entity): entity(entity) {}
	void print(Context& context) const {
		if (as<VoidType>(entity)) {
			print_impl(format("typedef void t%;", print_number(entity->get_id())), context);
		}
		else if (as<IntType>(entity)) {
			print_impl(format("typedef int t%;", print_number(entity->get_id())), context);
		}
		else if (as<TupleType>(entity)) {
			print_impl(format("typedef struct t% t%;", print_number(entity->get_id()), print_number(entity->get_id())), context);
		}
		else if (as<StructureInstantiation>(entity)) {
			print_impl(format("typedef struct t% t%;", print_number(entity->get_id()), print_number(entity->get_id())), context);
		}
		else if (const BuiltinFunction* function = as<BuiltinFunction>(entity)) {
			if (function->get_name() == "__builtin_joy_print_int") {
				print_impl(format("static void f%(int n);", print_number(function->get_id())), context);
			}
		}
		else if (const FunctionInstantiation* function = as<FunctionInstantiation>(entity)) {
			print_impl(format("static % f%(%);", PrintType(function->get_return_type()), print_number(function->get_id()), PrintFunctionArguments(function)), context);
		}
	}
};

class PrintDefinition {
	const Entity* entity;
public:
	PrintDefinition(const Entity* entity): entity(entity) {}
	void print(Context& context) const {
		if (auto* t = as<TupleType>(entity)) {
			print_impl(ln(format("struct t% {", print_number(t->get_id()))), context);
			context.increase_indentation();
			for (std::size_t i = 0; i < t->get_element_types().size(); ++i) {
				const Type* element_type = t->get_element_types()[i];
				print_impl(ln(format("% v%;", PrintType(element_type), print_number(i))), context);
			}
			context.decrease_indentation();
			print_impl(ln("};"), context);
		}
		else if (auto* structure = as<StructureInstantiation>(entity)) {
			print_impl(ln(format("// %", PrintTypeName(structure))), context);
			print_impl(ln(format("struct t% {", print_number(structure->get_id()))), context);
			context.increase_indentation();
			for (const StructureInstantiation::Member& member: structure->get_members()) {
				print_impl(ln(format("% %;", PrintType(member.get_type()), member.get_name())), context);
			}
			context.decrease_indentation();
			print_impl(ln("};"), context);
		}
		else if (const BuiltinFunction* function = as<BuiltinFunction>(entity)) {
			if (function->get_name() == "__builtin_joy_print_int") {
				print_impl(ln("int printf(const char*, ...);"), context);
				print_impl(ln(format("static void f%(int n) {", print_number(function->get_id()))), context);
				print_impl(indented(ln("printf(\"%d\\n\", n);")), context);
				print_impl(ln('}'), context);
			}
		}
		else if (const FunctionInstantiation* function = as<FunctionInstantiation>(entity)) {
			print_impl(ln(format("// %", PrintFunctionSignature(function))), context);
			print_impl(ln(format("static % f%(%) %", PrintType(function->get_return_type()), print_number(function->get_id()), PrintFunctionArguments(function), PrintBlock(function->get_block()))), context);
		}
	}
};

class PrintProgram {
	const Program* program;
public:
	PrintProgram(const Program* program): program(program) {}
	void print(Context& context) const {
		for (const Entity* entity: program->get_entities()) {
			print_impl(ln(PrintDeclaration(entity)), context);
		}
		for (const Entity* entity: program->get_entities()) {
			print_impl(PrintDefinition(entity), context);
		}
		const Entity* main_function = program->get_main_function();
		print_impl(ln("int main(void) {"), context);
		context.increase_indentation();
		print_impl(ln(format("f%();", print_number(main_function->get_id()))), context);
		print_impl(ln("return 0;"), context);
		context.decrease_indentation();
		print_impl(ln('}'), context);
	}
};

void codegen_c(std::ostream& ostream, const Program* program) {
	Context context(ostream);
	print(context, PrintProgram(program));
}
