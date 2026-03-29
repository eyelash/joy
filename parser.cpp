#include "parser.hpp"
#include "parsley/pratt.hpp"

using namespace parser;

template <class T> class NewMapper {
public:
	constexpr NewMapper() {}
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(new T(std::forward<A>(a)...));
	}
};

template <class OuterT, class InnerT = OuterT> class ReferenceMapper {
public:
	constexpr ReferenceMapper() {}
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(Reference<OuterT>(new InnerT(std::forward<A>(a)...)));
	}
};

template <class T> using ExpressionMapper = ReferenceMapper<Expression, T>;
template <class T> using StatementMapper = ReferenceMapper<Statement, T>;

class StringCollector {
	std::string name;
public:
	void push(char c) {
		name.push_back(c);
	}
	void push(std::uint32_t code_point) {
		name.append(from_code_point(code_point));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(name));
	}
};

template <std::uint32_t base> class BaseTag {
public:
	constexpr BaseTag() {}
};

template <std::uint32_t base, char start_char, std::uint32_t start_value> class DigitMapper {
public:
	constexpr DigitMapper() {}
	template <class C> static void map(const C& callback, char c) {
		callback.push(start_value + (c - start_char), BaseTag<base>());
	}
};

class NumberCollector {
	std::uint32_t value = 0;
public:
	template <std::uint32_t base> void push(std::uint32_t digit, BaseTag<base>) {
		value *= base;
		value += digit;
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(value);
	}
};

template <std::uint32_t value> using BoolLiteralCollector = ConstantCollector<std::uint32_t, value>;

template <char c> using EscapeCollector = ConstantCollector<char, c>;

template <class T> class NudTag {
public:
	constexpr NudTag() {}
};

template <class T> class LedTag {
public:
	constexpr LedTag() {}
};

template <BinaryOperation operation> class BinaryOperationTag {
public:
	constexpr BinaryOperationTag() {}
};

class ExpressionCollector {
	Reference<Expression> expression;
public:
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	template <class T, class... A> void push(NudTag<T>, A&&... a) {
		expression = new T(std::forward<A>(a)...);
	}
	template <class T, class... A> void push(LedTag<T>, A&&... a) {
		expression = new T(std::move(expression), std::forward<A>(a)...);
	}
	template <BinaryOperation operation> void push(BinaryOperationTag<operation>, Reference<Expression>&& right) {
		expression = new BinaryExpression(operation, std::move(expression), std::move(right));
	}
	void set_location(const SourceLocation& location) {
		expression->set_location(location);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(expression));
	}
};

template <BinaryOperation operation> using OperationCollector = MapCollector<TagMapper<BinaryOperationTag<operation>>, TupleCollector<Reference<Expression>>>;

class ProgramCollector {
	std::vector<Reference<Function>> functions;
	std::vector<Reference<Structure>> structures;
public:
	void push(Reference<Function>&& function) {
		functions.push_back(std::move(function));
	}
	void push(Reference<Structure>&& structure) {
		structures.push_back(std::move(structure));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new Program(std::move(functions), std::move(structures)));
	}
};

constexpr auto whitespace_char = choice(' ', '\t', '\n', '\r');

constexpr auto identifier_start_char = choice(range('a', 'z'), range('A', 'Z'), '_');
constexpr auto identifier_char = choice(identifier_start_char, range('0', '9'));

constexpr auto binary_digit = map<DigitMapper<2, '0', 0>>(range('0', '1'));
constexpr auto octal_digit = map<DigitMapper<8, '0', 0>>(range('0', '7'));
constexpr auto decimal_digit = map<DigitMapper<10, '0', 0>>(range('0', '9'));
constexpr auto hexadecimal_digit = choice(
	map<DigitMapper<16, '0', 0>>(range('0', '9')),
	map<DigitMapper<16, 'a', 10>>(range('a', 'f')),
	map<DigitMapper<16, 'A', 10>>(range('A', 'F'))
);

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
	return sequence(ignore(s), not_(identifier_char));
}

template <class P> constexpr auto operator_(P p) {
	return sequence(whitespace, ignore(p), whitespace);
}

template <class P> constexpr auto comma_separated(P p) {
	return optional(sequence(p, whitespace, zero_or_more(sequence(ignore(','), whitespace, p, whitespace))));
}

constexpr auto identifier = collect<StringCollector>(sequence(identifier_start_char, zero_or_more(identifier_char)));
constexpr auto expect_identifier = choice(identifier, error("expected an identifier"));

