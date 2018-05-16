#include "backendllvm/common.h"

/*
#include "lisp/backend/llvm/llvm_internal.h"
#include "lisp/backend/llvm/LlvmBackend.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_LISP_EXPORTED instance<> __trampoline_interpreter(LlvmSubroutine* subroutine, instance<>* arry, size_t count)
{
	auto sexpr = instance<Sexpr>::make();
	sexpr->cells.reserve(count + 1);
	sexpr->cells.push_back(instance<LlvmSubroutine>(subroutine));
	std::copy(arry, arry + count, std::back_inserter(sexpr->cells));

	auto frame = Execution::getCurrent();
	auto ns = frame->getNamespace();
	return ns->get<BootstrapInterpreter>()->exec();
}

CRAFT_DEFINE(LlvmBackend)
{
	_.use<PBackend>().singleton<LlvmBackendProvider>();

	_.defaults();
}

LlvmBackend::LlvmBackend(instance<Namespace> lisp)
	: _tm(EngineBuilder().selectTarget()) // from current process
	, _dl(_tm->createDataLayout())
	, _objectLayer([]() { return std::make_shared<SectionMemoryManager>(); }) // lambda to make memory sections
	, _compileLayer(_objectLayer, SimpleCompiler(*_tm))
	, lisp(lisp)
	, compiler(instance<LlvmCompiler>::make())
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

void LlvmBackend::aquireBuiltins(instance<lisp::Module> builtins)
{

}

void LlvmBackend::addModule(instance<LlvmModule> module)
{
	module->generate();

	// Add the set to the JIT with the resolver we created above and a newly created SectionMemoryManager.
	//module->handle = cantFail(_compileLayer.addModule(std::move(module->ir), _resolver));
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

void LlvmBackend::removeModule(ModuleHandle H)
{
	cantFail(_compileLayer.removeModule(H));
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