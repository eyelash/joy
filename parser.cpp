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

template <std::uint32_t value> class BoolLiteralCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(value);
	}
};

template <char c> class EscapeCollector {
public:
	template <class... A> void push(A&&...) {}
	template <class C> void retrieve(const C& callback) {
		callback.push(c);
	}
};

using ArrayLiteralCollector = MapCollector<NewMapper<ArrayLiteral>, VectorCollector<Reference<Expression>>>;

class StructLiteralCollector {
	Reference<Expression> type;
	std::vector<StructLiteral::Member> members;
public:
	void push(Reference<Expression>&& type) {
		this->type = std::move(type);
	}
	void push(std::string&& name, Reference<Expression>&& expression) {
		members.emplace_back(std::move(name), std::move(expression));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new StructLiteral(std::move(type), std::move(members)));
	}
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
	void push(std::uint32_t value) {
		expression = new IntLiteral(value);
	}
	void push(Tag<CharLiteral>, std::string&& string) {
		expression = new CharLiteral(std::move(string));
	}
	void push(Tag<StringLiteral>, std::string&& string) {
		expression = new StringLiteral(std::move(string));
	}
	template <BinaryOperation operation> void push(BinaryOperationTag<operation>, Reference<Expression>&& right) {
		expression = new BinaryExpression(operation, std::move(expression), std::move(right));
	}
	void push(Tag<Assignment>, Reference<Expression>&& right) {
		expression = new Assignment(std::move(expression), std::move(right));
	}
	void push(Tag<Call>, std::vector<Reference<Expression>>&& arguments) {
		expression = new Call(std::move(expression), std::move(arguments));
	}
	void push(Tag<MemberAccess>, std::string&& member_name) {
		expression = new MemberAccess(std::move(expression), std::move(member_name));
	}
	void set_location(const SourceLocation& location) {
		expression->set_location(location);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(expression));
	}
};

template <BinaryOperation operation> using OperationCollector = MapCollector<TagMapper<BinaryOperationTag<operation>>, TupleCollector<Reference<Expression>>>;

using CallCollector = MapCollector<TagMapper<Tag<Call>>, VectorCollector<Reference<Expression>>>;

using MemberAccessCollector = MapCollector<TagMapper<Tag<MemberAccess>>, TupleCollector<std::string>>;

class BlockCollector {
	std::vector<Reference<Statement>> statements;
public:
	void push(Reference<Statement>&& statement) {
		statements.push_back(std::move(statement));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(Block(std::move(statements)));
	}
};

class StatementCollector {
	Reference<Statement> statement;
public:
	void push(Reference<Statement>&& statement) {
		this->statement = std::move(statement);
	}
	void push(Block&& block) {
		statement = new BlockStatement(std::move(block));
	}
	void push(Reference<Expression>&& expression) {
		statement = new ExpressionStatement(std::move(expression));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(statement));
	}
};

class EmptyStatementCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(new EmptyStatement());
	}
};

class LetStatementCollector {
	std::string name;
	Reference<Expression> type;
	Reference<Expression> expression;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(Tag<Type>, Reference<Expression>&& expression) {
		type = std::move(expression);
	}
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new LetStatement(std::move(name), std::move(type), std::move(expression)));
	}
};

class IfStatementCollector {
	Reference<Expression> condition;
	Reference<Statement> then_statement;
	Reference<Statement> else_statement;
public:
	struct ThenTag {};
	struct ElseTag {};
	void push(Reference<Expression>&& condition) {
		this->condition = std::move(condition);
	}
	void push(ThenTag, Reference<Statement>&& statement) {
		then_statement = std::move(statement);
	}
	void push(ElseTag, Reference<Statement>&& statement) {
		else_statement = std::move(statement);
	}
	template <class C> void retrieve(const C& callback) {
		if (else_statement == nullptr) {
			else_statement = new EmptyStatement();
		}
		callback.push(new IfStatement(std::move(condition), std::move(then_statement), std::move(else_statement)));
	}
};

class WhileStatementCollector {
	Reference<Expression> condition;
	Reference<Statement> statement;
public:
	void push(Reference<Expression>&& condition) {
		this->condition = std::move(condition);
	}
	void push(Reference<Statement>&& statement) {
		this->statement = std::move(statement);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new WhileStatement(std::move(condition), std::move(statement)));
	}
};

class ReturnStatementCollector {
	Reference<Expression> expression;
public:
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new ReturnStatement(std::move(expression)));
	}
};

struct TemplateArgument {};

class FunctionCollector {
	std::string name;
	std::vector<std::string> template_arguments;
	std::vector<Function::Argument> arguments;
	Reference<Expression> return_type;
	Block block;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(Tag<TemplateArgument>, std::string&& name) {
		template_arguments.push_back(std::move(name));
	}
	void push(std::string&& name, Reference<Expression>&& type) {
		arguments.emplace_back(std::move(name), std::move(type));
	}
	void push(Reference<Expression>&& expression) {
		return_type = std::move(expression);
	}
	void push(Block&& block) {
		this->block = std::move(block);
	}
	template <class C> void retrieve(const C& callback) {
		if (return_type == nullptr) {
			return_type = new Name("Void");
		}
		callback.push(new Function(std::move(name), std::move(template_arguments), std::move(arguments), std::move(return_type), std::move(block)));
	}
};

