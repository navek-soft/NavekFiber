#pragma once
#include <cinttypes>
#include <string>

namespace Core {
	using namespace std;

	class Server {
	public:
		Server(const string& port, const string& eth = "*");
		~Server();
	};
}