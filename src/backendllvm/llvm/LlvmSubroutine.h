#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmSubroutine
		: public virtual craft::types::Object
	{
		CRAFT_LISP_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmSubroutine);
	public:
		instance<LlvmModule> _module;

		std::string _binding_hint;
		instance<> _lisp;

		llvm::Function* func;

	public:

		CRAFT_LISP_EXPORTED LlvmSubroutine(instance<LlvmModule>, instance<>);

	public:

		CRAFT_LISP_EXPORTED void generate();

	};
}}
