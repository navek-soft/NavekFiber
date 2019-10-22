#include <cstdio>
#include "server/capp.h"
#include "ci/cstring.h"

int main(int argc,char* argv[])
{
	return fiber::capp::run(argc, argv);
	return 0;

}