#pragma once
#include <cinttypes>

namespace Fiber {
	namespace Proto {
		static constexpr uint8_t* RequestMethods[] = { (uint8_t*)"GET ", (uint8_t*)"POST ", (uint8_t*)"PUT ", (uint8_t*)"HEAD ", (uint8_t*)"OPTIONS ", (uint8_t*)"DELETE ", (uint8_t*)"TRACE ", (uint8_t*)"CONNECT ", (uint8_t*)"PATCH " };
		enum RequestMethod {UNSUPPORTED,GET,POST,PUT,HEAD,OPTIONS,DELETE,TRACE,CONNECT,PATCH};
	}
}