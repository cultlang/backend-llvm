#pragma once
#include "backendllvm/common.h"

namespace cultlang {
namespace backendllvm
{
	extern craft::lisp::BuiltinModuleDescription BuiltinLlvm;

	CULTLANG_BACKENDLLVM_EXPORTED void make_llvm_bindings(craft::instance<craft::lisp::Module> mod);
}}
