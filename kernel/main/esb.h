#pragma once
#include "interface/IConfig.h"
#include <string>
#include <unordered_map>
#include <list>

namespace Core {
	using namespace std;
	using namespace Dom;
	class ESB : public Dom::Client::Manager<IConfig> {
	private:
		string programDir;
		string programWorkDir;
		string programName;
	private:
		void LoadConfig(const std::string&& configFileName, unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& config);
	public:
		ESB();
		~ESB();
		void Run(int argc, char* argv[]);
		virtual const char* GetConfigValue(const char* propname, const char* propdefault = "");
		virtual const char* GetProgramName();
		virtual const char* GetProgramDir();
		virtual const char* GetProgramCurrentDir();
	};
}