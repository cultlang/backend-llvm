#include "backendllvm/common.h"
#include "backendllvm/all.h"

#include "replxx/replxx.hxx"
#include "lisp/semantics/cult/cult.h"
#include "lisp/library/system/prelude.h"
#include "lisp/features/repl.h"

using namespace craft;
using namespace craft::lisp;

void ensure_dlls()
{
	auto target = path::dir(path::executable_path());
	auto files = path::list_files(target);
	for (auto f : files)
	{
		auto ext = f.substr(f.find('.') + 1);
		if (ext == "dll" || ext == "so" || ext == "dylib")
		{
			craft::types::load_dll(path::join(target, f));
		}
	}
}


int main(int argc, char** argv)
{
	craft::types::boot();
	ensure_dlls();
	if (argc != 1)
	{
		std::vector<instance<std::string>> _argv;
		for (auto i = 0; i < argc; i++)
		{
			_argv.push_back(instance<std::string>::make(argv[i]));
		}
		instance<Environment> global_env = instance<Environment>::make(spdlog::stdout_color_mt("environment"), _argv);
		instance<Namespace> ns = global_env->ns_user;
		std::string f;
		try
		{
			f = argv[1];
		}
		catch (...)
		{
			global_env->log()->info("No Such File: {0}", argv[1]);
		}
		try
		{
			auto live_module = ns->requireModule(instance<>(), fmt::format("file:{0}", f));
			live_module->initialize();
		}
		catch (std::exception const& e)
		{
			global_env->log()->error(e.what());
			return -1;
		}
	}
	else
	{
		features::LispRepl r;

		while (true)
		{
			try
			{
				auto eval = r.step();
				std::cout << eval.toString() << "\n";
			}
			catch (features::ReplExitException e)
			{
				break;
			}
			catch (features::ReplParseException const& e)
			{
				std::cout << "Parse Error: " << e.what() << "\n";
			}
			catch (std::exception const& e)
			{
				std::cout << e.what() << "\n";
			}
			catch (...)
			{
				std::cout << "Unhandled Internal Exception... " << "\n";
			}
		}
	}

	return 0;
}