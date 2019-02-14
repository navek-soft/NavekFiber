#include "kernel.h"
#include "trace.h"
#include <fstream>
#include <regex>
#include <unordered_map>
#include <list>

using namespace std;
using namespace Fiber;

static inline string implode_config_path(const list<string>&& path,const string&& valname=string()) {
	string result;
	if (!path.empty()) {
		auto&& it = path.begin();
		for (ssize_t n = path.size(); --n; it++) {
			result.append(*it).append(".");
		}
		result.append(*it);
		if (!valname.empty()) { result.append(".").append(valname); }
	}
	else {
		if (!valname.empty()) { result = valname; }
	}

	return result;
}

void Kernel::LoadConfig(const std::string&& configFileName, unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& config) {

	static const std::regex re_config_section(R"(^\s*(\[+)\s*(.*?)\s*\]+)");
	static const std::regex re_config_propety(R"(^\s*([\w\-:]+)\s*=\s*(?:("|'|)(.*?)\2)\s*$)");
	static const std::regex re_config_line_break_clean(R"(^\s*(.*?)\\\s*$)");
	static const std::regex re_config_space_clean(R"(^\s*(.*?)\s*$)");
	static const std::regex re_config_remove_comment(R"(\s*#[^#]*$)");

	using value = pair<list<string>, unordered_map<string, string>>;

	ifstream cfgfile(configFileName);
	if (cfgfile) {
		std::string content;
		std::smatch match;
		list<string> path;
		size_t line = 0;
		while (getline(cfgfile, content) && ++line) {
			if (std::regex_search(content, match, re_config_line_break_clean) && match.size() > 1) {
				string linebreak;
				content = match.str(1);
				while (getline(cfgfile, linebreak) && ++line && (std::regex_search(linebreak, match, re_config_line_break_clean) && match.size() > 1)) {
					content += std::regex_replace(match.str(1), re_config_remove_comment, "");
				}
				content += std::regex_replace(linebreak, re_config_space_clean, "$1");
			}
			if (std::regex_search(content, match, re_config_section) && match.size() > 1) {
				size_t clvl = match.str(1).length();
				if (clvl == path.size()) {
					path.pop_back();
					auto&& node = std::regex_replace(match.str(2), re_config_remove_comment, "");
					config.emplace(make_pair(implode_config_path(move(path), move(node)), value()));
					auto&& it = config.emplace(make_pair(implode_config_path(move(path)), value()));
					it.first->second.first.emplace_back(node);
					path.emplace_back(node);
				}
				else if (clvl < path.size()) {
					do {
						path.pop_back();
					} while (path.size() >= clvl);
					auto&& node = std::regex_replace(match.str(2), re_config_remove_comment, "");
					config.emplace(make_pair(implode_config_path(move(path), move(node)), value()));
					auto&& it = config.emplace(make_pair(implode_config_path(move(path)), value()));
					it.first->second.first.emplace_back(node);
					path.emplace_back(node);
				}
				else if (clvl > path.size()) {
					auto&& node = std::regex_replace(match.str(2), re_config_remove_comment, "");
					config.emplace(make_pair(implode_config_path(move(path),move(node)), value()));
					auto&& it = config.emplace(make_pair(implode_config_path(move(path)), value()));
					it.first->second.first.emplace_back(node);
					path.emplace_back(node);
				}
				continue;
			}
			if (std::regex_search(content, match, re_config_propety) && match.size() > 1) {
				auto&& it = config.find(implode_config_path(move(path)));
				it->second.second.emplace(match.str(1), std::regex_replace(match.str(3), re_config_remove_comment, ""));
				continue;
			}
		}
	}
	else {
		throw system_error(errno, std::system_category(), configFileName);
	}

}

const char* Kernel::GetConfigValue(const char* propname, const char* propdefault) {
	return nullptr;
}

const char* Kernel::GetProgramName() {
	return programName.c_str();
}

const char* Kernel::GetProgramDir() {
	return programDir.c_str();
}

const char* Kernel::GetProgramCurrentDir() {
	return programWorkDir.c_str();
}
