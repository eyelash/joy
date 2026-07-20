#include "parser.hpp"
#include "passes.hpp"
#include "codegen_c.hpp"

struct Success {
	static constexpr const char* severity = "success";
	using Color = printer::Green;
};

template <class... A> void print_success(A&&... a) {
	printer::Context context(StandardOutput::get());
	printer::print_diagnostic<Success>(context, std::forward<A>(a)...);
	context.print('\n');
}

static void compile(int argc, const char** argv, Diagnostics& diagnostics) {
	if (argc <= 1) {
		diagnostics.add_error("no input file");
		return;
	}
	std::string path(argv[1]);
	Reference<Program> program = parse_program(path.c_str(), diagnostics);
	if (diagnostics.has_error()) {
		return;
	}
	pass1(program, diagnostics);
	if (diagnostics.has_error()) {
		return;
	}
	memory_management(program);
	std::string output_path = path + ".c";
	WriteFile file(output_path.c_str());
	BufferedOutput output(file);
	codegen_c(output, program);
	diagnostics.print();
	print_success(printer::format("\"%\" generated", StringView(output_path)));
}

int main(int argc, const char** argv) {
	Diagnostics diagnostics;
	compile(argc, argv, diagnostics);
	if (diagnostics.has_error()) {
		diagnostics.print();
		return 1;
	}
}
