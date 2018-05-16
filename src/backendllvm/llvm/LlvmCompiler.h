#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmCompiler
		: public virtual craft::types::Object
	{
		CRAFT_LISP_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmCompiler);

	public:
		llvm::LLVMContext context;

		struct CompilerState
		{

		};

		llvm::Type* type_anyPtr;
		llvm::Type* type_instance;

	protected:
		std::map<instance<lisp::SpecialForm>, std::function<void (CompilerState&, instance<>)>> _compilerVisitor_specialForms;

		static CRAFT_LISP_EXPORTED void compile_do(CompilerState&, instance<>);
		static CRAFT_LISP_EXPORTED void compile_cond(CompilerState&, instance<>);
		static CRAFT_LISP_EXPORTED void compile_while(CompilerState&, instance<>);

	public:
		CRAFT_LISP_EXPORTED llvm::FunctionType* getLlvmType(types::ExpressionStore signature);

	public:
		CRAFT_LISP_EXPORTED LlvmCompiler();

	public:

	};
}}
