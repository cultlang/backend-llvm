#include "backendllvm/common.h"

#include "llvm_internal.h"
#include "LlvmModule.h"

using namespace craft;
using namespace craft::types;
using namespace craft::lisp;

using namespace llvm;
using namespace llvm::orc;

CRAFT_DEFINE(LlvmModule)
{
	_.use<PSemantics>().singleton<LlvmSemanticsProvider>();

	_.defaults();
}

LlvmModule::LlvmModule(instance<LlvmBackend> backend, instance<lisp::Module> module)
{
	_backend = backend;
	_module = module;
}


instance<LlvmBackend> LlvmModule::getBackend() const
{
	return _backend;
}
instance<lisp::Module> LlvmModule::getModule() const
{
	return _module;
}
std::unique_ptr<llvm::Module>& LlvmModule::getIr()
{
	return _ir;
}

void LlvmModule::generate()
{
	if (_ir) return;

	auto s = _module->uri();
	_ir = std::make_unique<llvm::Module>(StringRef(s), _backend->context);
	_ir->setDataLayout(_backend->_dl);

	auto semantics = _module->require<CultSemantics>();
	auto stmt_count = semantics->countStatements();
	for (auto i = 0; i < stmt_count; ++i)
	{
		auto stmt = semantics->getStatement(i);

		if (!stmt.isType<BindSite>())
			continue;

		auto bindSite = stmt.asType<BindSite>();
		auto res = bindSite->valueAst();
		
		if (res.isType<Function>())
			require(res);
	}
}

instance<> LlvmModule::require(instance<SCultSemanticNode> snode)
{
	if (!snode.hasFeature<SBindable>())
		throw stdext::exception("Cannot require an llvm representation of a `{0}`.", snode);

	auto sym = snode.asFeature<SBindable>()->getBinding()->getSymbol();
	auto entry = _entries.lookup(sym);

	if (entry != nullptr)
	{
		assert(entry->_cult == snode);
		return entry->_llvm;
	}

	if (snode.isType<Function>())
	{
		auto subroutine = instance<LlvmSubroutine>::make(craft_instance(), snode);
		subroutine->generate();

		_entries.define(sym, [&](auto index) { return new _Entry{ snode, subroutine }; });
		return subroutine;
	}
	else
		throw stdext::exception("Cannot require an llvm representation of a `{0}`.", snode);
}

/******************************************************************************
** CultSemanticsProvider
******************************************************************************/

instance<lisp::Module> LlvmSemanticsProvider::getModule(instance<> semantics) const
{
	return semantics.asType<LlvmModule>()->getModule();
}

instance<> LlvmSemanticsProvider::read(instance<lisp::Module> into, ReadOptions const* opts) const
{
	auto building = instance<LlvmModule>::make(into->getNamespace()->get<LlvmBackend>(), into);

	return building;
}

instance<> LlvmSemanticsProvider::lookup(instance<> semantics_, instance<Symbol> sym) const
{
	instance<LlvmModule> semantics = semantics_;
	semantics->generate();
	return semantics->_entries.lookup(sym)->_llvm;
}
