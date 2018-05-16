#include "backendllvm/common.h"

#include "all.h"

#include "lisp/library/system/prelude.h"
#include "lisp/semantics/cult/calling.h"



using namespace craft;
using namespace craft::lisp;
using namespace craft::types;
using namespace cultlang::backendllvm;



#define lMM semantics->builtin_implementMultiMethod
#define BiB "builtinbase"

#include "typedefs.h"


instance<Module> cultlang::backendllvm::make_llvm_bindings(instance<lisp::Namespace> ns, instance<> loader)
{
	auto ret = instance<Module>::make(ns, loader);
	auto sem = instance<CultSemantics>::make(ret);
	ret->builtin_setSemantics(sem);

	auto semantics = ret->require<CultSemantics>();

	//lMM(BiB"", []() { return instance<Base>::make();});
	
	return ret;
}

BuiltinModuleDescription cultlang::backendllvm::BuiltinLLVM("cult/llvm", cultlang::backendllvm::make_llvm_bindings);


#include "types/dll_entry.inc"
