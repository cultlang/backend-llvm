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

	// Load symbols into:
	backend->_internal_functions["___cult__runtime_truth"] = {
		(void*)&LlvmBackend::_cult_runtime_truth,
		llvm::FunctionType::get(llvm::Type::getInt1Ty(_backend->context), { type_anyInstance }, false),
	};
	backend->_internal_functions["___cult__runtime_subroutine_execute"] = {
		(void*)&LlvmBackend::_cult_runtime_subroutine_execute,
		llvm::FunctionType::get(type_anyInstance, { type_anyInstance, llvm::PointerType::get(type_anyInstance, 0), llvm::Type::getInt64Ty(_backend->context) }, false),
	};
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

llvm::Type* LlvmCompiler::getLlvmInstanceType(types::TypeId type)
{
	if (type == types::None)
		return type_anyInstance;
	return _getTypeCache(type).instance;
}
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

	for (auto arg : tuple->entries)
	{
		args.push_back(llvm::PointerType::get(getLlvmType(arg), 0));
	}

	return_ = getLlvmType(arrow->output);

	auto ret = llvm::FunctionType::get(return_, args, false);

	return ret;
}

llvm::FunctionType* LlvmCompiler::getInternalFunctionType(std::string const& name) const
{
	auto internal_it = _backend->_internal_functions.find(name);
	if (internal_it == _backend->_internal_functions.end())
		throw stdext::exception("Internal function {0} does not exist.", name);

	return internal_it->second.fntype;
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
LlvmCompileState::~LlvmCompileState()
{
	if (codeModule != nullptr)
		delete dataLayout;
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
			->log()->warn("Compiler does not support node `{1}`, default returned, exception: {0}", ex.what(), node);
	}
}

void LlvmCompileState::setModule(instance<lisp::Module> module)
{
	currentModule = module;
	codeModule = module->require<LlvmModule>()->getIr().get();
	dataLayout = new llvm::DataLayout(codeModule);

	_llfn_memcpy = llvm::Intrinsic::getDeclaration(codeModule, llvm::Intrinsic::memcpy, {
		llvm::Type::getInt8PtrTy(*context),
		llvm::Type::getInt8PtrTy(*context),
		llvm::Type::getInt64Ty(*context),
	});
	_llfn_memcpy->setLinkage(llvm::Function::ExternalLinkage);
}
void LlvmCompileState::setFunction(instance<lisp::Function> func)
{
	currentFunction = func;
	auto name = LlvmBackend::mangledName(func, _abi->abiName());

	codeFunction = llvm::Function::Create(
		getLlvmType(func->subroutine_signature()),
		llvm::Function::ExternalLinkage,
		name,
		codeModule);

	if (irBuilder != nullptr) delete irBuilder;
	irBuilder = new llvm::IRBuilder<>(*context);

	auto block = llvm::BasicBlock::Create(*context, name, codeFunction);
	irBuilder->SetInsertPoint(block);

	_abi->doFunctionPre(codeFunction);
}

void LlvmCompileState::pushScope(instance<lisp::SScope> scope)
{
	// TODO make MM
	scopeStack.push_back({ scope, {} });
	if(scope.isType<Block>())
	{
		if(!scope->getSlotCount())
			return;

		lastReturnedValue = irBuilder->CreateAlloca(
			_compiler->type_anyInstance,
			llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), scope->getSlotCount())
		);
		auto count = scope->getSlotCount();
		for(auto i = 0; i < count; i++)
		{
			auto bind = scope->lookupSlot(i);
			scopeStack.back().values[bind] = irBuilder->CreateGEP(lastReturnedValue, {
				llvm::ConstantInt::get(*context, llvm::APInt(32, i))
			}, bind->getSymbol()->getDisplay());
		}
	}
	else if(scope.isType<lisp::Function>())
	{
		//XXX(clarkrinker) Branch Assumes ~scope~ is scope that was set by SetFunction

		auto fn = scope.asType<lisp::Function>();

		auto count = fn->argCount();
		for (auto i = 0; i < count; i++)
		{
			auto bind = scope->lookupSlot(i);
			auto arg = codeFunction->arg_begin() + _abi->getArgumentIndex(i);
			arg->setName(bind->getSymbol()->getDisplay());
			scopeStack.back().values[bind] = arg;
		}
	}
	
	
}
void LlvmCompileState::popScope()
{
	scopeStack.pop_back();
}
llvm::Value* LlvmCompileState::getScopeValue(instance<lisp::Binding> bind)
{
	llvm::Value* alloc = nullptr;
	auto searchScope = bind->getScope();
	for (auto map_it = scopeStack.rbegin(); map_it != scopeStack.rend(); ++map_it)
	{
		if (map_it->scope == searchScope)
		{
			return map_it->values[bind];
		}
	}
	return nullptr;
}


