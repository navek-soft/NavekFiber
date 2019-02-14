#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <sys/socket.h>
#include <poll.h>

namespace Fiber {
	using namespace std;
	
	using msgid = uint_fast64_t;

	class Server {
	public:
		class Handler {
		protected:
			Handler() { ; }
			virtual ~Handler() { ; }
		};
	private:
		using callback = function<shared_ptr<Handler>(int, sockaddr_storage&,socklen_t)>;
		atomic_uint_fast32_t			msgIndex;
		unordered_map<int, callback>	conHandlers;
		vector<struct pollfd>			conFd;
	public:
		Server(const unordered_map<string, string>& options);
		~Server();
		bool AddListener(const string& proto, const string& listen, const unordered_map<string, string>& options);

		bool Listen(size_t timeout_msec = 5000);
	};
}