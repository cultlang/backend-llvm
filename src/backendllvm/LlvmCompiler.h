#pragma once
#include "backendllvm/common.h"
#include "llvm_internal.h"

namespace craft {
namespace lisp
{
	// Forward Declarations
	class LlvmCompiler;
	class LlvmCompileState;
	class LlvmAbiBase;
	class LlvmAbiWindows;
	/******************************************************************************
	** SLlvmAbi
	******************************************************************************/

	class SLlvmAbi
		: public craft::types::Aspect
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_LEGACY_FEATURE_DECLARE(craft::lisp::SLlvmAbi, "llvm.abi", types::FactoryAspectManager);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED virtual std::string abiName() = 0;

		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Function* getOrMakeFunction(llvm::Module* module, std::string const& name, llvm::FunctionType* fn_type, bool declare) const = 0;

		CULTLANG_BACKENDLLVM_EXPORTED virtual size_t transmuteArgumentIndex(size_t) const = 0;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::FunctionType* transmuteSignature(llvm::FunctionType*) const = 0;

		CULTLANG_BACKENDLLVM_EXPORTED virtual void genReturn(llvm::Value*) = 0;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Value* genCall(llvm::Value*, std::vector<llvm::Value*> const& args) = 0;
	};

	/******************************************************************************
	** LlvmCompiler
	******************************************************************************/

