#include "parser.hpp"
#include "codegen_c.hpp"

int main(int argc, const char** argv) {
	using namespace parser;
	using namespace printer;
	if (argc <= 1) {
		return 1;
	}
	std::string path = argv[1];
	auto source = read_file(path.c_str());
	parser::Context context(source);
	Program program;
	const Result result = parse_program(context, program);
	if (result == ERROR) {
		print_error(path.c_str(), context.get_source(), context.get_position(), context.get_error());
		return 1;
	}
	if (result == FAILURE) {
		print(ln(bold(yellow("failure"))));
		return 1;
	}
	print(ln(bold(green("success"))));
	std::ofstream output(path + ".c");
	codegen_c(output, program);
}
