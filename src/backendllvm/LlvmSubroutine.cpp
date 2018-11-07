#pragma warning( push )
#pragma warning( disable : 4244)
#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmSubroutine.h"

#include "llvm/Transforms/Utils/Cloning.h"

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

	_jitted = false;

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

	if (_jitted)
		return _jit_handle_generic;

	auto backend = _module->getBackend();

	auto function = _ast.asFeature<Function>();
	auto name = getName();

	auto proto_func = _module->getIr()->getFunction(name);
	std::string verify_str;
	llvm::raw_string_ostream verify_strm(verify_str);
	verifyModule(*_module->getIr().get(), &verify_strm);
	verify_strm.flush();
	if (!verify_str.empty())
		backend->getEnvironment()->log()->info(verify_str);

	auto ir = std::make_unique<llvm::Module>(_module->getModule()->uri(), backend->context);
	ir->setDataLayout(backend->_dl);
	ir->setTargetTriple(llvm::sys::getDefaultTargetTriple());

	auto func = llvm::Function::Create(proto_func->getFunctionType(), llvm::Function::ExternalLinkage, name, ir.get());
	func->setDLLStorageClass(llvm::Function::DLLExportStorageClass);

	// TODO: specialize for ABI type??
	llvm::ValueToValueMapTy vvmap;
	auto funcArgIt = func->arg_begin();
	for (auto const& it : proto_func->args())
		if (vvmap.count(&it) == 0)
		{
			funcArgIt->setName(it.getName());
			vvmap[&it] = &*funcArgIt++;
		}
	llvm::SmallVector<llvm::ReturnInst*, 8> returns;
	llvm::CloneFunctionInto(func, proto_func, vvmap, true, returns);
	func->setCallingConv(llvm::CallingConv::Win64);

	//std::string verify_str;
	//llvm::raw_string_ostream verify_strm(verify_str);
	//verifyModule(*ir.get(), &verify_strm);
	//verify_strm.flush();
	//if (!verify_str.empty())
	//	backend->getEnvironment()->log()->info(verify_str);

	ir->print(llvm::errs(), nullptr);

	_jit_handle_generic = backend->addModule(std::move(ir));
	_jitted = true;

	return _jit_handle_generic;
}

instance<> LlvmSubroutine::invoke(GenericInvoke const& invk)
{
	auto function = _ast.asFeature<Function>();
	auto name = getName();

	specialize();
	auto backend = _module->getBackend();
	auto res = backend->getSymbolAddress(name);

	if (res == 0)
	{
		SPDLOG_TRACE(_module->getModule()->getEnvironment()->log(),
			"LlvmSubroutine::invoke: null getSymbolAddress");
		return instance<>();
	}
		
	return types::invoke(function->subroutine_signature(), types::Function(res), invk);
}

std::string LlvmSubroutine::stringifyPrototypeIr()
{
	auto function = _ast.asFeature<Function>();
	auto name = getName();

	auto func = _module->getIr()->getFunction(name);
	if (func == nullptr)
		throw stdext::exception("Failed to find prototype in IR.");

	std::string res_str;
	llvm::raw_string_ostream res_strm(res_str);
	func->print(res_strm);

	return res_str;
}

llvm::FunctionType* LlvmSubroutine::getLlvmType()
{
	auto backend = _module->getBackend();
	auto compiler = backend->getCompiler();
	if(_ast.isType<lisp::Function>())
	{
		return compiler->getLlvmType(_ast.asType<lisp::Function>()->subroutine_signature());
	}

	throw stdext::exception("Unsupported ast Type {}", _ast);
}

std::string LlvmSubroutine::getName()
{
	auto function = _ast.asFeature<Function>();
	return LlvmBackend::mangledName(function, "windows");
}
#pragma warning( pop ) 