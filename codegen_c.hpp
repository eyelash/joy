#pragma once

#include "ast.hpp"
#include <iostream>

void codegen_c(BufferedOutput& output, const Program* program);
