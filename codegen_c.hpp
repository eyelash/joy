#pragma once

#include "ast.hpp"
#include <iostream>

void codegen_c(std::ostream& ostream, const Program& program);
