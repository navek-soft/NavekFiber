#pragma once
#include "coption.h"
#include <getopt.h>
#include <vector>

namespace core {
	class ccmdline {
	public:
		class arg {
			friend class ccmdline;
			std::string long_name;
			char		short_name;
			int			require_arg;
			core::coption& value_arg;
			operator struct ::option() const { return { long_name.c_str(),require_arg ,nullptr,short_name }; }
			operator core::coption& () const { return value_arg; }
		public:
			arg(core::coption& value, const std::string& lname, const char sname = '\0', int require = 0)
				: long_name(lname), short_name(sname), require_arg(require), value_arg(value) {
				;
			}
		};
	public:
		static void options(int argc, char* argv[], std::vector<core::ccmdline::arg>&& args, std::string& progname, std::string& pwd, std::string& cwd);
		static inline void options(int argc, char* argv[], std::vector<core::ccmdline::arg>&& args, std::string& pwd, std::string& cwd) {
			std::string progname;
			options(argc, argv, std::move(args), pwd, cwd, progname);
		}
		static inline void options(int argc, char* argv[], std::vector<core::ccmdline::arg>&& args, std::string& pwd) {
			std::string progname, cwd;
			options(argc, argv, std::move(args), pwd, cwd, progname);
		}
		
		static inline void options(int argc, char* argv[], std::vector<core::ccmdline::arg>&& args) {
			std::string progname, cwd, pwd;
			options(argc, argv, std::move(args), pwd, cwd, progname);
		}
	};
}