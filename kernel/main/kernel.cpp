#include "kernel.h"
#include <trace.h>
using namespace std;
using namespace Fiber;

bool Kernel::CreateMQ(const clsuid& cid, void ** ppv) {
	return CreateInstance(cid, ppv, "MQ");
}

bool Kernel::CreateSAPI(const clsuid& cid, void ** ppv) {
	return CreateInstance(cid, ppv, "SAPI");
}


bool Kernel::Write(const char* Key, const char* Name, const char* Value) {
	log_print("<Serialize> %s.%s = %s", Key, Name, Value);
	return true;
}

bool Kernel::Read(const char* Key, char** Id, char** Value) {
	/* alloc use for Id,Value */
	return false;
}