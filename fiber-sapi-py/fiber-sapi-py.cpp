#include <cstdio>
#include "core/coption.h"
#include "core/cexcept.h"
#include "sapi/cfpm.h"
#include "py/csapi-python.h"

int main(int argc, char* argv[])
{
	core::coptions options;
	options.emplace("fpm-num-workers", "5").
		emplace("fpm-enable-resurrect", "1").
		emplace("fpm-pipe", "/tmp/navekfiber-fpm");

	try {
		fiber::cfpm::run(std::unique_ptr<fiber::csapi>(new fiber::csapi_python(options)), options);
	}
	catch (core::system_error & er) {
		return 3;
	}

	//core::cthreadpool esbThreadPool(options.at("threads-count", "1").number(), options.at("threads-max", "1").number());

    return 0;
}