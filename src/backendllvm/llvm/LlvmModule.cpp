#include "backendllvm/common.h"

/*
#include "lisp/backend/llvm/llvm_internal.h"
#include "lisp/backend/llvm/LlvmModule.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmModule)
{
	_.defaults();
}

LlvmModule::LlvmModule(instance<LlvmBackend> backend, instance<lisp::Module> lisp)
{
	_backend = backend;
	_lisp = lisp;
}

void LlvmModule::generate()
{
	auto s = _lisp->uri();
	ir = std::make_unique<llvm::Module>(StringRef(s), _backend->compiler->context);
	ir->setDataLayout(_backend->_dl);

	std::vector<Type*> functionTypeArgs_trampoline = { _backend->compiler->type_anyPtr, llvm::PointerType::get(_backend->compiler->type_instance, 0), llvm::Type::getInt64Ty(_backend->compiler->context) };
	auto functionType_trampoline = llvm::FunctionType::get(_backend->compiler->type_instance, functionTypeArgs_trampoline, false);
	auto _function_trampoline = llvm::Function::Create(functionType_trampoline, llvm::Function::ExternalLinkage, "__trampoline_interpreter", ir.get());

	for (auto b : _lisp->bindings())
	{
		if (!b.typeId().isType<Binding>())
			continue;
		instance<Binding> binding(b);
		
		auto v = binding->getValue(instance<>());
		if (v.typeId().isType<Function>())
		{
			instance<Function> f = v;
			f->backend = _backend.asFeature<PBackend>()->addFunction(craft_instance(), f);
			auto subroutine = f->backend.asType<LlvmSubroutine>();
			subroutine->_binding_hint = b->name();
			subroutine->generate();
		}
	}
}

void LlvmModule::addSubroutine(instance<LlvmSubroutine> subroutine)
{

}
*/