#include "parser.hpp"
#include "passes.hpp"
#include "codegen_c.hpp"

static void compile(const std::string& path, Diagnostics& diagnostics) {
	Reference<Program> program = parse_program(path.c_str(), diagnostics);
	if (diagnostics.has_error()) {
		return;
	}
	pass1(program, diagnostics);
	if (diagnostics.has_error()) {
		return;
	}
	memory_management(program);
	std::ofstream output(path + ".c");
	codegen_c(output, program);
}

int main(int argc, const char** argv) {
	using namespace printer;
	if (argc <= 1) {
		return 1;
	}
	Diagnostics diagnostics;
	compile(argv[1], diagnostics);
	diagnostics.print();
	if (diagnostics.has_error()) {
		return 1;
	}
	print(ln(bold(green("success"))));
}
