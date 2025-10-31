#include "parser.hpp"
#include "parsley/pratt.hpp"

using namespace parser;

class NameCollector {
	std::string name;
public:
	void push(char c) {
		name.push_back(c);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(name));
	}
};

class IntCollector {
	std::int32_t value = 0;
public:
	void push(char c) {
		value *= 10;
		value += c - '0';
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(value);
	}
};

class FalseCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(0);
	}
};

class TrueCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(1);
	}
};

class ExpressionCollector {
	Reference<Expression> expression;
public:
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	void push(std::int32_t value) {
		expression = new IntLiteral(value);
	}
	void push(std::string&& name) {
		expression = new Name(std::move(name));
	}
	void push(Reference<Expression>&& right, BinaryOperation operation) {
		expression = new BinaryExpression(operation, std::move(expression), std::move(right));
	}
	void push(Reference<Expression>&& right, Tag<Assignment>) {
		expression = new Assignment(std::move(expression), std::move(right));
	}
	void push(Tag<Call>, std::vector<Reference<Expression>>&& arguments) {
		expression = new Call(std::move(expression), std::move(arguments));
	}
	void set_location(const SourceLocation& location) {
		expression->set_location(location);
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

class CallCollector {
	std::vector<Reference<Expression>> arguments;
public:
	void push(Reference<Expression>&& expression) {
		arguments.push_back(std::move(expression));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(Tag<Call>(), std::move(arguments));
	}
};

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
	struct TypeTag {};
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(Reference<Expression>&& expression, TypeTag) {
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
	void push(Reference<Statement>&& statement, ThenTag) {
		then_statement = std::move(statement);
	}
	void push(Reference<Statement>&& statement, ElseTag) {
		else_statement = std::move(statement);
	}
	template <class C> void retrieve(const C& callback) {
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

class FunctionCollector {
	std::string name;
	std::vector<Function::Argument> arguments;
	Block block;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(std::string&& name, Reference<Expression>&& type) {
		arguments.emplace_back(std::move(name), std::move(type));
	}
	void push(Block&& block) {
		this->block = std::move(block);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(Function(std::move(name), std::move(arguments), std::move(block)));
	}
};

class ArgumentCollector {
	std::string name;
	Reference<Expression> type;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(Reference<Expression>&& expression) {
		type = std::move(expression);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(name), std::move(type));
	}
};

class StructureCollector {
	std::string name;
	std::vector<Structure::Member> members;
public:
	void push(std::string&& name) {
		this->name = std::move(name);
	}
	void push(std::string&& name, Reference<Expression>&& type) {
		members.emplace_back(std::move(name), std::move(type));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(Structure(std::move(name), std::move(members)));
	}
};

using MemberCollector = ArgumentCollector;

class ProgramCollector {
	std::vector<Function> functions;
	std::vector<Structure> structures;
public:
	void push(Function&& function) {
		functions.push_back(std::move(function));
	}
	void push(Structure&& structure) {
		structures.push_back(std::move(structure));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(Program(std::move(functions), std::move(structures)));
	}
};

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

template <class P> constexpr auto comma_separated(P p) {
	return optional(sequence(p, whitespace, zero_or_more(sequence(ignore(','), whitespace, p, whitespace))));
}

constexpr auto identifier = collect<NameCollector>(sequence(alphabetic_char, zero_or_more(alphanumeric_char)));

constexpr auto int_literal = collect<IntCollector>(one_or_more(numeric_char));

struct type {
	static constexpr auto parser = pratt<ExpressionCollector>(
		pratt_level(
			terminal(choice(
				identifier,
				error("expected a type")
			))
		)
	);
};

struct expression {
	static constexpr auto parser = pratt<ExpressionCollector>(
		pratt_level(
			infix_rtl<TagMapper<Tag<Assignment>>>(operator_(sequence('=', not_('='))))
		),
		pratt_level(
			infix_ltr<OperationMapper<BinaryOperation::EQ>>(operator_("==")),
			infix_ltr<OperationMapper<BinaryOperation::NE>>(operator_("!="))
		),
		pratt_level(
			infix_ltr<OperationMapper<BinaryOperation::LT>>(operator_(sequence('<', not_('=')))),
			infix_ltr<OperationMapper<BinaryOperation::LE>>(operator_("<=")),
			infix_ltr<OperationMapper<BinaryOperation::GT>>(operator_(sequence('>', not_('=')))),
			infix_ltr<OperationMapper<BinaryOperation::GE>>(operator_(">="))
		),
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
			postfix<IgnoreCallback>(collect<CallCollector>(sequence(
				ignore('('),
				whitespace,
				comma_separated(sequence(
					not_(')'),
					not_(end()),
					reference<expression>()
				)),
				whitespace,
				expect(")")
			)))
		),
		pratt_level(
			terminal(choice(
				sequence(ignore('('), whitespace, reference<expression>(), whitespace, expect(")")),
				collect<FalseCollector>(keyword("false")),
				collect<TrueCollector>(keyword("true")),
				int_literal,
				identifier,
				error("expected an expression")
			))
		)
	);
};

struct statement;

struct block {
	static constexpr auto parser = collect<BlockCollector>(sequence(
		ignore('{'),
		whitespace,
		zero_or_more(sequence(
			not_('}'),
			not_(end()),
			reference<statement>(),
			whitespace
		)),
		expect("}")
	));
};

struct statement {
	static constexpr auto parser = collect<StatementCollector>(choice(
		reference<block>(),
		collect<EmptyStatementCollector>(ignore(';')),
		collect<LetStatementCollector>(sequence(
			keyword("let"),
			whitespace,
			choice(
				identifier,
				error("expected an identifier")
			),
			whitespace,
			optional(sequence(
				ignore(':'),
				whitespace,
				tag<LetStatementCollector::TypeTag>(reference<type>()),
				whitespace
			)),
			expect("="),
			whitespace,
			reference<expression>(),
			whitespace,
			expect(";")
		)),
		collect<IfStatementCollector>(sequence(
			keyword("if"),
			whitespace,
			expect("("),
			whitespace,
			reference<expression>(),
			whitespace,
			expect(")"),
			whitespace,
			tag<IfStatementCollector::ThenTag>(reference<statement>()),
			whitespace,
			optional(sequence(
				keyword("else"),
				whitespace,
				tag<IfStatementCollector::ElseTag>(reference<statement>())
			))
		)),
		collect<WhileStatementCollector>(sequence(
			keyword("while"),
			whitespace,
			expect("("),
			whitespace,
			reference<expression>(),
			whitespace,
			expect(")"),
			whitespace,
			reference<statement>()
		)),
		sequence(
			reference<expression>(),
			whitespace,
			expect(";")
		)
	));
};

constexpr auto function = collect<FunctionCollector>(sequence(
	keyword("func"),
	whitespace,
	choice(
		identifier,
		error("expected an identifier")
	),
	whitespace,
	expect("("),
	whitespace,
	comma_separated(collect<ArgumentCollector>(sequence(
		not_(')'),
		not_(end()),
		choice(
			identifier,
			error("expected an identifier")
		),
		whitespace,
		optional(sequence(
			ignore(':'),
			whitespace,
			reference<type>(),
			whitespace
		))
	))),
	whitespace,
	expect(")"),
	whitespace,
	choice(
		reference<block>(),
		error("expected a block")
	)
));

constexpr auto structure = collect<StructureCollector>(sequence(
	keyword("struct"),
	whitespace,
	identifier,
	whitespace,
	expect("{"),
	whitespace,
	comma_separated(collect<MemberCollector>(sequence(
		not_('}'),
		not_(end()),
		choice(
			identifier,
			error("expected an identifier")
		),
		whitespace,
		optional(sequence(
			ignore(':'),
			whitespace,
			reference<type>(),
			whitespace
		))
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

void parse_program(const char* path, Program& program_, Errors& errors) {
	auto source = read_file(path);
	Context context(source);
	const Result result = parse_impl(program, context, GetValueCallback<Program>(program_));
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
	program_.set_path(path);
}
