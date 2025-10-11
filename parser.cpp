#include "parsley/parser.hpp"
#include "parsley/pratt.hpp"

using namespace parser;

constexpr auto whitespace_char = choice(' ', '\t', '\n', '\r');

constexpr auto numeric_char = range('0', '9');

constexpr auto alphabetic_char = choice(range('a', 'z'), range('A', 'Z'), '_');

constexpr auto alphanumeric_char = choice(alphabetic_char, numeric_char);

constexpr auto comment = choice(
	sequence("//", zero_or_more(sequence(not_("\n"), any_char()))),
	sequence("/*", zero_or_more(sequence(not_("*/"), any_char())), expect("*/"))
);

constexpr auto whitespace = ignore(sequence(
	zero_or_more(whitespace_char),
	zero_or_more(sequence(
		comment,
		zero_or_more(whitespace_char)
	))
));

constexpr auto identifier = sequence(alphabetic_char, zero_or_more(alphanumeric_char));

struct Expression;
constexpr auto expression = reference<Expression>();
struct Expression {
	static constexpr auto parser = pratt<IgnoreCallback>(
		pratt_level(
			terminal(choice(
				identifier,
				error("expected an expression")
			))
		)
	);
};

constexpr auto program = sequence(
	whitespace,
	expression,
	whitespace,
	choice(
		end(),
		error("unexpected character at end of program")
	)
);

int main(int argc, const char** argv) {
	using namespace printer;
	if (argc <= 1) {
		return 1;
	}
	const char* path = argv[1];
	auto source = read_file(path);
	parser::Context context(source);
	const Result result = parse_impl(program, context, IgnoreCallback());
	if (result == ERROR) {
		print_error(path, context.get_source(), context.get_position(), context.get_error());
		return 1;
	}
	if (result == FAILURE) {
		print(ln(bold(yellow("failure"))));
		return 1;
	}
	print(ln(bold(green("success"))));
}