using IntLiteralCollector = MapCollector<ExpressionMapper<IntLiteral>, NumberCollector>;

constexpr auto int_literal = collect<IntLiteralCollector>(choice(
	sequence(ignore("0b"), one_or_more(binary_digit)),
	sequence(ignore("0o"), one_or_more(octal_digit)),
	sequence(ignore("0x"), one_or_more(hexadecimal_digit)),
	one_or_more(decimal_digit)
));

constexpr auto bool_literal = map<ExpressionMapper<IntLiteral>>(choice(
	collect<BoolLiteralCollector<0>>(keyword("false")),
	collect<BoolLiteralCollector<1>>(keyword("true"))
));

constexpr auto escape = sequence(
	ignore('\\'),
	choice(
		collect<EscapeCollector<'\n'>>('n'),
		collect<EscapeCollector<'\\'>>('\\'),
		collect<EscapeCollector<'"'>>('"'),
		collect<EscapeCollector<'\''>>('\''),
		collect<EscapeCollector<'\0'>>('0'),
		collect<NumberCollector>(sequence(ignore("u{"), one_or_more(hexadecimal_digit), expect("}"))),
		error("invalid escape sequence")
	)
);

using CharLiteralCollector = MapCollector<ExpressionMapper<CharLiteral>, StringCollector>;
using StringLiteralCollector = MapCollector<ExpressionMapper<StringLiteral>, StringCollector>;

constexpr auto char_literal = collect<CharLiteralCollector>(sequence(
	ignore('\''),
	zero_or_more(choice(
		escape,
		sequence(not_('\''), any_char())
	)),
	expect("'")
));

constexpr auto string_literal = collect<StringLiteralCollector>(sequence(
	ignore('"'),
	zero_or_more(choice(
		escape,
		sequence(not_('"'), any_char())
	)),
	expect("\"")
));

DECLARE_PARSER(type)
DECLARE_PARSER(expression)
DECLARE_PARSER(statement)
DECLARE_PARSER(branch)

using ArrayLiteralCollector = MapCollector<ExpressionMapper<ArrayLiteral>, VectorCollector<Reference<Expression>>>;

constexpr auto array_literal = collect<ArrayLiteralCollector>(sequence(
	ignore('['),
	whitespace,
	comma_separated(sequence(
		not_(']'),
		not_(end()),
		expression
	)),
	whitespace,
	expect("]")
));

using StructLiteralCollector = MapCollector<TagMapper<NudTag<StructLiteral>>, TupleCollector<Reference<Expression>, std::vector<StructLiteral::Member>>>;
using StructLiteralMemberCollector = MapCollector<ConstructorMapper<StructLiteral::Member>, TupleCollector<std::string, Reference<Expression>>>;

constexpr auto struct_literal = collect<StructLiteralCollector>(sequence(
	keyword("new"),
	whitespace,
	type,
	whitespace,
	expect("{"),
	whitespace,
	collect<VectorCollector<StructLiteral::Member>>(comma_separated(
		collect<StructLiteralMemberCollector>(sequence(
			not_('}'),
			not_(end()),
			expect_identifier,
			whitespace,
			expect("="),
			whitespace,
			expression
		))
	)),
	whitespace,
	expect("}")
));

// JavaScript-style struct literals
constexpr auto alternative_struct_literal = collect<StructLiteralCollector>(sequence(
	ignore('{'),
	whitespace,
	collect<VectorCollector<StructLiteral::Member>>(comma_separated(
		collect<StructLiteralMemberCollector>(sequence(
			not_('}'),
			not_(end()),
			expect_identifier,
			whitespace,
			expect(":"),
			whitespace,
			expression
		))
	)),
	whitespace,
	expect("}")
));

constexpr auto name = map<ExpressionMapper<Name>>(identifier);

using CallCollector = MapCollector<TagMapper<LedTag<Call>>, VectorCollector<Reference<Expression>>>;

constexpr auto type_impl = pratt<ExpressionCollector>(
	pratt_level(
		postfix<CallCollector>(sequence(
			whitespace,
			ignore('<'),
			whitespace,
			comma_separated(sequence(
				not_('>'),
				not_(end()),
				type
			)),
			whitespace,
			expect(">")
		))
	),
	pratt_level(
		terminal(choice(
			name,
			error("expected a type")
		))
	)
);
DEFINE_PARSER(type, type_impl)

