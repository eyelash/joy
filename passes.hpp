#pragma once

#include "ast.hpp"

void type_checking(Program* program, Errors& errors);
void pass1(Program* program, Errors& errors);
