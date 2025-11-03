#pragma once

#include "ast.hpp"

void parse_program(const char* path, Reference<Program>& program, Errors& errors);