using AssignmentCollector = MapCollector<TagMapper<LedTag<Assignment>>, TupleCollector<Reference<Expression>>>;

using AccessorCollector = MapCollector<TagMapper<LedTag<Accessor>>, TupleCollector<Reference<Expression>>>;

constexpr auto expression_impl = pratt<ExpressionCollector>(
	pratt_level(
		infix_rtl<AssignmentCollector>(operator_(sequence('=', not_('='))))
	),
	pratt_level(
		infix_ltr<OperationCollector<BinaryOperation::EQ>>(operator_("==")),
		infix_ltr<OperationCollector<BinaryOperation::NE>>(operator_("!="))
	),
	pratt_level(
		infix_ltr<OperationCollector<BinaryOperation::LT>>(operator_(sequence('<', not_('=')))),
		infix_ltr<OperationCollector<BinaryOperation::LE>>(operator_("<=")),
		infix_ltr<OperationCollector<BinaryOperation::GT>>(operator_(sequence('>', not_('=')))),
		infix_ltr<OperationCollector<BinaryOperation::GE>>(operator_(">="))
	),
	pratt_level(
		infix_ltr<OperationCollector<BinaryOperation::ADD>>(operator_('+')),
		infix_ltr<OperationCollector<BinaryOperation::SUB>>(operator_('-'))
	),
	pratt_level(
		infix_ltr<OperationCollector<BinaryOperation::MUL>>(operator_('*')),
		infix_ltr<OperationCollector<BinaryOperation::DIV>>(operator_('/')),
		infix_ltr<OperationCollector<BinaryOperation::REM>>(operator_('%'))
	),
	pratt_level(
		postfix<CallCollector>(sequence(
			whitespace,
			ignore('('),
			whitespace,
			comma_separated(sequence(
				not_(')'),
				not_(end()),
				expression
			)),
			whitespace,
			expect(")")
		)),
		postfix<AccessorCollector>(sequence(
			whitespace,
			ignore('.'),
			whitespace,
			map<ExpressionMapper<StringLiteral>>(expect_identifier)
		)),
		postfix<AccessorCollector>(sequence(
			whitespace,
			ignore('['),
			whitespace,
			expression,
			whitespace,
			expect("]")
		))
	),
	pratt_level(
		terminal(choice(
			sequence(ignore('('), whitespace, expression, whitespace, expect(")")),
			string_literal,
			char_literal,
			array_literal,
			alternative_struct_literal,
			struct_literal,
			bool_literal,
			int_literal,
			name,
			error("expected an expression")
		))
	)
);
DEFINE_PARSER(expression, expression_impl)

using BlockCollector = MapCollector<ConstructorMapper<Block>, VectorCollector<Reference<Statement>>>;

static constexpr auto block = collect<BlockCollector>(sequence(
	ignore('{'),
	whitespace,
	zero_or_more(sequence(
		not_('}'),
		not_(end()),
		statement,
		whitespace
	)),
	expect("}")
));

using EmptyStatementCollector = MapCollector<StatementMapper<EmptyStatement>, TupleCollector<>>;
using LetStatementCollector = MapCollector<StatementMapper<LetStatement>, TupleCollector<std::string, Reference<Expression>, Reference<Expression>>>;
using IfStatementCollector = MapCollector<StatementMapper<IfStatement>, TupleCollector<Reference<Expression>, Block, Block>>;
using WhileStatementCollector = MapCollector<StatementMapper<WhileStatement>, TupleCollector<Reference<Expression>, Block>>;
using ReturnStatementCollector = MapCollector<StatementMapper<ReturnStatement>, TupleCollector<Reference<Expression>>>;
using ExpressionStatementCollector = MapCollector<StatementMapper<ExpressionStatement>, TupleCollector<Reference<Expression>>>;

constexpr auto empty_statement = collect<EmptyStatementCollector>(ignore(';'));

constexpr auto let_statement = collect<LetStatementCollector>(sequence(
	keyword("let"),
	whitespace,
	expect_identifier,
	whitespace,
	optional(sequence(
		ignore(':'),
		whitespace,
		tag<TupleIndex<1>>(type),
		whitespace
	)),
	expect("="),
	whitespace,
	tag<TupleIndex<2>>(expression),
	whitespace,
	expect(";")
));

