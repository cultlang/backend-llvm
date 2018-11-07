#pragma warning( push )
#pragma warning( disable : 4244)
#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmBackend.h"

using namespace craft;
using namespace craft::types;

using namespace llvm;
using namespace llvm::orc;



#ifdef _WIN32
#define CLANG_INTERPRETER_WIN_EXCEPTIONS
#include "WindowsMemoryManager.h"
typedef interpreter::SingleSectionMemoryManager MemManager;
#else
typedef llvm::SectionMemoryManager MemManager;
#endif

CRAFT_DEFINE(craft::lisp::LlvmBackend)
{
	_.use<PBackend>().singleton<craft::lisp::LlvmBackendProvider>();

	_.defaults();
}

instance<> craft::lisp::LlvmBackend::_cult_runtime_subroutine_execute(instance<> subroutine, instance<>* args, size_t argc)
{
	// Because this method decrefs one more than it does incref for the subroutine:
	subroutine.incref();

	if (!subroutine.hasFeature<craft::lisp::PSubroutine>())
	{
		throw stdext::exception("{0} does not implement subroutine.");
	}

	auto sub = subroutine.asFeature<PSubroutine>();

	return sub->execute(subroutine, {args, argc});
}

bool craft::lisp::LlvmBackend::_cult_runtime_truth(instance<> v)
{
	// Because this method decrefs one more than it does incref for the subroutine:
	v.incref();

	if (v.isType<bool>())
		return *v.asType<bool>();
	return v;
}

std::string craft::lisp::LlvmBackend::mangledName(instance<SBindable> bindable, std::string const& postFix)
{
	auto binding = bindable->getBinding();
	auto module = binding->getScope()->getSemantics()->getModule();

	// TODO ensure more than 2 @ is an error for any real symbol
	// TODO add type specialization here?
	if (postFix.empty())
		return fmt::format("{1}@@@{0}", module->uri(), binding->getSymbol()->getDisplay());
	else
		return fmt::format("{1}@@@{0}:::{2}", module->uri(), binding->getSymbol()->getDisplay(), postFix);
}

craft::lisp::LlvmBackend::LlvmBackend(instance<craft::lisp::Environment> env)
	: context()
	, _es()
	, _tm(EngineBuilder()
		.setTargetOptions([]() -> TargetOptions {
			TargetOptions ret;
			ret.ExceptionModel = ExceptionHandling::WinEH;
			return ret;
		}())
		.selectTarget()) // from current process
	, _dl(_tm->createDataLayout())
	, _objectLayer(_es,
		[this](VModuleKey k) {
			return RTDyldObjectLinkingLayer::Resources{
				std::make_shared<MemManager>(), _resolvers[k]
			};
	})
	, _compileLayer(_objectLayer, SimpleCompiler(*_tm))
	, _optimizeLayer(_compileLayer, [this](std::unique_ptr<::llvm::Module> M) {return optimizeModule(std::move(M));})
	, _compileCallbackManager(cantFail(orc::createLocalCompileCallbackManager(_tm->getTargetTriple(), _es, 0)))
	, _codLayer(_es, _optimizeLayer,
		[&](orc::VModuleKey K) { return _resolvers[K]; },
		[&](orc::VModuleKey K, std::shared_ptr<SymbolResolver> R) {_resolvers[K] = std::move(R);},
		[](llvm::Function &F) { return std::set<llvm::Function*>({&F}); },
		*_compileCallbackManager,
		orc::createLocalIndirectStubsManagerBuilder(_tm->getTargetTriple())
	)
	, _env(env)
{
	env->log()->info("LLVM Target: {0}", (std::string)_tm->getTargetTriple().str());
	env->log()->info("LLVM Target Features: {0}", (std::string)_tm->getTargetFeatureString());

	_objectLayer.setProcessAllSections(true);

	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr); // load the current process
}

void craft::lisp::LlvmBackend::craft_setupInstance()
{
	_compiler = instance<craft::lisp::LlvmCompiler>::make(craft_instance());
}

instance<craft::lisp::LlvmCompiler> craft::lisp::LlvmBackend::getCompiler() const
{
	return _compiler;
}

instance<craft::lisp::Environment> craft::lisp::LlvmBackend::getEnvironment() const
{
	return _env;
}

