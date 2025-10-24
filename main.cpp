#include "parser.hpp"
#include "codegen_c.hpp"

static void compile(const std::string& path, Errors& errors) {
	Program program;
	parse_program(path.c_str(), program, errors);
	if (errors) {
		return;
	}
	std::ofstream output(path + ".c");
	codegen_c(output, program);
}

int main(int argc, const char** argv) {
	using namespace printer;
	if (argc <= 1) {
		return 1;
	}
	Errors errors;
	compile(argv[1], errors);
	errors.print();
	if (errors) {
		return 1;
	}
	print(ln(bold(green("success"))));
}
