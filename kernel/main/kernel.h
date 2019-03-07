#pragma once
#include "interface/IConfig.h"
#include <string>
#include <unordered_map>
#include <list>
#include <csignal>

namespace Fiber {
	using namespace std;
	using namespace Dom;
	class Kernel : public Dom::Client::Manager<IConfig> {
	private:
		string programDir;
		string programWorkDir;
		string programName;
		static sig_atomic_t procSignal;
	private:
		void LoadConfig(const std::string&& configFileName, unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& config);
	public:
		Kernel();
		~Kernel();
		void Run(int argc, char* argv[]);
		virtual const char* GetConfigValue(const char* propname, const char* propdefault = "");
		virtual const char* GetProgramName();
		virtual const char* GetProgramDir();
		virtual const char* GetProgramCurrentDir();
	};
}