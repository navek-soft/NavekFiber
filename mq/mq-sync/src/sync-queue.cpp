#include "sync-queue.h"
#include "trace.h"
#include <utils/option.h>
#include <json/writer.hpp>


using namespace Dom;
using namespace std;

SyncQueueHandler::SyncQueueHandler() {
	dbg_trace("");
}
SyncQueueHandler::~SyncQueueHandler() {
	dbg_trace("");
}

inline void SyncQueueHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) const {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		std::string queueLength = std::to_string(mq_queue.size());
		headers.emplace("X-MQ-CLASS", "Sync.MQ.Queue");
		headers.emplace("X-MQ-QUEUE", queueLength.c_str());
		headers.emplace("Content-Type", ContentType.c_str());
		request->Reply(Code, Message, Body.data(), Body.length(), headers, true);
	}
}

bool SyncQueueHandler::Initialize(IUnknown* pIKernel, IUnknown* pIConfig) {

	if (pIKernel && pIKernel->QueryInterface(mq_kernel.guid(), mq_kernel)) {
		pIKernel->AddRef();
		if (pIConfig && pIConfig->QueryInterface(mq_config.guid(), mq_config)) {
			pIConfig->AddRef();
			mq_options.LimitPeriodSec = utils::option::time_period(mq_config->GetConfigValue("queue-limit-request", "0s"));
			try {
				mq_options.LimitSize = std::stol(mq_config->GetConfigValue("queue-limit-size", "0"));
			}
			catch (...) {
				log_print("<Runtime.Warning>. Invalid `queue-limit-size` option value. Default: 0");
				mq_options.LimitSize = 0;
			}
			try {
				mq_options.LimitTasks = std::stol(mq_config->GetConfigValue("queue-limit-tasks", "0"));
			}
			catch (...) {
				log_print("<Runtime.Warning>. Invalid `queue-limit-size` option value. Default: 0");
				mq_options.LimitTasks = 0;
			}
		}
		return true;
	}
	return false;
}

const IMQ::Info& SyncQueueHandler::GetInfo() const {
	static constexpr IMQ::Info module_info = { "Sync.MQ.Queue","0.0.1" };
	return module_info;
}

void SyncQueueHandler::Finalize(IUnknown*) {
}

void SyncQueueHandler::Get(IUnknown* unknown) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		unique_lock<std::mutex> lock(mq_sync);
		if (!mq_queue.empty()) {
			auto muid = mq_queue.front();
			mq_queue.pop();
			mq_locked.emplace(muid);

			auto payload = move(mq_messages.at(muid)->GetContent());
			request->Reply(200, "OK", payload->GetData(),payload->GetLength());
		}
		else {
			ReplyWithCode(unknown, 204, "Empty", "");
		}
	}
	else {
		throw std::runtime_error("IRequest not implemented");
	}
}

void SyncQueueHandler::Post(IUnknown* unknown) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		auto&& x_mid = request->GetHeaderOption("X-MQ-MESSAGE-REPLY-ID", nullptr);
		if (!x_mid->Empty()) {
			unique_lock<std::mutex> lock(mq_sync);
			try {
				uint64_t xid = std::stoul(std::string(x_mid->GetData(), x_mid->GetLength()));

				auto&& client = mq_messages.find(xid);

				if (client != mq_messages.end()) {
					auto&& content = request->GetContent();

					auto oContentType = request->GetHeaderOption("Content-Type", "text/json");
					;

					if (client->second->Reply(200, "OK", content->GetData(), content->GetLength(), {
						{"Content-Type",std::string(oContentType->GetData(), oContentType->GetLength()).c_str()},
						{"X-MQ-MESSAGE-REPLY-ID",std::to_string(request->GetMsgId()).c_str() },
						{"X-MQ-CLASS", "Sync.MQ.Queue"},
					})) {
						ReplyWithCode(unknown, 200, "OK", "");
						mq_messages.erase(xid);
						mq_locked.erase(xid);
					}
					else {
						ReplyWithCode(unknown, 499, "Client Closed Request", "");
					}
				}
				else {
					ReplyWithCode(unknown, 300, "Multiple Replays. Reply Ignored.", "");
				}
			}
			catch (std::exception& ex) {
				ReplyWithCode(unknown, 400, "Bad Request. Invalid `X-MQ-MESSAGE-REPLY-ID` header", "");
			}
		}
		else {
			ReplyWithCode(unknown, 400, "Bad Request. Header `X-MQ-MESSAGE-REPLY-ID` not found", "");
		}
	}
	else {
		throw std::runtime_error("IRequest not implemented");
	}
}

void SyncQueueHandler::Put(IUnknown* unknown) {

	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		unique_lock<mutex> lock(mq_sync);
		if (!mq_options.LimitSize || mq_queue.size() < mq_options.LimitSize) {
			mq_queue.emplace(request->GetMsgId());
			if (mq_messages.emplace(request->GetMsgId(), move(request)).second) {
				return;
			}
		}
		auto&& slimit = std::to_string(mq_options.LimitSize);
		ReplyWithCode(unknown, 507, "Insufficient Storage", "", { {"X-MQ-LIMIT",slimit.c_str()} });
	}
	else {
		throw std::runtime_error("IRequest not implemented");
	}
}

void SyncQueueHandler::Head(IUnknown* unknown) {
	uint64_t mid = 0;
	{
		unique_lock<std::mutex> lock(mq_sync);
		mid = mq_queue.empty() ? 0 : mq_queue.front();
	}
	ReplyWithCode(unknown, 200, "OK", json::string(json::object({ {"muid",to_string(mid)} })));
}

void SyncQueueHandler::Options(IUnknown* unknown) {
	ReplyWithCode(unknown, 200, "OK",
		"<GET> - Get message from queue head\n"\
		"<PUT> - Emplace message in queue tail.\n"\
		"<POST> - Push message reply with specific ID (X-MQ-MESSAGE-ID header option must be specified)\n"\
		"<HEAD> - Check queue status. Reply header option contain X-MQ-QUEUE-LENGTH - number of queue messages, X-MQ-QUEUE-LOCKED - number of locked (in progress) messages\n"\
		"<OPTIONS> - Supported metods and this help", {}, "text/plain");
}

void SyncQueueHandler::Delete(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method DELETE Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncQueueHandler::Trace(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method TRACE Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncQueueHandler::Connect(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method CONNECT Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncQueueHandler::Patch(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method PATCH Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

