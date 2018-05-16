#pragma once
#include "backendllvm/common.h"

#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmModule
		: public virtual craft::types::Object
	{
		CRAFT_LISP_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmModule);
	public:
		instance<LlvmBackend> _backend;

		instance<Module> _lisp;

		std::unique_ptr<llvm::Module> ir;
		LlvmBackend::ModuleHandle handle;

	public:

		CRAFT_LISP_EXPORTED LlvmModule(instance<LlvmBackend>, instance<lisp::Module>);

	public:

		CRAFT_LISP_EXPORTED void generate();

		CRAFT_LISP_EXPORTED void addSubroutine(instance<LlvmSubroutine> subroutine);
	};
}}
