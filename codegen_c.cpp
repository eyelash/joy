#include "codegen_c.hpp"
#include "parsley/printer.hpp"

using namespace printer;

void codegen_c(std::ostream& ostream, const Program& program) {
	Context context(ostream);
	for (const Function& function: program.get_functions()) {
		print(context, ln(function.get_name()));
	}
}
