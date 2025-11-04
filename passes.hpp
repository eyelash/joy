#pragma once

#include "ast.hpp"

void type_checking(Program* program, Errors& errors);
Reference<Program> pass1(const Program* program, Errors& errors);
