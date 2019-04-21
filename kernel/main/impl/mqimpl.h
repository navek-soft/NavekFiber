#pragma once
#include <string>
#include <unordered_map>


namespace Fiber {
	using namespace std;
	class MQImpl : Dom::Client::Embedded<MQImpl,IMQ> {
	public:
		MQImpl(IConfig* config,IKernel* ) { ; }
		void Process(const std::string& path, Dom::Interface<IMQ>&&)
	};
}

