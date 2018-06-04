#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmSubroutine
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmSubroutine);
	private:
		friend class LlvmBackend;

		instance<LlvmModule> _module;

		// TODO this may be responsible for many functions / multimethods
		instance<SCultSemanticNode> _ast;

		// TODO ad specialization code
		LlvmBackend::JitModule _jit_handle_generic;
		//std::map<types::ExpressionStore, LlvmBackend::JitModule> _jit_handle_specialized;

	public:

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmSubroutine(instance<LlvmModule>, instance<SCultSemanticNode>);

	public:
		// TODO take type signature:
		CULTLANG_BACKENDLLVM_EXPORTED void generate();
		CULTLANG_BACKENDLLVM_EXPORTED LlvmBackend::JitModule specialize(std::vector<types::TypeId>* = nullptr);

		CULTLANG_BACKENDLLVM_EXPORTED std::string stringifyPrototypeIr();
	};
}}
