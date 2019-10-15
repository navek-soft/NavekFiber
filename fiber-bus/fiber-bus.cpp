#include <cstdio>
#include "core/cserver.h"
#include "core/cexcept.h"
#include "server/chttpserver.h"

using namespace core;

int main()
{
	{
		cserver srv;
		coptions::list options;
		options.emplace("port", "8080");
		try {
			srv.emplace(new ibus::chttpserver(coptions(options)));

			while (srv.listen(-1) >= 0) {
				;
			}
			srv.shutdown();
		}
		catch (system_error & er) {
			fprintf(stderr, "[ core::system_error ] %s\n", er.what());
		}
	}
    return 0;
}