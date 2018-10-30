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

				c->lastReturnedValue = c->genCall(c->getGenericFunction(sub), args);
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

			auto subroutineCall = c->getInternalFunction("___cult__runtime_subroutine_execute");

			auto sub = c->genAsConstant(c->lastReturnedInstance);
			c->genSpillInstances(args);
			auto argv = c->lastReturnedValue;
			auto argc = c->genAsConstant(args.size());

			c->lastReturnedValue = c->genCall(subroutineCall, {sub, argv, argc});
			return;
		}
		
		throw stdext::exception("Callsite {} is not compilable yet.", c->lastReturnedInstance);
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Condition> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Condition");

		// The count of all branches in the final PHI node, we add one for the default else branch
		auto count = ast->branchCount() + 1;

		std::vector<llvm::Value*> br_values(count, nullptr);
		std::vector<llvm::BasicBlock*> br_blocks(count, nullptr);

		// Get the entry and end blocks
		auto entry_block = c->irBuilder->GetInsertBlock();
		auto end_block = llvm::BasicBlock::Create(*c->context, "br-merge", c->codeFunction);

		// construct the final else branch
		auto else_block = llvm::BasicBlock::Create(*c->context, "br-else", c->codeFunction, end_block);
		c->irBuilder->SetInsertPoint(else_block);
		auto else_ast = ast->branchDefaultAst();
		if (else_ast)
		{
			c->compile(else_ast);
			br_values[0] = c->lastReturnedValue;
		}
		else
		{
			br_values[0] = c->genAsConstant(instance<>());
		}
		c->irBuilder->CreateBr(end_block);
		br_blocks[0] = else_block;

		// This loop goes in reverse
		auto i_else_block = else_block;
		for (auto ri = 0; ri < (count - 1); ++ri)
		{
			// actual index, goes from branchCount-1 -> 0; get the relevant branches
			auto i = (count - 2) - ri;
			auto i_condast = ast->branchConditionAst(i);
			auto i_ast = ast->branchAst(i);

			// get (or generate) the blocks
			auto i_block = llvm::BasicBlock::Create(*c->context, fmt::format("br-{0}", i), c->codeFunction, i_else_block);
			auto i_condblock = (i == 0) ? entry_block : llvm::BasicBlock::Create(*c->context, fmt::format("br-cond-{0}", i), c->codeFunction, i_block);

			// generate the cond block:
			c->irBuilder->SetInsertPoint(i_condblock);
			c->compile(i_condast);
			c->genTruth(c->lastReturnedValue);
			c->irBuilder->CreateCondBr(c->lastReturnedValue, i_block, i_else_block);

			// generate the code block:
			c->irBuilder->SetInsertPoint(i_block);
			c->compile(i_ast);
			c->irBuilder->CreateBr(end_block);

			// set the phi data
			br_values[ri + 1] = c->lastReturnedValue;
			br_blocks[ri + 1] = i_block;

			// set the downchain invariant
			i_else_block = i_condblock;
		}

		c->irBuilder->SetInsertPoint(end_block);
		auto phi = c->irBuilder->CreatePHI(c->getLlvmInstanceType(types::None), count);
		for (auto i = 0; i < count; ++i)
		{
			phi->addIncoming(br_values[i], br_blocks[i]);
		}

		c->lastReturnedValue = phi;
	});
	sem->builtin_implementMultiMethod("compile",
		[](instance<LlvmCompileState> c, instance<LlvmAbiBase> abi, instance<Loop> ast)
	{
		SPDLOG_TRACE(c->currentModule->getNamespace()->getEnvironment()->log(),
			"compile/Loop");


		auto entry_block =  c->irBuilder->GetInsertBlock();

		auto cond_block = llvm::BasicBlock::Create(*c->context, "loop-cond", c->codeFunction);
		auto loop_block = llvm::BasicBlock::Create(*c->context, "loop", c->codeFunction);
		auto endloop_block = llvm::BasicBlock::Create(*c->context, "loop-end", c->codeFunction);

		c->irBuilder->CreateBr(cond_block);
		
		// Compile loop exit
		c->irBuilder->SetInsertPoint(loop_block);
		c->compile(ast->bodyAst());
		c->lastReturnedValue = c->irBuilder->CreateLoad(c->lastReturnedValue);
		c->irBuilder->CreateBr(cond_block);
	

		// Compile and Store call to condition
		c->irBuilder->SetInsertPoint(cond_block);
		auto phi = c->irBuilder->CreatePHI(c->getLlvmInstanceType(types::None), 2, "loop-lastval");
		phi->addIncoming(c->genAsConstant(instance<>()), entry_block);
		phi->addIncoming(c->lastReturnedValue, loop_block);
		
		c->compile(ast->conditionAst());
		auto branch = c->genTruth(c->lastReturnedValue);
		c->irBuilder->CreateCondBr(branch, loop_block, endloop_block);


		c->irBuilder->SetInsertPoint(endloop_block);
		c->lastReturnedValue = phi;
		
	});

	module->getNamespace()->refreshBackends();
	module->getNamespace()->get<LlvmBackend>()->getCompiler()->builtin_validateSpecialForms(module);
}

BuiltinModuleDescription cultlang::backendllvm::BuiltinLlvm("cult/llvm", cultlang::backendllvm::make_llvm_bindings);


#include "types/dll_entry.inc"
