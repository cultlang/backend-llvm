#pragma once

#include "backendllvm/common.h"

//
// LLVM
//

#ifdef _MSC_VER
#pragma warning( push, 1 )
#pragma warning( disable : 4141 )
#pragma warning( disable : 4291 )
#pragma warning( disable : 4624 )
#endif

// Depreciated in C++ 17, llvm uses them
namespace std
{
	template <class Arg, class Result>
	struct unary_function {
		typedef Arg argument_type;
		typedef Result result_type;
	};

	template <class Arg1, class Arg2, class Result>
		struct binary_function {
		typedef Arg1 first_argument_type;
		typedef Arg2 second_argument_type;
		typedef Result result_type;
	};

	template <class Arg, class Result>
	class pointer_to_unary_function : public unary_function <Arg, Result>
	{
	protected:
		Result(*pfunc)(Arg);
	public:
		explicit pointer_to_unary_function(Result(*f)(Arg)) : pfunc(f) {}
		Result operator() (Arg x) const
		{
			return pfunc(x);
		}
	};

	template <class Arg1, class Arg2, class Result>
	class pointer_to_binary_function : public binary_function <Arg1, Arg2, Result>
	{
	protected:
		Result(*pfunc)(Arg1, Arg2);
	public:
		explicit pointer_to_binary_function(Result(*f)(Arg1, Arg2)) : pfunc(f) {}
		Result operator() (Arg1 x, Arg2 y) const
		{
			return pfunc(x, y);
		}
	};

	template <class Arg, class Result>
	pointer_to_unary_function<Arg, Result> ptr_fun(Result(*f)(Arg))
	{
		return pointer_to_unary_function<Arg, Result>(f);
	}

	template <class Arg1, class Arg2, class Result>
	pointer_to_binary_function<Arg1, Arg2, Result> ptr_fun(Result(*f)(Arg1, Arg2))
	{
		return pointer_to_binary_function<Arg1, Arg2, Result>(f);
	}
}

#include "llvm/Support/TargetSelect.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Legacy.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif

//
// Ours
//

#include "lisp/backend/backend.h"
#include "lisp/backend/BootstrapInterpreter.h"

#include "lisp/semantics/cult/cult.h"

//
// Lisp Object
//

namespace craft {
namespace lisp
{
	class LlvmBackend;
	class LlvmModule;
	class LlvmSubroutine;
}}

#include "LlvmCompiler.h"
#include "LlvmBackend.h"
#include "LlvmModule.h"
#include "LlvmSubroutine.h"
