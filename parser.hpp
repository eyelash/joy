#pragma once

#include "ast.hpp"
#include "parsley/parser.hpp"

parser::Result parse_program(parser::Context& context, Reference<Expression>& expression);