craft::lisp::LlvmBackend::JitModule craft::lisp::LlvmBackend::addModule(std::unique_ptr<llvm::Module> module)
{
	auto K = _es.allocateVModule();
	auto Filename = "output.o";
	std::error_code EC;
	raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

	if (EC) {
		errs() << "Could not open file: " << EC.message();
		return K;
	}

	legacy::PassManager pass;
	auto FileType = TargetMachine::CGFT_ObjectFile;

	if (_tm->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
		errs() << "TargetMachine can't emit a file of this type";
		return K;
	}

	pass.run(*module);
	dest.flush();

	_resolvers[K] = createLegacyLookupResolver(
		_es,
		[this](const std::string &Name) -> JITSymbol {
			auto internal_it = _internal_functions.find(Name);
			if (internal_it != _internal_functions.end())
				return JITSymbol((uintptr_t)internal_it->second.funcptr, JITSymbolFlags::Exported);
			else if (auto Sym = _compileLayer.findSymbol(Name, false))
				return Sym;
			else if (auto Err = Sym.takeError())
				return std::move(Err);
			if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
				return JITSymbol(SymAddr, JITSymbolFlags::Exported);
			return nullptr;
		},
		[](Error Err) { 
			cantFail(std::move(Err), "lookupFlags failed"); 
		}
	);

    cantFail(_codLayer.addModule(K, std::move(module)));
    return K;
}

std::unique_ptr<llvm::Module> craft::lisp::LlvmBackend::optimizeModule(std::unique_ptr<llvm::Module> M)
{
	auto FPM = llvm::make_unique<legacy::FunctionPassManager>(M.get());

    // Add some optimizations.
    FPM->add(createInstructionCombiningPass());
    FPM->add(createReassociatePass());
    FPM->add(createGVNPass());
    FPM->add(createCFGSimplificationPass());
    FPM->doInitialization();

    // Run the optimizations over all functions in the module being added to
    // the JIT.
    for (auto &F : *M)
      FPM->run(F);

    return M;
}

void craft::lisp::LlvmBackend::addJit(instance<craft::lisp::LlvmSubroutine> subroutine)
{
	subroutine->generate();
}
void craft::lisp::LlvmBackend::removeJit(instance<craft::lisp::LlvmSubroutine> subroutine)
{
	cantFail(_compileLayer.removeModule(subroutine->_jit_handle_generic));
}

instance<> craft::lisp::LlvmBackend::require(instance<craft::lisp::SCultSemanticNode> node)
{
	auto module = craft::lisp::SScope::findScope(node)->getSemantics()->getModule();
	auto llvm = module->require<LlvmModule>();

	llvm->generate();
	return llvm->require(node);
}

JITSymbol craft::lisp::LlvmBackend::findSymbol(std::string const& name)
{
	std::string mangled_name;
	raw_string_ostream mangled_name_stream(mangled_name);
	Mangler::getNameWithPrefix(mangled_name_stream, name, _dl);
	return _codLayer.findSymbol(mangled_name_stream.str(), false);
}

JITTargetAddress craft::lisp::LlvmBackend::getSymbolAddress(std::string const& name)
{
	auto s = findSymbol(name);
	return cantFail(s.getAddress());
}

craft::lisp::LlvmBackendProvider::LlvmBackendProvider()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
}

instance<> craft::lisp::LlvmBackendProvider::init(instance<craft::lisp::Environment> ns) const
{
	return instance<craft::lisp::LlvmBackend>::make(ns);
}

instance<> craft::lisp::LlvmBackendProvider::makeCompilerOptions() const
{
	return instance<>();
}

void craft::lisp::LlvmBackendProvider::compile(instance<> backend, instance<> options, std::string const& path, instance<lisp::Module> module) const
{

}

/*
instance<> LlvmBackendProvider::addModule(instance<> backend_ns, instance<lisp::Module> lisp_module) const
{
	instance<LlvmBackend> ns = backend_ns;

	if (lisp_module->uri() == "")
	{
		ns->aquireBuiltins(lisp_module);
	}

	instance<LlvmModule> module = instance<LlvmModule>::make(ns, lisp_module);

	ns->addModule(module);
	return module;
}

instance<> LlvmBackendProvider::addFunction(instance<> backend_module, instance<> lisp_subroutine) const
{
	instance<LlvmModule> module = backend_module;
	instance<LlvmSubroutine> subroutine = instance<LlvmSubroutine>::make(module, lisp_subroutine);

	module->addSubroutine(subroutine);
	return subroutine;
}

instance<> LlvmBackendProvider::exec(instance<lisp::SFrame> frame, instance<> code) const
{
	instance<Function> function;
	if (code.typeId().isType<Sexpr>())
		code = code.asType<Sexpr>()->car();
	if (code.typeId().isType<Function>())
		function = code;

	if (function)
	{
		instance<LlvmBackend> backend = function->backend.asType<LlvmSubroutine>()->_module->_backend;


	}
	else
		return frame->getEnvironment()->environment()->eval(frame, code);
}
*/

#pragma warning( pop ) 