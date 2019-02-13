#pragma once
#include <interface/IConfig.h>
#include <unordered_map>
#include <string>
#include <list>

namespace Core {
	using namespace std;
	class ConfigImpl : public Dom::Client::Embedded<ConfigImpl,IConfig> {
		const unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& configValues;
		const string &&progDir, &&progWorkDir, &&progName;
		string space;
	public:
		ConfigImpl(const unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& Values, const string &&Dir, const string&&WorkDir, const string &&Name,const string&& ns = string())
			: configValues(move(Values)), progDir(move(Dir)), progWorkDir(move(WorkDir)), progName(move(Name)), space(ns) {
			;
		}
		ConfigImpl(const ConfigImpl&) = delete;
		const ConfigImpl& operator = (const ConfigImpl&) = delete;
		const ConfigImpl& operator = (const ConfigImpl&&) = delete;

		ConfigImpl(const ConfigImpl&& c) : configValues(move(c.configValues)), progDir(move(c.progDir)), progWorkDir(move(c.progWorkDir)), progName(move(c.progName)), space(c.space) {
			;
		}

		inline const list<string> GetSections() const { 
			auto&& it = configValues.find(space);  
			return it != configValues.cend() ? it->second.first : list<string>(); 
		}

		inline ConfigImpl operator[](const char* ns) const { 
			return ConfigImpl(move(configValues), move(progDir), move(progWorkDir), move(progName), move(space.empty() ? ns : (space + "." + ns)));
		}
		inline operator bool() { return configValues.find(space) != configValues.cend(); }

		inline virtual const char* GetConfigValue(const char* propname, const char* propdefault = "") { 
			size_t prop_vpart = string(propname).rfind('.');
			string path(space), value;

			if (prop_vpart != string::npos) {
				if (!path.empty()) path.append(".");
				path.append(propname, 0, prop_vpart);
				value.append(propname + prop_vpart + 1);
			}
			else {
				value = propname;
			}

			auto&& pit = configValues.find(path);

			if (pit != configValues.end()) {
				auto&& vit = pit->second.second.find(value);
				return vit->second.c_str();
			}

			return propdefault;
		}
		inline virtual const char* GetProgramName() { return progName.c_str(); }
		inline virtual const char* GetProgramDir() { return progDir.c_str(); }
		inline virtual const char* GetProgramCurrentDir() { return progWorkDir.c_str(); }
	};
}