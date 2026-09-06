#pragma once

#include "ast.hpp"

void desugaring(Program* program);

void pass1(Program* program, Diagnostics& diagnostics);
