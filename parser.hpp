#pragma once

#include "ast.hpp"

Reference<Program> parse_program(const char* path, Diagnostics& diagnostics);
