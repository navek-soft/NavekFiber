#include "coption.h"
#include <list>
#include <regex>
#include <cmath>

namespace core {
	namespace impl {
		static inline std::string trim_space(std::string::const_iterator it_from, std::string::const_iterator it_to) {
			const char* from = it_from.base(), * to = it_to.base();
			/* trim left */
			for (; std::isspace(*from) && from < to; from++) { ; }
			/* trim right */
			for (; to > from&& std::isspace(*(to - 1)); to--) { ; }
			return std::string(from, to);
		}

		static inline std::deque<std::string> explode_string(const std::string& string, const std::string& delimiter, bool trim) {
			/* explode chains */
			std::deque<std::string> list;
			std::size_t start = 0;
			std::size_t end = string.find(delimiter);
			if (!string.empty()) {
				while (end != std::string::npos)
				{
					if (trim) {
						list.emplace_back(trim_space(string.begin() + start, string.begin() + end));
					}
					else {
						list.emplace_back(string.begin() + start, string.begin() + end);
					}
					start = end + delimiter.length();
					end = string.find(delimiter, start);
				}
				if (trim) {
					/* trim left */
					list.emplace_back(trim_space(string.begin() + start, string.begin() + string.length()));
				}
				else {
					list.emplace_back(string.begin() + start, string.end());
				}
			}
			return list;
		}

		static inline std::list<std::string> gen_sequence(const std::string& begin, const std::string& end) {
			/* rfind numeric sequence */
			std::list<std::string> seq;
			if (begin.empty()) {
				seq.emplace_back(end);
				return seq;
			}
			if (end.empty() || begin == end) {
				seq.emplace_back(begin);
				return seq;
			}

			if (!std::isdigit(begin.back()) || !std::isdigit(end.back())) {
				seq.emplace_back(begin);
				seq.emplace_back(end);
				return seq;
			}

			std::size_t seq_begin = 0, seq_end = 0;

			auto rit = begin.rbegin();
			for (std::size_t multipler = 1; rit < begin.rend() && std::isdigit(*rit); rit++, multipler *= 10) {
				seq_begin += multipler * ((*rit) - '0');
			}
			rit = end.rbegin();
			for (std::size_t multipler = 1; rit < end.rend() && std::isdigit(*rit); rit++, multipler *= 10) {
				seq_end += multipler * ((*rit) - '0');
			}
			std::string prefix;
			prefix.assign(end.begin(), end.begin() + (end.rend() - rit));
			std::string value;
			if (seq_end >= seq_begin) {
				for (; seq_begin <= seq_end; seq_begin++) {
					value = prefix;
					seq.emplace_back(value.append(std::to_string(seq_begin)));
				}
			}
			else {
				for (; seq_begin >= seq_end; seq_begin--) {
					value = prefix;
					seq.emplace_back(value.append(std::to_string(seq_begin)));
				}
			}
			return seq;
		}
	}
}

std::size_t core::coption::seconds() const {
	static const std::regex re(R"((?:\s*(\d+)\s*h)?(?:\s*(\d+)\s*m)?(?:\s*(\d+)\s*s{0,1})?)");
	std::smatch match;
	if (!opt_value.empty() && std::regex_search(opt_value, match, re) && match.size() > 1) {
		auto&& h = match.str(1);
		auto&& m = match.str(2);
		auto&& s = match.str(3);
		return (h.empty() ? 0 : std::stol(h) * 3600) + (m.empty() ? 0 : std::stol(m) * 60) + (s.empty() ? 0 : std::stol(s));
	}
	return 0;
}

ssize_t core::coption::number(std::size_t base) const {
	if (!opt_value.empty()) {
		return std::stoll(opt_value, 0, int(base));
	}
	return 0;
}

double core::coption::decimal(ssize_t precision) const {
	if (!opt_value.empty()) {
		if (precision == -1) {
			return std::stod(opt_value, nullptr);
		}
		else {
			auto div = std::pow(10., (double)precision);
			return ((double)std::lround(std::stod(opt_value, nullptr) * div + 0.5)) / (double)div;
		}
	}
	return 0.0f;
}


