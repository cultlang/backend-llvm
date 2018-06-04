#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmSubroutine.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmSubroutine)
{
	_.defaults();
}

LlvmSubroutine::LlvmSubroutine(instance<LlvmModule> module, instance<SCultSemanticNode> ast)
{
	_module = module;
	_ast = ast;

	if (!_ast.isType<Function>())
		throw stdext::exception("LlvmSubroutine only supports Functions (`{0}` is not one).", _ast);
}
	
void LlvmSubroutine::generate()
{
	_module->getBackend()->getCompiler()->compile(_ast);
}

LlvmBackend::JitModule LlvmSubroutine::specialize(std::vector<TypeId>* types)
{
	if (types != nullptr)
		throw stdext::exception("Specialization not supported yet.");

	if (*_jit_handle_generic)
		return _jit_handle_generic;

	auto backend = _module->getBackend();

	auto function = _ast.asFeature<Function>();
	auto name = LlvmBackend::mangledName(function);
	auto type = backend->getCompiler()->getLlvmType(function->subroutine_signature());

	auto ir = std::make_unique<llvm::Module>(name, backend->getCompiler()->context);
	ir->setDataLayout(backend->_dl);
	auto func = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, ir.get());

	/*
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
	*/

	std::string verify_str;
	llvm::raw_string_ostream verify_strm(verify_str);
	verifyFunction(*func, &verify_strm);
	if (!verify_str.empty())
		backend->getNamespace()->getEnvironment()->log()->info(verify_str);

	_jit_handle_generic = cantFail(backend->_compileLayer.addModule(std::move(ir), backend->_resolver));

	return _jit_handle_generic;
}

std::string LlvmSubroutine::stringifyPrototypeIr()
{
	auto function = _ast.asFeature<Function>();
	auto name = LlvmBackend::mangledName(function);

	auto func = _module->getIr()->getFunction(name);
	if (func == nullptr)
		throw stdext::exception("Failed to find prototype in IR.");

	std::string res_str;
	llvm::raw_string_ostream res_strm(res_str);
	func->print(res_strm);

	return res_str;
}