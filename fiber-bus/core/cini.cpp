#include "cini.h"
#include <fstream>
#include <regex>

using namespace core;
using namespace std;

static const std::regex re_config_section(R"(^\s*(\[+)\s*(.*?)\s*\]+)");
static const std::regex re_config_propety(R"(^\s*([\w\-:]+)\s*=\s*(?:("|'|)(.*?)\2)\s*$)");
static const std::regex re_line_break_clean(R"(^\s*(.*?)\\\s*$)");
static const std::regex re_space_clean(R"(^\s*(.*?)\s*$)");
static const std::regex re_remove_comment(R"(\s*#[^#]*$)");

cini::cini() : head(nullptr) { ; }
cini& cini::operator = (cini&& cfg) { head.swap(cfg.head); return *this; }

inline cini::csection* cini::get_section(list<string>& path) {
	csection* result = head.get();
	for (auto&& name : path) {
		auto&& n = result->_sections.emplace(name, nullptr);
		if (n.second) {
			n.first->second = shared_ptr<csection>(new csection());
			result = n.first->second.get();
		}
		else {
			result = n.first->second.get();
		}
	}
	return result;
}

cini::cini(const string configfile) : head(new csection()) {
	ifstream cfgfile(configfile);
	if (cfgfile) {
		std::string content;
		std::smatch match;
		list<string> path;
		size_t line = 0;
		while (getline(cfgfile, content) && ++line) {
			if (std::regex_search(content, match, re_line_break_clean) && match.size() > 1) {
				string linebreak;
				content = match.str(1);
				while (getline(cfgfile, linebreak) && ++line && (std::regex_search(linebreak, match, re_line_break_clean) && match.size() > 1)) {
					content += std::regex_replace(match.str(1), re_remove_comment, "");
				}
				content += std::regex_replace(linebreak, re_space_clean, "$1");
			}
			if (std::regex_search(content, match, re_config_section) && match.size() > 1) {
				size_t clvl = match.str(1).length();
				if (clvl == path.size()) {
					path.pop_back();
					path.emplace_back(std::regex_replace(match.str(2), re_remove_comment, ""));
					get_section(path);
				}
				else if (clvl < path.size()) {
					do {
						path.pop_back();
					} while (path.size() >= clvl);
					path.emplace_back(std::regex_replace(match.str(2), re_remove_comment, ""));
					get_section(path);
				}
				else if (clvl > path.size()) {
					path.emplace_back(std::regex_replace(match.str(2), re_remove_comment, ""));
					get_section(path);
				}
				continue;
			}
			if (std::regex_search(content, match, re_config_propety) && match.size() > 1) {
				get_section(path)->_properties.emplace(match.str(1), std::regex_replace(match.str(3), re_remove_comment, ""));
				continue;
			}
		}
	}
	else {
		throw system_error(errno, std::system_category(), configfile);
	}
}
