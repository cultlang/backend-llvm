#include "backendllvm/common.h"
/*
#include "lisp/backend/llvm/llvm_internal.h"
#include "lisp/backend/llvm/LlvmSubroutine.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmSubroutine)
{
	_.defaults();
}

LlvmSubroutine::LlvmSubroutine(instance<LlvmModule> module, instance<> lisp)
{
	_module = module;
	_lisp = lisp;
}

void LlvmSubroutine::generate()
{
	auto backend = _module->_backend;

	auto signature = _lisp.asFeature<SSubroutine>()->signature();
	auto type = backend->compiler->getLlvmType(signature);

	func = llvm::Function::Create(type, llvm::Function::ExternalLinkage, _binding_hint, _module->ir.get());

	BasicBlock* entry = BasicBlock::Create(backend->compiler->context, "entry", func);
	// TODO Figure out what the fuck FPMathTag does in the constructor
	IRBuilder<> builder(entry);

	auto ty_i64 = Type::getInt64Ty(backend->compiler->context);
	auto ir_zero = llvm::ConstantInt::get(ty_i64, (uint64_t)0);
	auto ir_array_size = llvm::ConstantInt::get(ty_i64, (uint64_t)signature->arguments.size());

	auto ir_array = builder.CreateAlloca(llvm::ArrayType::get(backend->compiler->type_instance, signature->arguments.size()));

	for (auto i = 0; i < signature->arguments.size(); ++i)
	{
		auto arg = signature->arguments[i];
		auto ir_index = llvm::ConstantInt::get(ty_i64, (uint64_t)i);
		auto ir_ptr = builder.CreateGEP(ir_array, { ir_zero, ir_index } );
		builder.CreateStore(func->arg_begin() + i, ir_ptr);
	}

	std::vector<llvm::Value*> call_args = {
		builder.CreateIntToPtr(llvm::ConstantInt::get(Type::getInt64Ty(backend->compiler->context), (uint64_t)&_lisp), backend->compiler->type_anyPtr),
		builder.CreatePointerCast(ir_array, llvm::PointerType::get(backend->compiler->type_instance, 0)),
		ir_array_size
	};
	llvm::Value* ret = builder.CreateCall(_module->ir->getFunction("__trampoline_interpreter"), call_args);
	builder.CreateRet(ret);

	std::string verify_str;
  /// XXX Clang Err: Taking the address of a temporary object
	verifyFunction(*func, nullptr);
	if (!verify_str.empty())
		backend->lisp->environment()->log()->info(verify_str);
}

*/