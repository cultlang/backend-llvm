#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	class LlvmCompiler
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmCompiler);

	protected:
		friend LlvmBackend;
		instance<LlvmBackend> _backend;

		llvm::Type* type_anyPtr;
		llvm::Type* type_instanceMetaHeader;
		llvm::Type* type_anyInstance;

		struct _TypeCacheEntry
		{
			llvm::StructType* opaque_struct;
			llvm::StructType* instance;
		};

		std::map<types::TypeId, _TypeCacheEntry> _typeCache;

	public:
		llvm::LLVMContext context;

		struct CompilerState
		{
			instance<lisp::Module> currentModule;
			llvm::Module* codeModule;
			llvm::Function* codeFunction;

			llvm::IRBuilder<>* irBuilder;
			llvm::Value* lastReturnedValue;
		};

		CompilerState* state;

	private:
		instance<MultiMethod> _fn_system_compile;

		CULTLANG_BACKENDLLVM_EXPORTED _TypeCacheEntry _getTypeCache(types::TypeId type);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmCompiler(instance<LlvmBackend> backend);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED void compile(instance<lisp::SCultSemanticNode> node);
		CULTLANG_BACKENDLLVM_EXPORTED void compile_setModule(instance<lisp::Module> module);
		CULTLANG_BACKENDLLVM_EXPORTED void compile_setFunction(instance<lisp::Function> func);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmInstanceType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmValueType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmValuePointerType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmType(types::IExpression* node);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::FunctionType* getLlvmType(types::ExpressionStore signature);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* build_instanceAsConstant(instance<> inst);

	public:

		// Ensures the module has everything the interpreter needs
		CULTLANG_BACKENDLLVM_EXPORTED void builtin_validateSpecialForms(instance<lisp::Module> module);
	};

}}