llvm::Value* LlvmCompileState::getInternalFunction(std::string const& name)
{
	auto type = _abi->getTypeSignature(getCompiler()->getInternalFunctionType(name));

	auto res = codeModule->getOrInsertFunction(name, type, { });
	if (auto res_func = dyn_cast<llvm::Function>(res))
	{
		res_func->setLinkage(llvm::Function::ExternalLinkage);
		_abi->doFunctionPre(res_func);
	}

	return res;
}

llvm::Constant* LlvmCompileState::genAsConstant(size_t v)
{
	return llvm::ConstantInt::get(*context, llvm::APInt(64, v));
}

llvm::Constant* LlvmCompileState::genAsConstant(instance<> inst)
{
	auto entry = _compiler->_getTypeCache(inst.typeId());

	return llvm::ConstantStruct::getAnon(
		{
			llvm::ConstantExpr::getIntToPtr(
				genAsConstant((uintptr_t)inst.asInternalPointer()),
				llvm::PointerType::get(_compiler->type_instanceMetaHeader, 0)),
			llvm::ConstantExpr::getIntToPtr(
				genAsConstant((uintptr_t)inst.get()),
				_compiler->type_anyPtr)
				//llvm::PointerType::get(entry.opaque_struct, 0)) // TODO use When we add types properly
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

llvm::Value* LlvmCompileState::genCall(llvm::Value* calle, std::vector<llvm::Value*> const& args)
{
	return lastReturnedValue = _abi->genCall(calle, args);
}

llvm::Value* LlvmCompileState::genPushInstance()
{
	return lastReturnedValue = irBuilder->CreateAlloca(
			_compiler->type_anyInstance,
			llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 1)
		);
}

void LlvmCompileState::genInstanceAssign(llvm::Value* dest, llvm::Value* src)
{
	// TODO incref

	if (auto *srcconst = dyn_cast<llvm::Constant>(src))
	{
		irBuilder->CreateStore(srcconst, dest);
	}
	else if (!src->getType()->isPointerTy())
	{
		irBuilder->CreateStore(src, dest);
	}
	else
	{
		// TODO: do something about this:
		auto inst_size = dataLayout->getTypeAllocSize(_compiler->type_anyInstance);

		irBuilder->CreateCall(_llfn_memcpy, {
			irBuilder->CreatePointerCast(dest, _compiler->type_anyPtr),
			irBuilder->CreatePointerCast(src, _compiler->type_anyPtr),
			llvm::ConstantInt::get(*context, llvm::APInt(64, inst_size)),
			llvm::ConstantInt::get(*context, llvm::APInt(32, 0)), // Remove in llvm:v7
			llvm::ConstantInt::get(*context, llvm::APInt(1, false))
		});
	}
}

void LlvmCompileState::genSpillInstances(std::vector<llvm::Value*> const& spill)
{
	lastReturnedValue = irBuilder->CreateAlloca(
		_compiler->type_anyInstance,
		llvm::ConstantInt::get(*context, llvm::APInt(64, spill.size()))
	);

	for (auto i = 0; i < spill.size(); ++i)
	{
		auto dest = irBuilder->CreateGEP(lastReturnedValue, {
				llvm::ConstantInt::get(*context, llvm::APInt(32, i))
		});

		genInstanceAssign(dest, spill[i]);
	}
}

llvm::Value* LlvmCompileState::genTruth(llvm::Value* v)
{
	auto subroutine = getInternalFunction("___cult__runtime_truth");

	return lastReturnedValue = genCall(subroutine, {v});
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

void LlvmAbiBase::doFunctionPre(llvm::Function*)
{

}
void LlvmAbiBase::doFunctionPost(llvm::Function*)
{

}

size_t LlvmAbiBase::getArgumentIndex(size_t i) const
{
	return i;
}
llvm::FunctionType* LlvmAbiBase::getTypeSignature(llvm::FunctionType* fnty) const
{
	return fnty;
}

void LlvmAbiBase::genReturn(llvm::Value* v)
{
	_c->irBuilder->CreateRet(v);
}

llvm::Value* LlvmAbiBase::genCall(llvm::Value* callee, std::vector<llvm::Value*> const& args)
{
	return _c->irBuilder->CreateCall(callee, args);
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

void LlvmAbiWindows::doFunctionPre(llvm::Function* fn)
{
	auto ret_arg = fn->arg_begin() + 0;
	ret_arg->setName("ret");
	ret_arg->addAttr(llvm::Attribute::StructRet);
}

size_t LlvmAbiWindows::getArgumentIndex(size_t i) const
{
	return i + 1;
}
llvm::FunctionType* LlvmAbiWindows::getTypeSignature(llvm::FunctionType* fnty) const
{
	auto& context = _c->getCompiler()->getBackend()->context;

	std::vector<llvm::Type*> args;
	llvm::Type* _return = fnty->getReturnType();

	if (!_return->isVoidTy() &&
		_c->dataLayout->getTypeAllocSize(_return) > 8)
	{
		args.push_back(llvm::PointerType::get(_return, 0));
		_return = llvm::Type::getVoidTy(context);
	}

	for (auto arg : fnty->params())
	{
		if (_c->dataLayout->getTypeAllocSize(arg) > 8)
		{
			args.push_back(llvm::PointerType::get(arg, 0));
		}
		else
		{
			args.push_back(arg);
		}
	}

	return llvm::FunctionType::get(_return, args, false);
}

void LlvmAbiWindows::genReturn(llvm::Value* v)
{
	// Assert struct return
	auto ret = _c->codeFunction->arg_begin() + 0;

	_c->genInstanceAssign(ret, v);

	_c->irBuilder->CreateRetVoid();
}

llvm::Value* LlvmAbiWindows::genCall(llvm::Value* callee, std::vector<llvm::Value*> const& args)
{
	std::vector<llvm::Value*> mod_args;
	llvm::Value* return_value = nullptr;

	auto func = dyn_cast<llvm::Function>(callee);
	if (func == nullptr)
		throw stdext::exception("Windows ABI genCall callee's must be functions at the moment.");

	// Check how we are returning from this
	if (func->arg_size() > 0
		&& func->hasAttribute(1, llvm::Attribute::StructRet))
	{
		return_value = _c->genPushInstance();
		mod_args.push_back(return_value);
	}

	for (auto i = 0; i < args.size(); ++i)
	{
		auto arg = args[i];

		if (!arg->getType()->isPointerTy()
			&& _c->dataLayout->getTypeAllocSize(arg->getType()) > 8)
		{
			auto new_arg = _c->genPushInstance();
			mod_args.push_back(new_arg);
			_c->genInstanceAssign(new_arg, arg);
		}
		else
		{
			mod_args.push_back(arg);
		}
	}

	auto call_value = _c->irBuilder->CreateCall(callee, mod_args);
	if (return_value == nullptr)
		return_value = call_value;
	return return_value;
}
