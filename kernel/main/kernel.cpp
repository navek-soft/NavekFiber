#include "kernel.h"

using namespace std;
using namespace Fiber;

bool Kernel::CreateMQ(const clsuid& cid, void ** ppv) {
	return CreateInstance(cid, ppv, "MQ");
}

bool Kernel::CreateSAPI(const clsuid& cid, void ** ppv) {
	return CreateInstance(cid, ppv, "SAPI");
}
