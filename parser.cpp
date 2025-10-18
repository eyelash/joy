#include "parser.hpp"
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

constexpr auto keyword(const char* s) {
	return sequence(ignore(s), not_(alphanumeric_char));
}

template <class P> constexpr auto operator_(P p) {
	return sequence(whitespace, ignore(p), whitespace);
}

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
	void push(Reference<Expression>&& right, BinaryOperation operation) {
		expression = make_expr<BinaryExpression>(operation, std::move(expression), std::move(right));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(expression));
	}
};

template <BinaryOperation operation> class OperationMapper {
public:
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(std::forward<A>(a)..., operation);
	}
};

struct expression {
	static constexpr auto parser = pratt<ExpressionCollector>(
		pratt_level(
			infix_ltr<OperationMapper<BinaryOperation::ADD>>(operator_('+')),
			infix_ltr<OperationMapper<BinaryOperation::SUB>>(operator_('-'))
		),
		pratt_level(
			infix_ltr<OperationMapper<BinaryOperation::MUL>>(operator_('*')),
			infix_ltr<OperationMapper<BinaryOperation::DIV>>(operator_('/')),
			infix_ltr<OperationMapper<BinaryOperation::REM>>(operator_('%'))
		),
		pratt_level(
			terminal(choice(
				sequence(ignore('('), whitespace, reference<expression>(), whitespace, expect(")")),
				int_literal,
				ignore(identifier),
				error("expected an expression")
			))
		)
	);
};

constexpr auto statement = sequence(
	choice(
		sequence(
			keyword("let"),
			whitespace,
			choice(
				ignore(identifier),
				error("expected an identifier")
			),
			whitespace,
			expect("="),
			whitespace,
			reference<expression>()
		),
		reference<expression>()
	),
	whitespace,
	expect(";")
);

struct block {
	static constexpr auto parser = sequence(
		expect("{"),
		whitespace,
		zero_or_more(sequence(
			not_('}'),
			statement,
			whitespace
		)),
		expect("}")
	);
};

constexpr auto function = sequence(
	keyword("func"),
	whitespace,
	choice(
		ignore(identifier),
		error("expected an identifier")
	),
	whitespace,
	expect("("),
	whitespace,
	expect(")"),
	whitespace,
	reference<block>()
);

constexpr auto program = sequence(
	whitespace,
	zero_or_more(sequence(
		not_(end()),
		choice(
			function,
			error("expected a function")
		),
		whitespace
	))
);

Result parse_program(Context& context, Reference<Expression>& expression) {
	return parse_impl(program, context, GetValueCallback<Reference<Expression>>(expression));
}
