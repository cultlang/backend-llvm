#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmBackend.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmBackend)
{
	_.use<PBackend>().singleton<LlvmBackendProvider>();

	_.defaults();
}

instance<> LlvmBackend::_cult_runtime_subroutine_execute(instance<> subroutine, instance<>* args, size_t argc)
{
	// Because this method decrefs one more than it does incref for the subroutine:
	subroutine.incref();

	if (!subroutine.hasFeature<PSubroutine>())
	{
		throw stdext::exception("{0} does not implement subroutine.");
	}

	auto sub = subroutine.asFeature<PSubroutine>();

	return sub->execute(subroutine, {args, argc});
}

bool LlvmBackend::_cult_runtime_truth(instance<> v)
{
	// Because this method decrefs one more than it does incref for the subroutine:
	v.incref();

	if (v.isType<bool>())
		return *v.asType<bool>();
	return v;
}

std::string LlvmBackend::mangledName(instance<SBindable> bindable, std::string const& postFix)
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

LlvmBackend::LlvmBackend(instance<Namespace> lisp)
	: context()
	, _es()
	, _resolver(createLegacyLookupResolver(
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
		})
	)
	, _tm(EngineBuilder().selectTarget()) // from current process
	, _dl(_tm->createDataLayout())
	, _objectLayer(_es,
		[this](VModuleKey) {
			return RTDyldObjectLinkingLayer::Resources{
				std::make_shared<SectionMemoryManager>(), _resolver
			};
	})
	, _compileLayer(_objectLayer, SimpleCompiler(*_tm))
	, _ns(lisp)
{
	lisp->getEnvironment()->log()->info("LLVM Target: {0}", (std::string)_tm->getTargetTriple().str());
	lisp->getEnvironment()->log()->info("LLVM Target Features: {0}", (std::string)_tm->getTargetFeatureString());

	_objectLayer.setProcessAllSections(true);

	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr); // load the current process
}

void LlvmBackend::craft_setupInstance()
{
	_compiler = instance<LlvmCompiler>::make(craft_instance());
}

instance<LlvmCompiler> LlvmBackend::getCompiler() const
{
	return _compiler;
}

instance<Namespace> LlvmBackend::getNamespace() const
{
	return _ns;
}

LlvmBackend::JitModule LlvmBackend::addModule(std::unique_ptr<llvm::Module> module)
{
	auto K = _es.allocateVModule();
    cantFail(_compileLayer.addModule(K, std::move(module)));
	
    return K;
}

void LlvmBackend::addJit(instance<LlvmSubroutine> subroutine)
{
	subroutine->generate();
}
void LlvmBackend::removeJit(instance<LlvmSubroutine> subroutine)
{
	cantFail(_compileLayer.removeModule(subroutine->_jit_handle_generic));
}

instance<> LlvmBackend::require(instance<SCultSemanticNode> node)
{
	auto module = SScope::findScope(node)->getSemantics()->getModule();
	auto llvm = module->require<LlvmModule>();

	llvm->generate();
	return llvm->require(node);
}

JITSymbol LlvmBackend::findSymbol(std::string const& name)
{
	std::string mangled_name;
	raw_string_ostream mangled_name_stream(mangled_name);
	Mangler::getNameWithPrefix(mangled_name_stream, name, _dl);
	return _compileLayer.findSymbol(mangled_name_stream.str(), false);
}

JITTargetAddress LlvmBackend::getSymbolAddress(std::string const& name)
{
	auto s = findSymbol(name);
	return cantFail(s.getAddress());
}

LlvmBackendProvider::LlvmBackendProvider()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
}

instance<> LlvmBackendProvider::init(instance<Namespace> ns) const
{
	return instance<LlvmBackend>::make(ns);
}

instance<> LlvmBackendProvider::makeCompilerOptions() const
{
	return instance<>();
}

void LlvmBackendProvider::compile(instance<> backend, instance<> options, std::string const& path, instance<lisp::Module> module) const
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
		return frame->getNamespace()->environment()->eval(frame, code);
}
*/