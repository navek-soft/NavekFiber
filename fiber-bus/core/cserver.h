#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>
#include "cpoll.h"
#include <csignal>

namespace core {
	class cserver {
	public:
		enum type { tsocket = 0, ttcp = 1, tudp, tpipe };
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
			std::size_t			nPort{ 0 }, soType{ 0 }, soProtocol{ 0 };
			std::string		sIp;
			std::string		sIFace;
		};
		class udp : public base {
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
			std::size_t			nPort{ 0 }, soType{ 0 }, soProtocol{ 0 };
			std::string		sIp;
			std::string		sIFace;
		};
		class pipe : public base {
		public:
			pipe(const std::string& pipe_file, uint32_t stype = SOCK_STREAM) noexcept
				: soType(stype), sPipeFilename(pipe_file) {
				;
			}
			pipe(const pipe& s) noexcept
				: soType(s.soType), sPipeFilename(s.sPipeFilename) {
				;
			}
			pipe& operator = (const pipe& s) = delete;
			virtual ~pipe() noexcept { unlink(sPipeFilename.c_str()); }

			virtual cserver::type type_id() const final { return cserver::tpipe; }
			virtual const std::string& getname() const = 0;
			virtual void onshutdown() const = 0; /* server gone away */
			virtual void onclose(int) const = 0; /* server gone away, client will be disconnected */
			virtual void onconnect(int, const sockaddr_storage&, uint64_t) const = 0; /* server accept new connection */
			virtual void ondisconnect(int) const = 0; /* client gone away */
			virtual void ondata(int) const = 0; /* new data came */
			virtual void onwrite(int) const = 0; /* ready to send */
			virtual void onidle(uint64_t time) const = 0; /* timeout idle */
		protected:
			friend class cserver;
			std::size_t			soType{ 0 };
			std::string		sPipeFilename;
		};
		cserver(std::size_t max_num_events = 1000) noexcept : eventPoll(), eventList(max_num_events) { signal(SIGPIPE, SIG_IGN); }
		virtual ~cserver() noexcept { shutdown(); }

		inline void emplace(const std::shared_ptr<cserver::base>& server) noexcept(false) {
			if (server->type_id() == cserver::ttcp) {
				emplace_tcp(std::move(server));
			}
			else if (server->type_id() == cserver::tudp) {
				emplace_udp(std::move(server));
			}
			else if (server->type_id() == cserver::tpipe) {
				emplace_pipe(std::move(server));
			}
			else {
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
	private:
		using hserver = typename std::unordered_map<int, std::pair<uint8_t, std::shared_ptr<cserver::base>>>::iterator;
		void emplace_tcp(const std::shared_ptr<cserver::base>& server) noexcept(false);
		void emplace_udp(const std::shared_ptr<cserver::base>& server) noexcept(false);
		void emplace_pipe(const std::shared_ptr<cserver::base>& server) noexcept(false);

		inline void accept_tcp(uint32_t events, int sock,hserver&& server) noexcept(false);
		inline void accept_udp(uint32_t events, int sock, hserver&& server) noexcept(false);
		inline void accept_pipe(uint32_t events, int sock, hserver&& server) noexcept(false);
		inline void accept_tcpclient(uint32_t events, int sock, hserver&& server) noexcept(false);
		inline void accept_timer(uint32_t events, int sock, hserver&& server) noexcept(false);
	};
}