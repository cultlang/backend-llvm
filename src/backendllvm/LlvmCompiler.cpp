#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmCompiler.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(SLlvmAbi)
{
	_.defaults();
}

/******************************************************************************
** LlvmCompiler
******************************************************************************/

CRAFT_DEFINE(LlvmCompiler)
{
	_.defaults();
}

LlvmCompiler::LlvmCompiler(instance<LlvmBackend> backend)
{
	_backend = backend;

	type_anyPtr = llvm::Type::getInt8PtrTy(_backend->context);
	type_instanceMetaHeader = llvm::StructType::create(_backend->context, "!instancemetaheader");
	type_instanceMetaHeader->setBody({ type_anyPtr, type_anyPtr, type_anyPtr, type_anyPtr });
	type_anyInstance = llvm::StructType::get(_backend->context, { llvm::PointerType::get(type_instanceMetaHeader, 0), type_anyPtr });
}

void LlvmCompiler::craft_setupInstance()
{
	_state = instance<LlvmCompileState>::make(craft_instance());
}

instance<LlvmBackend> LlvmCompiler::getBackend()
{
	return _backend;
}

void LlvmCompiler::compile(instance<lisp::SCultSemanticNode> node)
{
	_state->compile(node);
}

LlvmCompiler::_TypeCacheEntry LlvmCompiler::_getTypeCache(types::TypeId type)
{
	auto mclb = _typeCache.lower_bound(type);
	if (mclb != _typeCache.end() && !(_typeCache.key_comp()(type, mclb->first)))
		return mclb->second; // key already exists

	_TypeCacheEntry entry;
	entry.opaque_struct = StructType::create(_backend->context, type.toString(false));
	entry.instance = llvm::StructType::get(_backend->context, { llvm::PointerType::get(type_instanceMetaHeader, 0), llvm::PointerType::get(entry.opaque_struct, 0) });

	_typeCache.insert(mclb, { type, entry });
	return entry;
}

llvm::Type* LlvmCompiler::getLlvmInstanceType(types::TypeId type) { return _getTypeCache(type).instance; }
llvm::Type* LlvmCompiler::getLlvmValueType(types::TypeId type) { return _getTypeCache(type).opaque_struct; }
llvm::Type* LlvmCompiler::getLlvmValuePointerType(types::TypeId type) { return llvm::PointerType::get(_getTypeCache(type).opaque_struct, 0); }

llvm::Type* LlvmCompiler::getLlvmType(types::IExpression* node)
{
	auto kind = node->kind();
	if (kind.isType<ExpressionAny>())
	{
		return type_anyInstance;
	}
	else if (kind.isType<ExpressionConcrete>())
	{
		return getLlvmInstanceType(((ExpressionConcrete*)node)->node);
	}
	else throw stdext::exception("getLlvmType(): unknown node kind `{0}`.", kind);
}
llvm::FunctionType* LlvmCompiler::getLlvmType(types::ExpressionStore signature)
{
	llvm::Type* return_;
	std::vector<llvm::Type*> args;

	if (!signature.root()->kind().isType<ExpressionArrow>()) throw stdext::exception("getLlvmType(): malformed expression (not arrow).");
	auto arrow = (ExpressionArrow*)signature.root();

	if (!arrow->input->kind().isType<ExpressionTuple>()) throw stdext::exception("getLlvmType(): malformed expression (not tuple).");
	auto tuple = (ExpressionTuple*)arrow->input;

	// TODO windows ABI
	args.push_back(llvm::PointerType::get(getLlvmType(arrow->output), 0));

	for (auto arg : tuple->entries)
	{
		args.push_back(getLlvmType(arg));
	}

	//return_ = getLlvmType(arrow->output);
	return_ = llvm::Type::getVoidTy(_backend->context);

	auto ret = llvm::FunctionType::get(return_, args, false);

	return ret;
}

void LlvmCompiler::builtin_validateSpecialForms(instance<lisp::Module> module)
{
	_fn_system_compile = module->get<CultSemantics>()->lookup(Symbol::makeSymbol("compile"))->getSite()->valueAst();
}

/******************************************************************************
** LlvmCompileState
******************************************************************************/

CRAFT_DEFINE(LlvmCompileState)
{
	_.defaults();
}

LlvmCompileState::LlvmCompileState(instance<LlvmCompiler> compiler)
{
	_compiler = compiler;

	context = &getCompiler()->getBackend()->context;
	irBuilder = nullptr;
	lastReturnedValue = nullptr;
	codeModule = nullptr;
	codeFunction = nullptr;
}

void LlvmCompileState::craft_setupInstance()
{
	_abi = instance<LlvmAbiWindows>::make(craft_instance());
}

instance<LlvmCompiler> LlvmCompileState::getCompiler() const
{
	return _compiler;
}

