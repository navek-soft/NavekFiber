#pragma once
#include "../../core/coption.h"
#include "../crequest.h"
#include "../../sapi/csapi.h"
#include <ctime>


namespace fiber {
	class cchannel {
	protected:

		ssize_t external_post(const std::string& url, const crequest::payload& data, size_t data_length, const crequest::response_headers& headers);

		crequest::response_headers make_headers(const cmsgid& msg_id, std::time_t ctime, std::time_t mtime,uint16_t status, const std::string name, const crequest::response_headers& additional = {}, const crequest::headers& reqeaders = {});

		inline void dispatch(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
			switch (request->request_type()) {
			case crequest::get: OnGET(std::move(msg_id), std::move(path), request); break;
			case crequest::post: OnPOST(std::move(msg_id), std::move(path), request); break;
			case crequest::put: OnPUT(std::move(msg_id), std::move(path), request); break;
			case crequest::head: OnHEAD(std::move(msg_id), std::move(path), request); break;
			case crequest::options: OnOPTINOS(std::move(msg_id), std::move(path), request); break;
			case crequest::del: OnDELETE(std::move(msg_id), std::move(path), request); break;
			case crequest::trace: OnTRACE(std::move(msg_id), std::move(path), request); break;
			case crequest::connect: OnCONNECT(std::move(msg_id), std::move(path), request); break;
			case crequest::patch: OnPATCH(std::move(msg_id), std::move(path), request); break;
			default:
				request->response({}, 405, {}); request->disconnect();
				break;
			}
		}

		virtual void OnGET(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnOPTINOS(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnDELETE(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnTRACE(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnCONNECT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
		virtual void OnPATCH(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { request->response({}, 405, {}); request->disconnect(); }
	public:
		virtual ~cchannel() { ; }
		virtual void processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg) = 0;
	};

	
	/*
	class cqueue_sync : public cqueue {
		cqueue_sync(const ::core::coptions& options) {}
		virtual ~cqueue_sync() {}

		virtual void enqueue(const cmsgid& msg_id, std::shared_ptr<fiber::crequest>& msg) { printf("cqueue_sync\n"); }
	};

	class cqueue_async_durability : public cqueue {
		cqueue_async_durability(const core::coptions& options) {}
		virtual ~cqueue_async_durability() {}

		virtual void enqueue(const cmsgid& msg_id, std::shared_ptr<fiber::crequest>& msg) { printf("cqueue_async_durability\n"); }
	};

	class cqueue_sync_durability : public cqueue {
		cqueue_sync_durability(const core::coptions& options);
		virtual ~cqueue_sync_durability() {}

		virtual void enqueue(const cmsgid& msg_id, std::shared_ptr<fiber::crequest>& msg) { printf("cqueue_sync_durability\n"); }
	};
	*/
}