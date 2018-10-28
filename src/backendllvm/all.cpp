#include "backendllvm/common.h"
#include "all.h"

#include "lisp/semantics/cult/semantics.h"
#include "llvm_internal.h"

using namespace craft;
using namespace craft::lisp;
using namespace craft::types;

using namespace cultlang::backendllvm;



void cultlang::backendllvm::make_llvm_bindings(instance<Module> module)
{
	auto sem = module->require<CultSemantics>();

	//
	// LLVM - Helpers
	//
	sem->builtin_implementMultiMethod("LlvmCompiler",
		[]() -> instance<LlvmCompiler>
	{
		return Execution::getCurrent()->getNamespace()->get<LlvmBackend>()->getCompiler();
	});

	sem->builtin_implementMultiMethod("llvm-ir",
		[](instance<SCultSemanticNode> node)
	{
		auto res = Execution::getCurrent()->getNamespace()->get<LlvmBackend>()->require(node);

		if (res.isType<LlvmSubroutine>())
		{
			return instance<std::string>::make(res.asType<LlvmSubroutine>()->stringifyPrototypeIr());
		}
		else return instance<std::string>();
	});

	sem->builtin_implementMultiMethod("llvm-call",
		[](instance<SCultSemanticNode> node, VarArgs<instance<>> args)
	{
		auto res = Execution::getCurrent()->getNamespace()->get<LlvmBackend>()->require(node);

		if (res.isType<LlvmSubroutine>())
		{
			auto sub = res.asType<LlvmSubroutine>();

			GenericInvoke invoke(args.args.size());
			std::copy(args.args.begin(), args.args.end(), std::back_inserter(invoke.args));

			return sub->invoke(invoke);
		}
		else throw stdext::exception("`{0}` is not a callable object.", node);
	});


	//
	// LLVM - Compiler
	//
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Constant> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Constant");

		auto value = ast->getValue();

		c->lastReturnedValue = c->genAsConstant(value);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Resolve> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Resolve");

		auto binding = ast->getBinding();
		auto binding_scope = binding->getScope();

		// TODO make MM:
		//binding_scope.isType<lisp::Block>()
		if (!binding_scope.isType<CultSemantics>())
		{
			c->lastReturnedValue = c->getScopeValue(binding);
			// if (ast->isGetter()
			// 	&& c->lastReturnedValue->getType()->isPointerTy()) // TODO Use AST To determine if binding is value
			// 	c->lastReturnedValue = c->irBuilder->CreateLoad(c->lastReturnedValue);
			
			c->lastReturnedInstance = nullptr;
			return;
		}
		else // Implicitly scope level, may also be fall through for constants
		{
			c->lastReturnedInstance = binding->getSite()->valueAst();
			c->lastReturnedValue = nullptr;
			return;
			
			// throw stdext::exception("Resolving `{0}` to bindsite `{1}`.", binding->getSymbol(), bindvalue);
		}

		throw stdext::exception("Resolving `{0}` is not compilable yet.", binding->getSymbol());
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Assign> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Assign");

		c->compile(ast->slotAst());
		auto storeLoc = c->lastReturnedValue;

		c->compile(ast->valueAst());
		auto storeVal = c->lastReturnedValue;

		c->genInstanceAssign(storeLoc, storeVal);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<lisp::Function> ast)
	{
		c->setModule(ast->getSemantics()->getModule());
		c->setFunction(ast);

		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Function");

		// TODO read through args, set names (do this with push scope and a specialization there)
		c->pushScope(ast);
		auto count = ast->argCount();
		for(auto i = 0; i < count; i++)
		{
			c->compile(ast->argAst(i));
		}

		c->compile(ast->bodyAst());

		c->genReturn(c->lastReturnedValue);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Block> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Block");

		c->pushScope(ast);

		auto count = ast->statementCount();
		for (auto i = 0; i < count; i++)
		{
			c->compile(ast->statementAst(i));
		}
		// The last returned value is implictly set here
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Variable> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Variable");
		
		auto storeLoc = c->getScopeValue(ast->getBinding());

		// Dispatch the below to assign some how?
		auto iAst = ast->initalizerAst();
		if(iAst) 
		{
			c->compile(ast->initalizerAst());
			auto storeVal = c->lastReturnedValue;

			c->genInstanceAssign(storeLoc, storeVal);
		}
		else
		{
			c->lastReturnedValue = nullptr;
		}
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<BindSite> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/BindSite");

		// TODO elide scope lookup (and scope array size) for constants

		if (ast->isDynamicBind())
			throw stdext::exception("Compiler does not support dynamic binds.");
		if (ast->isAttachSite())
			throw stdext::exception("Compiler does not support attach.");

		auto storeLoc = c->getScopeValue(ast->getBinding());

		// Dispatch the below to assign some how?
		c->compile(ast->valueAst());
		auto storeVal = c->lastReturnedValue;
		if(storeVal)
			c->genInstanceAssign(storeLoc, storeVal);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<CallSite> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/CallSite");

		auto count = ast->argCount();

		std::vector<llvm::Value*> args;
		//TODO Windows ABI
		args.reserve(count + 1);

		for (auto i = 0; i < count; ++i)
		{
			c->compile(ast->argAst(i));
			args.push_back(c->lastReturnedValue);
		}

		c->compile(ast->calleeAst());
		if(!c->lastReturnedInstance)
		{
			c->lastReturnedValue = c->genCall(c->lastReturnedValue, args);
			return;
		}
		else if(c->lastReturnedInstance.isType<lisp::Function>())
		{
			auto bindfn = c->lastReturnedInstance.asType<lisp::Function>();
			auto res = Execution::getCurrent()->getNamespace()->get<LlvmBackend>()->require(bindfn);

			if (res.isType<LlvmSubroutine>())
			{
				auto sub = res.asType<LlvmSubroutine>();

				sub->specialize();

				auto callee = c->codeModule->getOrInsertFunction(sub->getName(), sub->getLlvmType(), {});

				c->lastReturnedValue = c->genCall(callee, args);
				return;
			}
			else throw stdext::exception("`{0}` is not a callable object.", res);
		}
		//else if(c->lastReturnedInstance.isType<lisp::MultiMethod>()) 
		//{
		//	auto bindmm = bindvalue.asType<lisp::MultiMethod>();
		//}
		else if(c->lastReturnedInstance.hasFeature<lisp::PSubroutine>()) 
		{
			auto psub = c->lastReturnedInstance.getFeature<lisp::PSubroutine>();

			auto subroutineCall = c->getInternalFunction("___cult__PSubroutine__runtime_execute");

			auto sub = c->genAsConstant(c->lastReturnedInstance);
			c->genSpillInstances(args);
			auto argv = c->lastReturnedValue;
			auto argc = c->genAsConstant(args.size());

			c->lastReturnedValue = c->genCall(subroutineCall, {sub, argv, argc});
			return;
		}
		
		throw stdext::exception("Unsupported CallSite {}", c->lastReturnedInstance);
	});

	module->getNamespace()->refreshBackends();
	module->getNamespace()->get<LlvmBackend>()->getCompiler()->builtin_validateSpecialForms(module);
}

BuiltinModuleDescription cultlang::backendllvm::BuiltinLlvm("cult/llvm", cultlang::backendllvm::make_llvm_bindings);


#include "types/dll_entry.inc"
