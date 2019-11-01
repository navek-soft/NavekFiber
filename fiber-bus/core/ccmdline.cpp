#include "ccmdline.h"
#include <limits.h>
#include <stdlib.h>

void core::ccmdline::options(int argc, char* argv[], std::vector<core::ccmdline::arg>&& args, std::string& pwd, std::string& cwd, std::string& program) {
	std::vector<struct ::option> options_long(args.size() + 1);
	std::string options_short;

	{
		std::string exec_proc{ argv[0] };
		cwd = std::getenv("PWD");
		if (cwd.back() != '/') { cwd.push_back('/'); }

		if (auto&& pos = exec_proc.rfind('/'); pos != std::string::npos) {
			pwd.assign(exec_proc.begin(), exec_proc.begin() + pos + 1);
			program.assign(exec_proc.begin() + pos + 1, exec_proc.end());
		}
		else {
			pwd = cwd;
			program = argv[0];
		}
		{
			char real_path[PATH_MAX];
			pwd.assign(realpath(pwd.c_str(), real_path));
			if (pwd.back() != '/') { pwd.push_back('/'); }
		}
	}

	for (size_t i = 0; i < args.size(); i++) {
		options_long[i].flag = nullptr;
		options_long[i].has_arg = args[i].require_arg;
		options_long[i].name = args[i].long_name.c_str();
		options_long[i].val = (int)args[i].short_name;

		if (args[i].short_name) {
			options_short.push_back((char)args[i].short_name);
			switch (args[i].require_arg) {
			case 2:
				options_short.push_back(':');
			case 1:
				options_short.push_back(':');
				break;
			}
		}
	}
	{
		auto& last = options_long.back();
		last.flag = nullptr;
		last.has_arg = 0;
		last.name = 0;
		last.val = 0;
	}

	int opt = -1, option_index = -1;
	while ((opt = getopt_long_only(argc, argv, options_short.c_str(), options_long.data(), &option_index)) != -1) {
		if (opt == '?' || opt == ':') { continue; }
		if (opt == 0) {
			if (args[option_index].require_arg == 1 && optarg == nullptr) {
				throw std::string(args[option_index].long_name);
			}
			args[option_index].value_arg.set(optarg != nullptr ? optarg : args[option_index].value_arg.get());
		}
		else {
			for (auto&& a : args) {
				if (a.short_name == (char)opt) {
					if (a.require_arg == 1 && optarg == nullptr) {
						throw std::string(a.long_name);
					}
					a.value_arg.set(optarg != nullptr ? optarg : a.value_arg.get());
					break;
				}
			}
		}
	}
}