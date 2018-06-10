#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmCompiler.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

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

	state = new CompilerState();
}

instance<LlvmBackend> LlvmCompiler::getBackend()
{
	return _backend;
}

void LlvmCompiler::compile(instance<lisp::SCultSemanticNode> node)
{
	try
	{
		auto res = _fn_system_compile.getFeature<PSubroutine>()->execute(_fn_system_compile, { craft_instance(), node });
	}
	catch (std::exception const& ex)
	{
		_backend->getNamespace()->getEnvironment()->log()->warn("Compiler does not support node `{1}`, default returned: {0}", ex.what(), node);
	}
}

void LlvmCompiler::compile_setModule(instance<lisp::Module> module)
{
	state->currentModule = module;
	state->codeModule = module->require<LlvmModule>()->getIr().get();
}
void LlvmCompiler::compile_setFunction(instance<lisp::Function> func)
{
	auto name = LlvmBackend::mangledName(func);

	state->codeFunction = llvm::Function::Create(
		getLlvmType(func->subroutine_signature()),
		llvm::Function::ExternalLinkage,
		name,
		state->codeModule);

	if (state->irBuilder != nullptr) delete state->irBuilder;
	state->irBuilder = new llvm::IRBuilder<>(_backend->context);

	auto block = llvm::BasicBlock::Create(_backend->context, name, state->codeFunction);
	state->irBuilder->SetInsertPoint(block);
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

	for (auto arg : tuple->entries)
	{
		args.push_back(getLlvmType(arg));
	}

	return_ = getLlvmType(arrow->output);

	return llvm::FunctionType::get(return_, args, false);
}

llvm::Value* LlvmCompiler::build_instanceAsConstant(instance<> inst)
{
	auto entry = _getTypeCache(inst.typeId());

	return llvm::ConstantStruct::getAnon(
		{
			llvm::ConstantExpr::getIntToPtr(llvm::ConstantInt::get(llvm::Type::getInt64Ty(_backend->context), (uint64_t)inst.asInternalPointer()), llvm::PointerType::get(type_instanceMetaHeader, 0)),
			llvm::ConstantExpr::getIntToPtr(llvm::ConstantInt::get(llvm::Type::getInt64Ty(_backend->context), (uint64_t)inst.get()), llvm::PointerType::get(entry.opaque_struct, 0))
		});
}

llvm::Value* LlvmCompiler::build_instanceCast(llvm::Value* value, TypeId type)
{
	_TypeCacheEntry* entry = nullptr;
	if (type != types::None) entry = &_getTypeCache(type);

	auto const targetPtrType = (entry == nullptr) ? type_anyPtr : llvm::PointerType::get(entry->opaque_struct, 0);

	if (auto *vconst = dyn_cast<llvm::Constant>(value))
	{
		return llvm::ConstantStruct::getAnon(
			{
				llvm::ConstantExpr::getExtractValue(vconst, { 0 }),
				llvm::ConstantExpr::getBitCast(llvm::ConstantExpr::getExtractValue(vconst, { 1 }), targetPtrType)
			});
	}
	else throw stdext::exception("Runtime casting not supported yet.");
}

void LlvmCompiler::builtin_validateSpecialForms(instance<lisp::Module> module)
{
	_fn_system_compile = module->get<CultSemantics>()->lookup(Symbol::makeSymbol("compile"))->getSite()->valueAst();
}
