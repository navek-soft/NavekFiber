#pragma once
#include <string>
#include <set>
#include <unordered_map>
#include <deque>

namespace core {
	class coption {
		mutable bool		opt_empty{true};
		mutable std::string	opt_value;
	public:
		coption() { ; }
		coption(const std::string& def_value) : opt_value(def_value) { ; }
		coption(const coption& o) : opt_empty(o.opt_empty), opt_value(o.opt_value) { ; }

		inline void set(const std::string& v) const { opt_value = v; opt_empty = false; }
		inline bool isset() const { return !opt_empty; }
		inline void unset() const { opt_empty = true; opt_value.erase(); }
		inline const std::string& get() const { return opt_value; }


		/*
		* Convert time string aka [<H>h [<M>m ]] <S>[s] to seconds
		*/
		std::size_t seconds() const;

		/*
		* Convert value to integer value
		*/
		ssize_t number(std::size_t base = 10) const;

		/*
		* Convert value to float value
		*/
		double decimal(ssize_t precision = -1) const;

		/*
		* Convert string to number of byte (<N>[G|M|K|B])
		*/
		std::size_t bytes() const;

		/*
		* Expand string to incremented sequense.
		* 1-4,8 expanded to 1,2,3,4,8
		* at9-at3, et3-et5 expanded to at3,at4,at5,at6,at7,at8,at9,et3,et4,et5
		*/
		std::set<std::string> sequence(const std::string& delimiter_chain = ",", const std::string& delimiter_range = "-", bool trim_values = true) const;

		/*
		* Split string
		*/
		std::deque<std::string> split(const std::string& delimiter, bool trim_values = true) const;

		/*
		* host port (<ip4,ip6,name>:<port>)
		*/
		using hostport = struct {
			std::string host, port;
		};
		hostport host() const;

		/*
		* Expand dsn string aka <proto>://<user>:<pwd>@<host>:<port></path/to/><filename>?<opt1>=<val1>&<opt2>=<val2>
		*/

		using dsn_params = struct {
			std::string dsn,proto,user,pwd,host,port,path,filename,fullpathname;
			std::unordered_map<std::string, std::string> options;
		};
			//std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::unordered_map<std::string, std::string>>;

		dsn_params dsn() const;
	};


	class coptions {
	public:
		using list = std::unordered_map<std::string, coption>;

		coptions() = default;
		coptions(const std::deque<std::pair<std::string, std::string>>& opts) { for (auto&& o : opts) { options.emplace(o.first, coption(o.second)); } }
		coptions(const coptions::list& opts) : options(opts) { ; }
		explicit coptions(const coptions& c) : options(c.options) { ; }
		inline coptions& emplace(const std::string& name, const std::string& opt) { options.emplace(name, coption(opt)); return *this; }
		inline coptions& emplace(const std::string& name, const coption& opt) { options.emplace(name, opt); return *this; }

		inline core::coption at(const std::string& name, const std::string& defalut_value) const {
			auto&& it = options.find(name);
			return it != options.end() ? core::coption(it->second) : core::coption(defalut_value);
		}
		inline core::coption at(const std::string& name) const {
			return core::coption(options.at(name));
		}
	private:
		coptions::list options;
	};
}
