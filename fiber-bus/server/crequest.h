#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include <atomic>
#include "../ci/cstring.h"

namespace fiber {
	class cmsgid {
	private:
		static inline std::atomic_uint64_t id_gcounter{ 0 };
		mutable		uint64_t	id_sequence{ 0 };
		mutable		uint16_t	no_sequence{ 0 };
		char		id_uid[16]{'0','0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0'};
		inline const uint64_t* u64() const { return (uint64_t*)id_uid; }
	protected:
		cmsgid(const char* uid) : id_sequence(0), no_sequence(0) { 
			std::memcpy(id_uid, uid, sizeof(id_uid));
		}
	public:
		cmsgid() = default;
		cmsgid(const cmsgid& m) : id_sequence (m.id_sequence), no_sequence(m.no_sequence) { std::memcpy(id_uid,m.id_uid,sizeof(id_uid)); }
		cmsgid(const std::string& m) { ; }
		cmsgid(const ci::cstringview& m) { 
			if (m.length() >= 16) { 
				std::memcpy(id_uid, m.data(), sizeof(id_uid)); 
				auto&& s_id_seq = ci::cstringview::find_next('.', m);
				if (!s_id_seq.empty()) {
					id_sequence = ci::cstringview(s_id_seq.end() + 1, m.end() ).to_number();

					auto&& s_no_seq = ci::cstringview::find_next('.', ci::cstringview(s_id_seq.end() + 2, m.end()));

					if (!s_id_seq.empty()) {
						no_sequence = (uint16_t)ci::cstringview(s_no_seq.end() + 1, m.end()).to_number();
					}
				}
			}; 
		}
		inline bool epmty() { auto&& _u64 = u64(); return _u64[0] == _u64[1] && _u64[1] == 0; }
		inline bool operator == (const cmsgid& m) const { return std::memcmp(id_uid,m.id_uid,sizeof(id_uid)) == 0; }
		inline cmsgid& operator = (const cmsgid& m) { id_sequence = m.id_sequence; no_sequence = m.no_sequence; std::memcpy(id_uid, m.id_uid, sizeof(id_uid)); return *this; }
		inline size_t hash() const { uint64_t* i64_id = (uint64_t*)id_uid;	return 0x5bd1e9955bd1e995ull ^ i64_id[0] ^ i64_id[1];}
		static inline cmsgid gen() { 
			char buffer[17];
			std::snprintf(buffer,sizeof(buffer),"%08X%08X", 0x5bd1e995ull ^ ((uint32_t)time(nullptr)), ++id_gcounter);
			return buffer;
		}
		inline std::string str() const {
			char buffer[sizeof(id_uid) + 1 + 8 + 1 + 2 + 1];
			std::snprintf(buffer, sizeof(buffer), "%*s.%lu.%lu",sizeof(id_uid), (char*)id_uid, id_sequence, no_sequence);
			return buffer;
		}
	};

	class crequest {
		static const std::unordered_map<size_t, const char*> codeResponse;
	public:
		enum type { invalid, unsupported, get, post, put, head, options, del, trace, connect, patch };
		enum status : uint16_t { 
			accepted		= 0x0001,	// receive the new request
			enqueued	= 0x0002,		// add to local queue
			pushed		= 0x0004,		// delivered to sapi executor 
			execute		= 0x0008,		// sapi executing the request
			bad			= 0x0010,		// sapi say, request is it invalid
			error		= 0x0020,		// sapi say, request cannot be executed,
			again		= 0x0040,		// sapi say, request cannot be executed immediately, try again later or other node
			answering	= 0x0080,		// sapi sending answer to producer
			stored		= 0x0100,		// message stored in to device
			complete	= 0x0200,		// sapi say, message processed

		};
		using code = std::pair<size_t, const char*>;
		using headers = std::unordered_map<ci::cstringview, ci::cstringview>;
		using payload = std::deque<ci::cstringview>;
		using response_headers = std::unordered_map<std::string, std::string>;

		inline code message(size_t msg_code) {  auto&& msg = codeResponse.find(msg_code); return msg != codeResponse.end() ? code(msg_code, msg->second) : code(msg_code,"unknown response code"); }
		inline code message(size_t msg_code,const char* msg) { return code(msg_code, msg); }

		virtual ~crequest() { ; }
		virtual type request_type() = 0;
		virtual const ci::cstringview& request_uri() = 0;
		virtual const headers& request_headers() = 0;
		virtual const payload& request_paload() = 0;
		virtual ssize_t request_paload_length() = 0;

		virtual size_t response(const uint8_t* response_data, ssize_t response_length) = 0;
		virtual size_t response(const payload& data, size_t data_length, size_t msg_code, const std::string& msg_text = {}, const response_headers& headers_list = {}) = 0;
		virtual size_t response(const ci::cstringformat& data, size_t msg_code, const std::string& msg_text = {}, const response_headers& headers_list = {}) = 0;

		virtual void disconnect() = 0;
	};
}


namespace std {
	template<>
	class hash<fiber::cmsgid> {
	public:
		inline size_t operator()(const fiber::cmsgid& s) const { return s.hash(); }
	};
	template<>
	class equal_to<fiber::cmsgid> {
	public:
		inline bool operator()(const fiber::cmsgid& a, const fiber::cmsgid& b) const
		{
			return a == b;
		}
	};
}