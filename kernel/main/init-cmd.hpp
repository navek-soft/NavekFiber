#pragma once
#include <sys/cmdline.h>
#include <sys/path.h>
#include <string>
#include "trace.h"

using namespace std;

static inline sys::cmdline initCmdLine(int argc, char* argv[], string& programDir, string& programWorkDir, string& programName) {

	using coption = sys::cmdline::option;
	sys::cmdline cmd;

	string procPathName(argv[0]);
	programDir.assign(argv[0], procPathName.rfind('/') + 1);
	programName = procPathName.substr(procPathName.rfind('/') + 1);
	programWorkDir = sys::cwd();

	cmd.options(argc, argv,
		coption("config", 'c', 1, nullptr)
	);

	log_option("Program name", "%s (PID: %ld)", programName.c_str(), getpid());
	log_option("Program directory", "%s", programDir.c_str(), getpid());
	log_option("Program work directory", "%s", programWorkDir.c_str());

	return cmd;
}