#pragma once
#include <cinttypes>
#include <string>

namespace Kernel {

	namespace Server {
		using namespace std;

		class Pipeline {
		public:
			Pipeline(const string& listen);
			~Pipeline();
		};
	}
}