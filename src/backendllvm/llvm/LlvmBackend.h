#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmBackend
		: public virtual craft::types::Object
	{
		CRAFT_LISP_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmBackend);
	public:
		typedef void(*f_specialFormHandler)();

	private:
		friend class LlvmBackendProvider;

		friend class LlvmModule;
		friend class LlvmSubroutine;

		std::unique_ptr<llvm::TargetMachine> _tm;
		const llvm::DataLayout _dl;
		llvm::orc::RTDyldObjectLinkingLayer _objectLayer;
		llvm::orc::IRCompileLayer<decltype(_objectLayer), llvm::orc::SimpleCompiler> _compileLayer;

		std::shared_ptr<llvm::JITSymbolResolver> _resolver;

	public:
		instance<Namespace> lisp;

		instance<LlvmCompiler> compiler;

	public:
		using ModuleHandle = decltype(_compileLayer)::ModuleHandleT;

	public:

		CRAFT_LISP_EXPORTED LlvmBackend(instance<Namespace>);

	public:

		CRAFT_LISP_EXPORTED void addModule(instance<LlvmModule> module);

		CRAFT_LISP_EXPORTED llvm::JITSymbol findSymbol(std::string const& name);
		CRAFT_LISP_EXPORTED llvm::JITTargetAddress getSymbolAddress(std::string const& name);

		CRAFT_LISP_EXPORTED void removeModule(ModuleHandle module);

	public:
		CRAFT_LISP_EXPORTED void builtin_addSpecialFormHandler(std::string const&, f_specialFormHandler handler);

		CRAFT_LISP_EXPORTED void builtin_validateSpecialForms(instance<lisp::Module> module);
	};

	class LlvmBackendProvider final
		: public types::Implements<PBackend>::For<LlvmBackend>
	{
	public:
		CRAFT_LISP_EXPORTED LlvmBackendProvider();

	public:
		virtual instance<> init(instance<Namespace> env) const override;
	};

}}
