#include "esb.h"
#include "trace.h"
#include <sys/cmdline.h>
#include <sys/path.h>
#include <sys/types.h>
#include <unistd.h>
#include "impl/cfgimpl.h"

using namespace std;
using namespace Core;


static inline sys::cmdline initCmdLine(int argc, char* argv[], string& programDir,string& programWorkDir,string& programName) {

	using coption = sys::cmdline::option;
	sys::cmdline cmd;

	string procPathName(argv[0]);
	programDir.assign(argv[0], procPathName.rfind('/') + 1);
	programName = procPathName.substr(procPathName.rfind('/') + 1);
	programWorkDir = sys::cwd();

	cmd.options(argc, argv,
		coption("config", 'c', 1, nullptr)
	);

	log_option("Program name","%s (PID: %ld)", programName.c_str(), getpid());
	log_option("Program directory", "%s", programDir.c_str(), getpid());
	log_option("Program work directory", "%s", programWorkDir.c_str());

	return cmd;
}

ESB::ESB() { ; }

ESB::~ESB() { ; }


void ESB::Run(int argc, char* argv[]) {
	
	/* Startup ESB */
	{
		try {
			using cconfig = unordered_map<string, pair<list<string>, unordered_map<string, string>>>;
			cconfig	config_data;
			/* Initialize ESB */
			{
				sys::cmdline&& cmd = initCmdLine(argc, argv, programDir, programWorkDir, programName);

				LoadConfig(cmd("config", "fiber.config"), move(config_data));

				ConfigImpl config(move(config_data), move(programDir), move(programWorkDir), move(programName));
				config.AddRef();

				auto&& server = config["fiber"]["server"];
				auto&& mq = config["fiber"]["mq"];
				auto&& sapi = config["fiber"]["sapi"];
				log_option("SAPI modules directory", "%s", sapi.GetConfigValue("modules_dir", "./"));
				for (auto it : sapi.GetSections()) {
					log_option(string("sapi." + it + ".class").c_str(),"%s",sapi.GetConfigValue(string(it+".class").c_str(),"*INVALID"));
				}
			}
		}
		catch (exception& ex) {
			log_print("[ %9s:%s ] %s","EXCEPTION","STARTUP",ex.what());
			throw;
		}
	}
}