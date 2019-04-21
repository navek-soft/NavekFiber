#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <csignal>
#include <memory>
#include "impl/cfgimpl.h"
#include "interface/IKernel.h"
#include "interface/ISerialize.h"

namespace Fiber {
	using namespace std;
	using namespace Dom;

	using cmqmodules = std::unordered_map < std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>;
	using csapimodules = std::unordered_map < std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>;

	class Kernel : public Dom::Client::Manager<IKernel, ISerialize> {
	private:
		string programDir;
		string programWorkDir;
		string programName;
		static sig_atomic_t procSignal;

		cmqmodules				fiberMQModules;
		csapimodules			fiberSAPIModules;
	private:
		void initConfig(const std::string&& configFileName, unordered_map<string, pair<list<string>, unordered_map<string, string>>>&& config);
	public:
		Kernel();
		~Kernel();
		void Run(int argc, char* argv[]);

		virtual bool CreateMQ(const clsuid& cid, void ** ppv);
		virtual bool CreateSAPI(const clsuid& cid, void ** ppv);

		virtual bool Write(const char* Key, const char* Id, const char* Value);
		virtual bool Read(const char* Key, char** Id, char** Value);
	};
}