	class LlvmCompiler
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmCompiler);

	protected:
		friend class LlvmBackend;
		friend class LlvmCompileState;
		instance<LlvmBackend> _backend;

		llvm::Type* type_anyPtr;
		llvm::StructType* type_instanceMetaHeader;
		llvm::Type* type_anyInstance;

		struct _TypeCacheEntry
		{
			llvm::StructType* opaque_struct;
			llvm::StructType* instance;
		};

		std::map<types::TypeId, _TypeCacheEntry> _typeCache;

		instance<LlvmCompileState> _state;

	private:
		instance<MultiMethod> _fn_system_compile;

		CULTLANG_BACKENDLLVM_EXPORTED _TypeCacheEntry _getTypeCache(types::TypeId type);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmCompiler(instance<LlvmBackend> backend);
		CULTLANG_BACKENDLLVM_EXPORTED void craft_setupInstance();

		CULTLANG_BACKENDLLVM_EXPORTED instance<LlvmBackend> getBackend();

	public:
		CULTLANG_BACKENDLLVM_EXPORTED void compile(instance<lisp::SCultSemanticNode> node);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmInstanceType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmValueType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmValuePointerType(types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Type* getLlvmType(types::IExpression* node);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::FunctionType* getLlvmType(types::ExpressionStore signature);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::FunctionType* getInternalFunctionType(std::string const& name) const;

	public:

		// Ensures the module has everything the interpreter needs
		CULTLANG_BACKENDLLVM_EXPORTED void builtin_validateSpecialForms(instance<lisp::Module> module);
	};

	/******************************************************************************
	** LlvmCompileState
	******************************************************************************/

	class LlvmCompileState
		: public virtual craft::types::Object
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmCompileState);
	private:
		instance<LlvmCompiler> _compiler;
		instance<SLlvmAbi> _abi;

	public:
		llvm::LLVMContext* context;

		instance<lisp::Module> currentModule;
		llvm::Module* codeModule;
		instance<lisp::Function> currentFunction;
		llvm::Function* codeFunction;

		llvm::DataLayout* dataLayout;

		llvm::IRBuilder<>* irBuilder;
		llvm::Value* lastReturnedValue;
		instance<> lastReturnedInstance;

		struct ScopeMap
		{
			instance<lisp::SScope> scope;
			std::map<instance<Binding>, llvm::Value*> values;
		};
		std::vector<ScopeMap> scopeStack;

	private:
		llvm::Function* _llfn_memcpy;

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmCompileState(instance<LlvmCompiler> compiler);
		CULTLANG_BACKENDLLVM_EXPORTED void craft_setupInstance();
		CULTLANG_BACKENDLLVM_EXPORTED ~LlvmCompileState();

		CULTLANG_BACKENDLLVM_EXPORTED instance<LlvmCompiler> getCompiler() const;

		CULTLANG_BACKENDLLVM_EXPORTED void setAbi(instance<SLlvmAbi>);
		CULTLANG_BACKENDLLVM_EXPORTED instance<SLlvmAbi> getAbi();

	public:
		CULTLANG_BACKENDLLVM_EXPORTED void compile(instance<lisp::SCultSemanticNode> node);

		CULTLANG_BACKENDLLVM_EXPORTED void setModule(instance<lisp::Module> module);
		CULTLANG_BACKENDLLVM_EXPORTED void setFunction(instance<lisp::Function> func);

		// scope manipulation
	public:
		CULTLANG_BACKENDLLVM_EXPORTED void pushScope(instance<lisp::SScope> scope);
		CULTLANG_BACKENDLLVM_EXPORTED void popScope();
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* getScopeValue(instance<lisp::Binding> bind);
		
		// compile helpers
	public:
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* getInternalFunction(std::string const& name);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* getGenericFunction(instance<lisp::LlvmSubroutine> sub);

		CULTLANG_BACKENDLLVM_EXPORTED llvm::Constant* genAsConstant(size_t);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Constant* genAsConstant(instance<> inst);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* genInstanceCast(llvm::Value*, types::TypeId type);
		CULTLANG_BACKENDLLVM_EXPORTED void genReturn(llvm::Value*);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* genCall(llvm::Value*, std::vector<llvm::Value*> const& args);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* genPushInstance();
		CULTLANG_BACKENDLLVM_EXPORTED void genInstanceAssign(llvm::Value* dest, llvm::Value* src);
		CULTLANG_BACKENDLLVM_EXPORTED void genSpillInstances(std::vector<llvm::Value*> const& spill);
		CULTLANG_BACKENDLLVM_EXPORTED llvm::Value* genTruth(llvm::Value*);

		// Forwarding helpers
	public:
		inline llvm::Type* getLlvmInstanceType(types::TypeId type) const { return getCompiler()->getLlvmInstanceType(type); }
		inline llvm::Type* getLlvmValueType(types::TypeId type) const { return getCompiler()->getLlvmValueType(type); }
		inline llvm::Type* getLlvmValuePointerType(types::TypeId type) const { return getCompiler()->getLlvmValuePointerType(type); }
		inline llvm::Type* getLlvmType(types::IExpression* node) const { return getCompiler()->getLlvmType(node); }
		inline llvm::FunctionType* getLlvmType(types::ExpressionStore signature) const
		{
			return _abi->transmuteSignature(getCompiler()->getLlvmType(signature));
		}

	};

	/******************************************************************************
	** LlvmAbiBase
	******************************************************************************/

	// Base ABI / pure llvm abi
	class LlvmAbiBase
		: public virtual craft::types::Object
		, public craft::types::Implements<SLlvmAbi>
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmAbiBase);

	protected:
		instance<LlvmCompileState> _c;

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmAbiBase(instance<LlvmCompileState> compileState);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED virtual std::string abiName() override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Function* getOrMakeFunction(llvm::Module* module, std::string const& name, llvm::FunctionType* fn_type, bool declare) const override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual size_t transmuteArgumentIndex(size_t) const override;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::FunctionType* transmuteSignature(llvm::FunctionType*) const override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual void genReturn(llvm::Value*) override;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Value* genCall(llvm::Value*, std::vector<llvm::Value*> const& args) override;
	};

	/******************************************************************************
	** LlvmAbiWindows
	******************************************************************************/

	// Windows ABI
	class LlvmAbiWindows
		: public LlvmAbiBase
	{
		CULTLANG_BACKENDLLVM_EXPORTED CRAFT_OBJECT_DECLARE(craft::lisp::LlvmAbiWindows);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED LlvmAbiWindows(instance<LlvmCompileState> compileState);

	public:
		CULTLANG_BACKENDLLVM_EXPORTED bool requiresStructReturn(llvm::FunctionType*) const;

		CULTLANG_BACKENDLLVM_EXPORTED virtual std::string abiName() override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Function* getOrMakeFunction(llvm::Module* module, std::string const& name, llvm::FunctionType* fn_type, bool declare) const override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual size_t transmuteArgumentIndex(size_t) const override;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::FunctionType* transmuteSignature(llvm::FunctionType*) const override;

		CULTLANG_BACKENDLLVM_EXPORTED virtual void genReturn(llvm::Value*) override;
		CULTLANG_BACKENDLLVM_EXPORTED virtual llvm::Value* genCall(llvm::Value*, std::vector<llvm::Value*> const& args) override;
	};
}}
