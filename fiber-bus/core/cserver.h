#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <netinet/in.h>
#include "cpoll.h"

namespace core {
	class cserver {
	private:
		enum type { tsocket = 0, ttcp = 1, tudp };
	public:
		class base { 
			public: 
				base() = default; 
				virtual ~base() { ; } 
				virtual cserver::type type_id() const = 0; 
				virtual const std::string& getname() const = 0;
				template<typename T> T& get() { return *((T*)this); } 
		};
		class tcp : public base {
		public:
			tcp(const uint16_t& port, const std::string& ip = {}, const std::string& iface = {}, uint32_t stype = SOCK_STREAM,uint32_t proto = IPPROTO_TCP) noexcept
				: nPort(port), soType(stype), soProtocol(proto), sIp(ip), sIFace(iface) { ; }
			tcp(const tcp& s) noexcept
				: flagReuseAddress(s.flagReuseAddress), flagNoDelay(s.flagNoDelay), flagFastOpen(s.flagFastOpen), flagReusePort(s.flagReusePort), flagKeepAlive(s.flagKeepAlive),
				optKeepAliveIdle(s.optKeepAliveIdle), optKeepAliveInterval(s.optKeepAliveInterval), optKeepAliveCountAttempts(s.optKeepAliveCountAttempts), optMaxConnections(s.optMaxConnections),
				optReceiveTimeout(s.optReceiveTimeout),
				nPort(s.nPort), soType(s.soType), soProtocol(s.soProtocol), sIp(s.sIp), sIFace(s.sIFace){ ; }
			tcp& operator = (const tcp& s) = delete;
			virtual ~tcp() noexcept { ; }

			inline void set_max_conection(int num_connections) { optMaxConnections = num_connections; }
			inline void set_reuseaddress(bool enable = true) { flagReuseAddress = enable; }
			inline void set_reuseport(bool enable = true) { flagReusePort = enable; }
			inline void set_fastopen(bool enable = true) { flagFastOpen = enable; }
			inline void set_nodelay(bool enable = true) { flagNoDelay = enable; }
			inline void set_receivetimeout(int milliseconds) { optReceiveTimeout = milliseconds; }
			inline void set_keepalive(bool enable = true, int idle = 0, int interval = 0, int count_attempts = 0) { flagKeepAlive = enable; optKeepAliveIdle = idle; optKeepAliveInterval = interval; optKeepAliveCountAttempts = count_attempts; }

			virtual cserver::type type_id() const final { return cserver::ttcp; }
			virtual void onshutdown() const = 0; /* server gone away */
			virtual void onclose(int) const = 0; /* server gone away, client will be disconnected */
			virtual void onconnect(int, const sockaddr_storage&,uint64_t) const = 0; /* server accept new connection */
			virtual void ondisconnect(int) const = 0; /* client gone away */
			virtual void ondata(int) const = 0; /* new data came */
			virtual void onwrite(int) const = 0; /* ready to send */
			virtual void onidle(uint64_t time) const = 0; /* timeout idle */
		protected:
			friend class cserver;
			int flagReuseAddress{ 0 }, flagNoDelay{ 0 }, flagFastOpen{ 0 }, flagReusePort{ 0 },
				flagKeepAlive{ 0 }, optKeepAliveIdle = { 0 }, optKeepAliveInterval = { 0 }, optKeepAliveCountAttempts = { 0 }, optMaxConnections{ 0 }, optReceiveTimeout{ 0 };
			size_t			nPort{ 0 }, soType{ 0 }, soProtocol{ 0 };
			std::string		sIp;
			std::string		sIFace;
		};
		class udp : protected base {
		public:
			udp(const uint16_t& port, const std::string& ip = {}, const std::string& iface = {}, uint32_t stype = SOCK_DGRAM, uint32_t proto = IPPROTO_UDP) noexcept
				: nPort(port), soType(stype), soProtocol(proto), sIp(ip), sIFace(iface) { ; }
			udp(const udp& s) noexcept
				: flagReuseAddress(s.flagReuseAddress), flagReusePort(s.flagReusePort),
				nPort(s.nPort), soType(s.soType), soProtocol(s.soProtocol), sIp(s.sIp), sIFace(s.sIFace) { ; }
			udp& operator = (const udp& s) = delete;
			virtual ~udp() noexcept { ; }
			inline void set_reuseaddress(bool enable = true) { flagReuseAddress = enable; }
			inline void set_reuseport(bool enable = true) { flagReusePort = enable; }

			virtual cserver::type type_id() const final { return cserver::tudp; }
			virtual const std::string& getname() const = 0;
			virtual void onshutdown() const = 0; /* server gone away */
			virtual void ondata(int) const = 0; /* new data came */
			virtual void onwrite(int) const = 0; /* ready to send */
			
		protected:
			friend class cserver;
			int flagReuseAddress{ 0 }, flagReusePort{ 0 };
			size_t			nPort{ 0 }, soType{ 0 }, soProtocol{ 0 };
			std::string		sIp;
			std::string		sIFace;
		};
		cserver(size_t max_num_events = 1000) noexcept : eventPoll(), eventList(max_num_events) { ; }
		virtual ~cserver() noexcept { shutdown(); }

		inline void emplace(cserver::base* server) throw() {
			if (server->type_id() == cserver::ttcp) {
				emplace_tcp(std::shared_ptr<cserver::base>((cserver::base*)server));
			}
			else if (server->type_id() == cserver::tudp) {
				emplace_udp(std::shared_ptr<cserver::base>((cserver::base*)server));
			}
			else {
				delete server;
				throw std::invalid_argument("server type");
			}
		}
		
		ssize_t listen(ssize_t timeout_milliseconds = -1);
		void shutdown();
	protected:
		uint64_t gettime();
		std::unordered_map<int, std::pair<uint8_t,std::shared_ptr<cserver::base>>>	socketPool;
		//std::unordered_map<int,std::unordered_set<int>>			clientPool;
		cepoll													eventPoll;
		cepoll::events											eventList;
		void emplace_tcp(std::shared_ptr<cserver::base>&& server) throw();
		void emplace_udp(std::shared_ptr<cserver::base>&& serve) throw();
	};
}