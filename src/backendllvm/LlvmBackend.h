#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	/******************************************************************************
	** LlvmBackend
	******************************************************************************/

	/*
		The current design for this is as follows. The LlvmBackend holds the compiler, jit executor,
	and the state of modules LLVM is aware of.

		The LlvmModule is a semantics provider that holds all the IR for a module, and manages
	the backends awareness of the other semantics in the module.

		The LlvmSubroutine (and perhaps other objects) manage the state of individual parts of the
	state for each module. The LlvmSubroutine will manage the cache of type signatures -> jit'ed
	code, it may do this by copying IR out of the module, and into a jit handle.
	
	*/

	class LlvmBackend
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmBackend);
	public:
		llvm::LLVMContext context;

	private:
		friend class LlvmBackendProvider;

		friend class LlvmCompiler;

		friend class LlvmModule;
		friend class LlvmSubroutine;

		llvm::orc::ExecutionSession _es;
		std::map<llvm::orc::VModuleKey, std::shared_ptr<llvm::orc::SymbolResolver>> _resolvers;
		
		std::unique_ptr<llvm::TargetMachine> _tm;
		
		const llvm::DataLayout _dl;
		llvm::orc::RTDyldObjectLinkingLayer _objectLayer;
		llvm::orc::IRCompileLayer<decltype(_objectLayer), llvm::orc::SimpleCompiler> _compileLayer;

		using OptimizeFunction = std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)>;
		llvm::orc::IRTransformLayer<decltype(_compileLayer), OptimizeFunction> _optimizeLayer;
		std::unique_ptr<llvm::orc::JITCompileCallbackManager> _compileCallbackManager;
  		llvm::orc::CompileOnDemandLayer<decltype(_optimizeLayer)> _codLayer;

		instance<Environment> _env;
		instance<LlvmCompiler> _compiler;

		struct _ModuleEntry
		{
			instance<LlvmModule> semantics;

			size_t cb_semantics_onUpdate;
		};

		std::vector<_ModuleEntry> _modules;
		std::map<instance<Module>, size_t> _modules_byInstance;

		struct _InternalFunctionEntry
		{
			void* funcptr;

			llvm::FunctionType* fntype;
		};

		std::map<std::string, _InternalFunctionEntry> _internal_functions;

	private:		
		// llvm-internal: ___cult__runtime_subroutine_execute
		static CULTLANG_BACKENDLLVM_EXPORTED instance<> _cult_runtime_subroutine_execute(instance<> subroutine, instance<>* args, size_t argc);		
		// llvm-internal: ___cult__runtime_truth
		static CULTLANG_BACKENDLLVM_EXPORTED bool _cult_runtime_truth(instance<> v);

	public:
		using JitModule = llvm::orc::VModuleKey;

		CULTLANG_BACKENDLLVM_EXPORTED static std::string mangledName(instance<SBindable>, std::string const& postFix = "");

	public:

		CULTLANG_BACKENDLLVM_EXPORTED LlvmBackend(instance<Environment>);
		CULTLANG_BACKENDLLVM_EXPORTED void craft_setupInstance();

		CULTLANG_BACKENDLLVM_EXPORTED instance<LlvmCompiler> getCompiler() const;
		CULTLANG_BACKENDLLVM_EXPORTED instance<Environment> getEnvironment() const;

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmBackend::JitModule addModule(std::unique_ptr<llvm::Module> module);
		CULTLANG_BACKENDLLVM_EXPORTED std::unique_ptr<llvm::Module> optimizeModule(std::unique_ptr<llvm::Module> M);
		CULTLANG_BACKENDLLVM_EXPORTED void addJit(instance<LlvmSubroutine> subroutine);
		CULTLANG_BACKENDLLVM_EXPORTED void removeJit(instance<LlvmSubroutine> subroutine);

		CULTLANG_BACKENDLLVM_EXPORTED instance<> require(instance<SCultSemanticNode>);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::JITSymbol findSymbol(std::string const& name);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::JITTargetAddress getSymbolAddress(std::string const& name);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED void builtin_validateSpecialForms(instance<lisp::Module> module);
	};

	class LlvmBackendProvider final
		: public types::Implements<PBackend>::For<LlvmBackend>
		, public types::Implements<PCompiler>::For<LlvmBackend>
	{
	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmBackendProvider();

	public:
		virtual instance<> init(instance<Environment> env) const override;

	public:
		virtual instance<> makeCompilerOptions() const override;

		virtual void compile(instance<> backend, instance<> options, std::string const& path, instance<lisp::Module> module) const override;
	};

}}
