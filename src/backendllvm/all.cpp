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

		c->lastReturnedValue = c->genInstanceAsConstant(value);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Resolve> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Resolve");

		auto binding = ast->getBinding();
		auto binding_scope = binding->getScope();

		// TODO make MM:
		if (binding_scope.isType<lisp::Block>())
		{
			c->lastReturnedValue = c->getScopeValue(binding);
			if (ast->isGetter())
				c->lastReturnedValue = c->irBuilder->CreateLoad(c->lastReturnedValue);
			
			return;
		}
		// TODO: Add function scope here
		else // Implicitly scope level, may also be fall through for constants
		{
			auto bindvalue = binding->getSite()->valueAst();

			// TODO make MM:
			if (bindvalue.isType<lisp::MultiMethod>())
			{
				auto bindmm = bindvalue.asType<lisp::MultiMethod>();
				//bindmm->call_internal();
			}
			else if (bindvalue.isType<lisp::Function>())
			{
				auto bindfn = bindvalue.asType<lisp::Function>();
				auto bindfnTy = llvm::PointerType::get(c->getLlvmType(bindfn->subroutine_signature()), 0);
			}

			throw stdext::exception("Resolving `{0}` to bindsite `{1}`.", binding->getSymbol(), bindvalue);
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

		c->genInstanceAssign(storeLoc, storeVal);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<CallSite> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/CallSite");

		auto count = ast->argCount();

		std::vector<llvm::Value*> args;
		args.reserve(count);

		for (auto i = 0; i < count; ++i)
		{
			c->compile(ast->argAst(i));
			args.push_back(c->lastReturnedValue);
		}

		c->compile(ast->calleeAst());

		c->genCall(c->lastReturnedValue, args);
	});

	module->getNamespace()->refreshBackends();
	module->getNamespace()->get<LlvmBackend>()->getCompiler()->builtin_validateSpecialForms(module);
}

BuiltinModuleDescription cultlang::backendllvm::BuiltinLlvm("cult/llvm", cultlang::backendllvm::make_llvm_bindings);


#include "types/dll_entry.inc"
