#include "ast.hpp"
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

class IntCollector {
	std::int32_t value = 0;
public:
	void push(char c) {
		value *= 10;
		value += c - '0';
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(make_expr<IntLiteral>(value));
	}
};

constexpr auto int_literal = collect<IntCollector>(one_or_more(numeric_char));

class ExpressionCollector {
	Reference<Expression> expression;
public:
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(expression));
	}
};

struct expression;
constexpr auto expression = reference<struct expression>();
struct expression {
	static constexpr auto parser = pratt<ExpressionCollector>(
		pratt_level(
			terminal(choice(
				int_literal,
				ignore(identifier),
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
	Reference<Expression> expression;
	const Result result = parse_impl(program, context, GetValueCallback<Reference<Expression>>(expression));
	if (result == ERROR) {
		print_error(path, context.get_source(), context.get_position(), context.get_error());
		return 1;
	}
	if (result == FAILURE) {
		print(ln(bold(yellow("failure"))));
		return 1;
	}
	print(ln(bold(green("success"))));
	if (IntLiteral* int_literal = expr_cast<IntLiteral>(expression)) {
		print(ln(print_number(int_literal->get_value())));
	}
}
