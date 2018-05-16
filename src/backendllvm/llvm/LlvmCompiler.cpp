#include "backendllvm/common.h"
/*
#include "lisp/backend/llvm/llvm_internal.h"
#include "lisp/backend/llvm/LlvmCompiler.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmCompiler)
{
	_.defaults();
}

LlvmCompiler::LlvmCompiler()
{
	type_anyPtr = llvm::Type::getInt8PtrTy(context);
	type_instance = llvm::StructType::get(context, { type_anyPtr, type_anyPtr });
}

llvm::FunctionType* LlvmCompiler::getLlvmType(instance<SubroutineSignature> signature)
{
	llvm::Type* return_;
	std::vector<llvm::Type*> args;

	for (auto arg : signature->arguments)
	{
		args.push_back(type_instance);
	}

	return_ = type_instance;

	return llvm::FunctionType::get(return_, args, false);
}

void LlvmCompiler::compile_do(CompilerState&, instance<>)
{

}
void LlvmCompiler::compile_cond(CompilerState&, instance<>)
{

}
void LlvmCompiler::compile_while(CompilerState&, instance<>)
{

}
*/