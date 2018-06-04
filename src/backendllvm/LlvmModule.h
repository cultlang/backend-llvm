#pragma once
#include "backendllvm/common.h"

#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	/******************************************************************************
	** LlvmModule
	******************************************************************************/

	class LlvmModule
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmModule);
	private:
		friend class LlvmSemanticsProvider;

		instance<LlvmBackend> _backend;
		instance<lisp::Module> _module;

		std::unique_ptr<llvm::Module> _ir;

		struct _Entry
		{
			instance<SCultSemanticNode> _cult;
			instance<> _llvm;
		};

		SymbolTableIndexed<_Entry*> _entries;

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmModule(instance<LlvmBackend>, instance<lisp::Module>);

	public:

		CULTLANG_BACKENDLLVM_EXPORTED instance<LlvmBackend> getBackend() const;
		CULTLANG_BACKENDLLVM_EXPORTED instance<lisp::Module> getModule() const;
		CULTLANG_BACKENDLLVM_EXPORTED std::unique_ptr<llvm::Module>& getIr();

		CULTLANG_BACKENDLLVM_EXPORTED void generate();

		CULTLANG_BACKENDLLVM_EXPORTED instance<> require(instance<SCultSemanticNode>);
	};


	/******************************************************************************
	** PSemantics for LlvmModule
	******************************************************************************/

	class LlvmSemanticsProvider
		: public craft::types::Implements<PSemantics>::For<LlvmModule>
	{
		virtual instance<lisp::Module> getModule(instance<> semantics) const override;

		virtual instance<> read(instance<lisp::Module> module, ReadOptions const* opts) const override;

		virtual instance<> lookup(instance<> semantics, instance<Symbol>) const override;
	};
}}