/*
* Convert string to number of byte (<N>[G|M|K|B])
*/
std::size_t core::coption::bytes() const {
	static const std::regex re(R"(\s*(\d+)\s*(G|M|K|B)?)");
	std::smatch match;
	if (!opt_value.empty() && std::regex_search(opt_value, match, re) && match.size() > 1) {
		auto&& units = match.str(2);
		return std::stoul(match.str(1)) << (units == "G" ? 30 : (units == "M" ? 20 : (units == "K" ? 10 : 0)));
	}
	return 0;
}

/*
* Expand string to incremented sequense.
* 1-4,8 expanded to 1,2,3,4,8
* at9-at3, et3-et5 expanded to at3,at4,at5,at6,at7,at8,at9,et3,et4,et5
*/
std::set<std::string> core::coption::sequence(const std::string& delimiter_chain, const std::string& delimiter_range, bool trim_values) const {
	std::set<std::string> result;
	if (!opt_value.empty()) {
		for (auto&& seq : core::impl::explode_string(opt_value, delimiter_chain, trim_values)) {
			auto rng = core::impl::explode_string(seq, delimiter_range, trim_values);
			if (rng.size() == 1) {
				result.emplace(rng.front());
			}
			else if (rng.size() > 1) {
				for (auto&& seq_range : core::impl::gen_sequence(rng.front(), rng.back())) {
					result.emplace(seq_range);
				}
			}
		}
	}
	return result;
}

std::deque<std::string> core::coption::split(const std::string& delimiter, bool trim_values) const {
	return core::impl::explode_string(opt_value, delimiter, trim_values);
}


/*
* Expand dsn string aka <proto>://<user>:<pwd>@<host>:<port></path/to/><filename>?<opt1>=<val1>&<opt2>=<val2>
*/
core::coption::dsn_params core::coption::dsn() const {

	/*
	std::string&& proto = {}, std::string&& user = {}, std::string&& password = {}, std::string&& host = {}, std::string&& port = {}, std::string&& path = {}, std::string&& filename = {},
	std::unordered_map<std::string, std::string>&& options = {}
	*/

	static const std::regex re_urn(R"((?:([\w-]+)://)?(?:([^:@]+)(?::([^@]+))?@)?([^:/$]+)(?::(\d+))?(?:$|(?:(/[^?$]*)(?:\?([^$]+))?$)))");
	static const std::regex re_opt(R"(([\w-]+)=([^$&]+))");

	std::string proto, user, pwd, host, port, path, filename, fullpathname;
	std::unordered_map<std::string, std::string> options;

	std::smatch match;
	if (std::regex_search(opt_value, match, re_urn) && match.size() > 1) {
		proto = match.str(1);
		if (!proto.empty() && proto != "file") {
			user = match.str(2);
			pwd = match.str(3);
			host = match.str(4);
			port = match.str(5);
			std::string pathname = match.str(6);
			path = pathname.substr(0, pathname.find_last_of('/') + 1);
			filename = pathname.substr(pathname.find_last_of('/') + 1);
			fullpathname = match.str(4) + match.str(6);
			{
				std::string opts(match.str(7));
				std::sregex_iterator next(opts.begin(), opts.end(), re_opt);
				std::sregex_iterator end;
				while (next != end) {
					std::smatch omatch = *next;
					options.emplace(omatch[1].str(), omatch[2].str());
					next++;
				}
			}
		}
		else {
			std::string pathname;
			if (match.prefix().compare("/") == 0) {
				pathname = "/";
			}
			proto = "file";
			pathname += match.str(4) + match.str(6);
			path = pathname.substr(0, pathname.find_last_of('/') + 1);
			filename = pathname.substr(pathname.find_last_of('/') + 1);
			fullpathname = pathname;

			{
				std::string opts(match.str(7));
				std::sregex_iterator next(opts.begin(), opts.end(), re_opt);
				std::sregex_iterator end;
				while (next != end) {
					std::smatch omatch = *next;
					options.emplace(omatch[1].str(), omatch[2].str());
					next++;
				}
			}
		}
	}
	return { opt_value, proto, user, pwd, host, port, path, filename,fullpathname,options };
}