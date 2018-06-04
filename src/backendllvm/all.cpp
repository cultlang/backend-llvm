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


	//
	// LLVM - Compiler
	//
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompiler> compiler, instance<Constant> ast)
	{
		auto value = ast->getValue();

		compiler->state->lastReturnedValue = compiler->build_instanceAsConstant(value);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompiler> compiler, instance<lisp::Function> ast)
	{
		compiler->compile_setModule(ast->getSemantics()->getModule());
		compiler->compile_setFunction(ast);

		// TODO read through args, set names

		compiler->compile(ast->bodyAst());

		compiler->state->irBuilder->CreateRet(compiler->state->lastReturnedValue);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompiler> compiler, instance<Block> ast)
	{
		auto count = ast->statementCount();
		for (auto i = 0; i < count; i++)
		{
			compiler->compile(ast->statementAst(i));
		}
		// The last returned value is implictly set here
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