constexpr auto if_statement = collect<IfStatementCollector>(sequence(
	keyword("if"),
	whitespace,
	expect("("),
	whitespace,
	expression,
	whitespace,
	expect(")"),
	whitespace,
	tag<TupleIndex<1>>(branch),
	whitespace,
	optional(sequence(
		keyword("else"),
		whitespace,
		tag<TupleIndex<2>>(branch)
	))
));

constexpr auto while_statement = collect<WhileStatementCollector>(sequence(
	keyword("while"),
	whitespace,
	expect("("),
	whitespace,
	expression,
	whitespace,
	expect(")"),
	whitespace,
	branch
));

constexpr auto return_statement = collect<ReturnStatementCollector>(sequence(
	keyword("return"),
	whitespace,
	optional(sequence(
		not_(';'),
		not_(end()),
		expression,
		whitespace
	)),
	expect(";")
));

constexpr auto expression_statement = collect<ExpressionStatementCollector>(sequence(
	expression,
	whitespace,
	expect(";")
));

constexpr auto statement_impl = choice(
	map<StatementMapper<BlockStatement>>(block),
	empty_statement,
	let_statement,
	if_statement,
	while_statement,
	return_statement,
	expression_statement
);
DEFINE_PARSER(statement, statement_impl)

constexpr auto branch_impl = choice(
	block,
	map<ConstructorMapper<Block>>(choice(
		empty_statement,
		if_statement,
		while_statement,
		return_statement,
		expression_statement
	))
);
DEFINE_PARSER(branch, branch_impl)

using FunctionCollector = MapCollector<ReferenceMapper<Function>, TupleCollector<std::string, std::vector<std::string>, std::vector<Function::Argument>, Reference<Expression>, Block>>;
using FunctionArgumentCollector = MapCollector<ConstructorMapper<Function::Argument>, TupleCollector<std::string, Reference<Expression>>>;

constexpr auto function = collect<FunctionCollector>(sequence(
	keyword("func"),
	whitespace,
	expect_identifier,
	whitespace,
	optional(sequence(
		ignore('<'),
		whitespace,
		collect<VectorCollector<std::string>>(comma_separated(sequence(
			not_('>'),
			not_(end()),
			expect_identifier
		))),
		whitespace,
		expect(">"),
		whitespace
	)),
	expect("("),
	whitespace,
	collect<VectorCollector<Function::Argument>>(comma_separated(
		collect<FunctionArgumentCollector>(sequence(
			not_(')'),
			not_(end()),
			expect_identifier,
			whitespace,
			expect(":"),
			whitespace,
			type
		))
	)),
	whitespace,
	expect(")"),
	whitespace,
	optional(sequence(
		ignore(':'),
		whitespace,
		type,
		whitespace
	)),
	choice(
		block,
		error("expected a block")
	)
));

using StructureCollector = MapCollector<ReferenceMapper<Structure>, TupleCollector<std::string, std::vector<std::string>, std::vector<Structure::Member>>>;
using StructureMemberCollector = MapCollector<ConstructorMapper<Structure::Member>, TupleCollector<std::string, Reference<Expression>>>;

constexpr auto structure = collect<StructureCollector>(sequence(
	keyword("struct"),
	whitespace,
	expect_identifier,
	whitespace,
	optional(sequence(
		ignore('<'),
		whitespace,
		collect<VectorCollector<std::string>>(comma_separated(sequence(
			not_('>'),
			not_(end()),
			expect_identifier
		))),
		whitespace,
		expect(">"),
		whitespace
	)),
	expect("{"),
	whitespace,
	collect<VectorCollector<Structure::Member>>(comma_separated(
		collect<StructureMemberCollector>(sequence(
			not_('}'),
			not_(end()),
			expect_identifier,
			whitespace,
			expect(":"),
			whitespace,
			type
		))
	)),
	whitespace,
	expect("}")
));

constexpr auto program = collect<ProgramCollector>(sequence(
	whitespace,
	zero_or_more(sequence(
		not_(end()),
		choice(
			function,
			structure,
			error("expected a toplevel declaration")
		),
		whitespace
	))
));

void parse_program(const char* path, Reference<Program>& program_, Errors& errors) {
	auto source = read_file(path);
	Context context(source);
	const Result result = parse_impl(program, context, GetValueCallback<Reference<Program>>(program_));
	if (result == ERROR) {
		errors.add_error(path, context.get_location(), context.get_error());
		return;
	}
#ifndef NDEBUG
	if (result == FAILURE) {
		errors.add_error(path, context.get_location(), "failure");
		return;
	}
#endif
	program_->set_path(path);
}
