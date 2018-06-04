#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmBackend.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CULTLANG_BACKENDLLVM_EXPORTED instance<> __trampoline_interpreter(LlvmSubroutine* subroutine, instance<>* arry, size_t count)
{
	GenericInvoke invoke(count);
	std::copy(arry, arry + count, std::back_inserter(invoke.args));

	/*
	auto frame = Execution::getCurrent();
	auto ns = frame->getNamespace();
	return ns->get<BootstrapInterpreter>()->exec();
	*/

	return instance<>();
}

CRAFT_DEFINE(LlvmBackend)
{
	_.use<PBackend>().singleton<LlvmBackendProvider>();

	_.defaults();
}

std::string LlvmBackend::mangledName(instance<SBindable> bindable)
{
	auto binding = bindable->getBinding();
	auto module = binding->getScope()->getSemantics()->getModule();

	// TODO ensure more than 2 @ is an error for any real symbol
	// TODO add type specialization here?
	return fmt::format("{1}@@@{0}", module->uri(), binding->getSymbol()->getDisplay());
}

LlvmBackend::LlvmBackend(instance<Namespace> lisp)
	: _tm(EngineBuilder().selectTarget()) // from current process
	, _dl(_tm->createDataLayout())
	, _objectLayer([]() { return std::make_shared<SectionMemoryManager>(); }) // lambda to make memory sections
	, _compileLayer(_objectLayer, SimpleCompiler(*_tm))
	, _ns(lisp)
{
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr); // load the current process

	// Build our symbol resolver:
	// Lambda 1: Look back into the JIT itself to find symbols that are part of the same "logical dylib".
	// Lambda 2: Search for external symbols in the host process.
	_resolver = createLambdaResolver(
		[&](const std::string &Name) {
		if (auto Sym = _compileLayer.findSymbol(Name, false))
			return Sym;
		return JITSymbol(nullptr);
	},
		[](const std::string &Name) {
		if (auto SymAddr =
			RTDyldMemoryManager::getSymbolAddressInProcess(Name))
			return JITSymbol(SymAddr, JITSymbolFlags::Exported);
		return JITSymbol(nullptr);
	});
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

void LlvmBackend::addModule(instance<LlvmModule> module)
{

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
	return _compileLayer.findSymbol(mangled_name_stream.str(), true);
}

JITTargetAddress LlvmBackend::getSymbolAddress(std::string const& name)
{
	return cantFail(findSymbol(name).getAddress());
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