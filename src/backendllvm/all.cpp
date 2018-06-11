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
		auto value = ast->getValue();

		c->lastReturnedValue = c->genInstanceAsConstant(value);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<lisp::Function> ast)
	{
		c->setModule(ast->getSemantics()->getModule());
		c->setFunction(ast);

		// TODO read through args, set names

		c->compile(ast->bodyAst());

		c->genReturn(c->genInstanceCast(c->lastReturnedValue, types::None));
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Block> ast)
	{
		auto count = ast->statementCount();
		for (auto i = 0; i < count; i++)
		{
			c->compile(ast->statementAst(i));
		}
		// The last returned value is implictly set here
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<CallSite> ast)
	{
		auto count = ast->argCount();

		std::vector<llvm::Value*> args;
		args.reserve(count);

		for (auto i = 0; i < count; ++i)
		{
			c->compile(ast->argAst(i));
			args.push_back(c->lastReturnedValue);
		}

	});

	/*
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompiler> compiler, instance<CallSite> ast)
	{
		// First we need a callee

		auto count = ast->argCount();
		for (auto i = 0; i < count; i++)
		{
			compiler->compile(ast->argAst(i));
		}

		// Now we can get a 

		compiler->state->irBuilder->CreateCall()
	});
	*/

	module->getNamespace()->get<LlvmBackend>()->getCompiler()->builtin_validateSpecialForms(module);
}

BuiltinModuleDescription cultlang::backendllvm::BuiltinLlvm("cult/llvm", cultlang::backendllvm::make_llvm_bindings);


#include "types/dll_entry.inc"