void LlvmCompileState::compile(instance<lisp::SCultSemanticNode> node)
{
	auto _mm_compiler = _compiler->_fn_system_compile;

	try
	{
		auto res = _mm_compiler.getFeature<PSubroutine>()->execute(_mm_compiler, { craft_instance(), _abi, node });
	}
	catch (std::exception const& ex)
	{
		getCompiler()->getBackend()
			->getNamespace()->getEnvironment()
			->log()->warn("Compiler does not support node `{1}`, default returned: {0}", ex.what(), node);
	}
}

void LlvmCompileState::setModule(instance<lisp::Module> module)
{
	currentModule = module;
	codeModule = module->require<LlvmModule>()->getIr().get();
}
void LlvmCompileState::setFunction(instance<lisp::Function> func)
{
	currentFunction = func;
	auto name = LlvmBackend::mangledName(func, _abi->abiName());

	codeFunction = llvm::Function::Create(
		getCompiler()->getLlvmType(func->subroutine_signature()),
		llvm::Function::ExternalLinkage,
		name,
		codeModule);

	if (irBuilder != nullptr) delete irBuilder;
	irBuilder = new llvm::IRBuilder<>(*context);

	auto block = llvm::BasicBlock::Create(*context, name, codeFunction);
	irBuilder->SetInsertPoint(block);

	_abi->doFunctionPre();
}

llvm::Value* LlvmCompileState::genInstanceAsConstant(instance<> inst)
{
	auto entry = _compiler->_getTypeCache(inst.typeId());

	return llvm::ConstantStruct::getAnon(
		{
			llvm::ConstantExpr::getIntToPtr(
				llvm::ConstantInt::get(
					llvm::Type::getInt64Ty(*context),
					(uint64_t)inst.asInternalPointer()),
				llvm::PointerType::get(_compiler->type_instanceMetaHeader, 0)),
			llvm::ConstantExpr::getIntToPtr(
				llvm::ConstantInt::get(
					llvm::Type::getInt64Ty(*context),
					(uint64_t)inst.get()),
				llvm::PointerType::get(entry.opaque_struct, 0))
		});
}

llvm::Value* LlvmCompileState::genInstanceCast(llvm::Value* value, TypeId type)
{
	LlvmCompiler::_TypeCacheEntry* entry = nullptr;
	if (type != types::None) entry = &_compiler->_getTypeCache(type);

	auto const targetPtrType = (entry == nullptr) ? _compiler->type_anyPtr : llvm::PointerType::get(entry->opaque_struct, 0);

	if (auto *vconst = dyn_cast<llvm::Constant>(value))
	{
		return llvm::ConstantStruct::getAnon(
			{
				llvm::ConstantExpr::getExtractValue(vconst,{ 0 }),
				llvm::ConstantExpr::getBitCast(llvm::ConstantExpr::getExtractValue(vconst,{ 1 }), targetPtrType)
			});
	}
	else throw stdext::exception("Runtime casting not supported yet.");
}

void LlvmCompileState::genReturn(llvm::Value* v)
{
	_abi->genReturn(v);
}

/******************************************************************************
** LlvmAbiBase
******************************************************************************/

CRAFT_DEFINE(LlvmAbiBase)
{
	_.use<SLlvmAbi>().byCasting();

	_.defaults();
}

LlvmAbiBase::LlvmAbiBase(instance<LlvmCompileState> compileState)
{
	_c = compileState;
}

std::string LlvmAbiBase::abiName()
{
	return "";
}

void LlvmAbiBase::doFunctionPre()
{

}
void LlvmAbiBase::doFunctionPost()
{

}

void LlvmAbiBase::genReturn(llvm::Value* v)
{
	_c->irBuilder->CreateRet(v);
}

/******************************************************************************
** LlvmAbiWindows
******************************************************************************/

CRAFT_DEFINE(LlvmAbiWindows)
{
	_.parent<LlvmAbiBase>();

	_.defaults();
}

LlvmAbiWindows::LlvmAbiWindows(instance<LlvmCompileState> compileState)
	: LlvmAbiBase(compileState)
{

}

std::string LlvmAbiWindows::abiName()
{
	return "windows";
}

void LlvmAbiWindows::doFunctionPre()
{
	_c->codeFunction->addAttribute(1, llvm::Attribute::StructRet);
}

void LlvmAbiWindows::genReturn(llvm::Value* v)
{
	// Assert struct return

	auto ret = _c->codeFunction->arg_begin() + 0;

	if (auto *vconst = dyn_cast<llvm::Constant>(v))
	{
		_c->irBuilder->CreateStore(vconst, ret);
	}
	else
	{
		auto callee = llvm::Intrinsic::getDeclaration(_c->codeModule, llvm::Intrinsic::memcpy, {});
	}

	_c->irBuilder->CreateRetVoid();
}