using ArgumentCollector = TupleCollector<std::string, Reference<Expression>>;

class StructureCollector {
	std::string name;
	std::vector<std::string> template_arguments;
	std::vector<Structure::Member> members;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(Tag<TemplateArgument>, std::string&& name) {
		template_arguments.push_back(std::move(name));
	}
	void push(std::string&& name, Reference<Expression>&& type) {
		members.emplace_back(std::move(name), std::move(type));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new Structure(std::move(name), std::move(template_arguments), std::move(members)));
	}
};

using MemberCollector = TupleCollector<std::string, Reference<Expression>>;

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

constexpr auto int_literal = choice(
	collect<NumberCollector>(sequence(ignore("0x"), one_or_more(hexadecimal_digit))),
	collect<NumberCollector>(one_or_more(decimal_digit))
);

constexpr auto bool_literal = choice(
	collect<BoolLiteralCollector<0>>(keyword("false")),
	collect<BoolLiteralCollector<1>>(keyword("true"))
);

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

using CharLiteralCollector = MapCollector<TagMapper<Tag<CharLiteral>>, StringCollector>;
using StringLiteralCollector = MapCollector<TagMapper<Tag<StringLiteral>>, StringCollector>;

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

constexpr auto struct_literal = collect<StructLiteralCollector>(sequence(
	keyword("new"),
	whitespace,
	type,
	whitespace,
	expect("{"),
	whitespace,
	comma_separated(collect<MemberCollector>(sequence(
		not_('}'),
		not_(end()),
		expect_identifier,
		whitespace,
		expect("="),
		whitespace,
		expression
	))),
	whitespace,
	expect("}")
));

// JavaScript-style struct literals
constexpr auto alternative_struct_literal = collect<StructLiteralCollector>(sequence(
	ignore('{'),
	whitespace,
	comma_separated(collect<MemberCollector>(sequence(
		not_('}'),
		not_(end()),
		expect_identifier,
		whitespace,
		expect(":"),
		whitespace,
		expression
	))),
	whitespace,
	expect("}")
));

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
			map<NewMapper<Name>>(identifier),
			error("expected a type")
		))
	)
);
DEFINE_PARSER(type, type_impl)

using AssignmentCollector = MapCollector<TagMapper<Tag<Assignment>>, TupleCollector<Reference<Expression>>>;

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
		postfix<MemberAccessCollector>(sequence(
			whitespace,
			ignore('.'),
			whitespace,
			identifier
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
			map<NewMapper<Name>>(identifier),
			error("expected an expression")
		))
	)
);
DEFINE_PARSER(expression, expression_impl)

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

constexpr auto statement_impl = collect<StatementCollector>(choice(
	block,
	collect<EmptyStatementCollector>(ignore(';')),
	collect<LetStatementCollector>(sequence(
		keyword("let"),
		whitespace,
		expect_identifier,
		whitespace,
		optional(sequence(
			ignore(':'),
			whitespace,
			tag<Tag<Type>>(type),
			whitespace
		)),
		expect("="),
		whitespace,
		expression,
		whitespace,
		expect(";")
	)),
	collect<IfStatementCollector>(sequence(
		keyword("if"),
		whitespace,
		expect("("),
		whitespace,
		expression,
		whitespace,
		expect(")"),
		whitespace,
		tag<IfStatementCollector::ThenTag>(statement),
		whitespace,
		optional(sequence(
			keyword("else"),
			whitespace,
			tag<IfStatementCollector::ElseTag>(statement)
		))
	)),
	collect<WhileStatementCollector>(sequence(
		keyword("while"),
		whitespace,
		expect("("),
		whitespace,
		expression,
		whitespace,
		expect(")"),
		whitespace,
		statement
	)),
	collect<ReturnStatementCollector>(sequence(
		keyword("return"),
		whitespace,
		optional(sequence(
			not_(';'),
			not_(end()),
			expression,
			whitespace
		)),
		expect(";")
	)),
	sequence(
		expression,
		whitespace,
		expect(";")
	)
));
DEFINE_PARSER(statement, statement_impl)

constexpr auto function = collect<FunctionCollector>(sequence(
	keyword("func"),
	whitespace,
	expect_identifier,
	whitespace,
	optional(sequence(
		ignore('<'),
		whitespace,
		comma_separated(sequence(
			not_('>'),
			not_(end()),
			tag<Tag<TemplateArgument>>(expect_identifier)
		)),
		whitespace,
		expect(">"),
		whitespace
	)),
	expect("("),
	whitespace,
	comma_separated(collect<ArgumentCollector>(sequence(
		not_(')'),
		not_(end()),
		expect_identifier,
		whitespace,
		expect(":"),
		whitespace,
		type
	))),
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

constexpr auto structure = collect<StructureCollector>(sequence(
	keyword("struct"),
	whitespace,
	expect_identifier,
	whitespace,
	optional(sequence(
		ignore('<'),
		whitespace,
		comma_separated(sequence(
			not_('>'),
			not_(end()),
			tag<Tag<TemplateArgument>>(expect_identifier)
		)),
		whitespace,
		expect(">"),
		whitespace
	)),
	expect("{"),
	whitespace,
	comma_separated(collect<MemberCollector>(sequence(
		not_('}'),
		not_(end()),
		expect_identifier,
		whitespace,
		expect(":"),
		whitespace,
		type
	))),
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
