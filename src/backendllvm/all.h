#pragma once
#include "backendllvm/common.h"

namespace cultlang {
namespace backendllvm
{
	extern craft::lisp::BuiltinModuleDescription BuiltinLLVM;

	CULTLANG_BACKENDLLVM_EXPORTED craft::instance<craft::lisp::Module> make_llvm_bindings(craft::instance<craft::lisp::Namespace> ns, craft::instance<> loader);
}}
