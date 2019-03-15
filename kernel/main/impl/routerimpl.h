#pragma once
#include <string>
#include <unordered_map>
#include <dom.h>
#include <interface/IMQ.h>
#include <interface/IKernel.h>
#include <interface/IRequest.h>
#include "../../net/server.h"
#include "cfgimpl.h"

namespace Fiber {
	using namespace std;
	class RouterImpl {

		class cpathstring {
			char*	cfrom,*cto;
			bool	freea;
		public:
			cpathstring(const std::string& path) : cfrom(nullptr), cto(nullptr), freea(true) { 
				cto = cfrom = new char[path.length()]; cto += path.length(); std::strncpy(cfrom, path.data(), path.length());
			}

			cpathstring(const cpathstring& path) : cfrom(path.cfrom), cto(path.cto), freea(false) { ; }
			cpathstring& operator = (const cpathstring& path) { cfrom = path.cfrom; cto = path.cto; freea = false; }

			cpathstring(const char* from = nullptr, const char* to = nullptr) : cfrom((char*)from), cto((char*)to), freea(false) { ; }
			~cpathstring() { if (freea) { delete[] cfrom; } }

			inline size_t hash() const {
				size_t len = sizeof(size_t);
				size_t hash = 0;
				for (char* it = (cfrom && cfrom[0] == '/' ? (cfrom + 1) : cfrom); it < cto && *it!='/' && len--; it++) {
					hash = (hash | *it) << 8;
				}
				return hash;
			}

			inline bool compare(const cpathstring& with) const {
				for (const char* lit = cfrom, *rit = with.cfrom; lit < cto && rit < with.cto;) {
					if (*lit != *rit) {
						if (*lit == '?') {
							if (*rit != '/') { rit++; continue; }
							lit++; continue;
						}
						else if (*rit == '?') {
							if (*lit != '/') { lit++; continue; }
							rit++; continue;
						}
						if (*lit == '*' || *rit == '*') {
							return true;
						}
						return false;
					}
					lit++;
					rit++;
				}
				return true;
			}
		};

		class cpathstring_hash {
		public:
			inline size_t operator()(const cpathstring& key) const { return key.hash(); }
		};
		class cpathstring_equal {
		public:
			inline bool operator()(const cpathstring& lkey, const cpathstring& rkey) const {
				return lkey.compare(rkey);
			}
		};

		using RouteList = std::unordered_multimap<cpathstring, Dom::Interface<IMQ>, cpathstring_hash, cpathstring_equal>;
		RouteList	Routes;
	public:
		RouterImpl() { ; }
		~RouterImpl(){ 
			Routes.clear(); 
		}
		bool AddRoute(const std::string& chPath, const std::string& mqClass, Fiber::ConfigImpl* chConfig, Dom::Interface<IKernel>&& kernel);
		void Process(Server::CHandler* request);
	};
}

