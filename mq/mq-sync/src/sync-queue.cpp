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

inline bool SyncQueueHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, const zcstring& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		headers.emplace("X-MQ-CLASS", "Sync.MQ.Queue");
		headers.emplace("X-MQ-QUEUE-SIZE", std::to_string(mq_queue.size()).c_str());
		headers.emplace("X-MQ-PENDING-SIZE", std::to_string(mq_locked.size()).c_str());
		if(!ContentType.empty()) headers.emplace("Content-Type", ContentType.c_str());
		return request->Reply(Code, Message, Body.c_str(), Body.length(), headers, true);
	}
	return false;
}

inline bool SyncQueueHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) const {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		std::string queueLength = std::to_string(mq_queue.size());
		headers.emplace("X-MQ-CLASS", "Sync.MQ.Queue");
		headers.emplace("X-MQ-QUEUE-SIZE", std::to_string(mq_queue.size()).c_str());
		headers.emplace("X-MQ-PENDING-SIZE", std::to_string(mq_locked.size()).c_str());
		if (!ContentType.empty()) headers.emplace("Content-Type", ContentType.c_str());
		return request->Reply(Code, Message, Body.data(), Body.length(), headers, true);
	}
	return false;
}

bool SyncQueueHandler::Initialize(IUnknown* pIKernel, IUnknown* pIConfig) {

	if (mq_kernel.QueryInterface(pIKernel)) {
		if (mq_config.QueryInterface(pIConfig)) {
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
		uint64_t muid = 0;
		Dom::Interface<IRequest> xrequest;
		{
			unique_lock<std::mutex> lock(mq_sync);
			if (!mq_queue.empty()) {
				muid = mq_queue.front();
				mq_queue.pop();
				mq_locked.emplace(muid);
				xrequest.QueryInterface(mq_messages.at(muid));
			}
		}
		if (xrequest) {
			ReplyWithCode(unknown, 200, "OK", xrequest->GetContent(), { {"X-MSG-FORWARD-UID",std::to_string(muid).c_str()} }, xrequest->GetHeaderOption("Content-Type", "application/json").str());
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
		auto&& x_mid = request->GetHeaderOption("X-MSG-FORWARD-UID", nullptr);
		if (!x_mid.empty()) {
			Dom::Interface<IRequest> xrequest;
			try {
				uint64_t xid = std::stoul(x_mid.str());
				{
					unique_lock<std::mutex> lock(mq_sync);
					auto&& client = mq_messages.find(xid);
					if (client != mq_messages.end()) {
						xrequest.QueryInterface(client->second);
						mq_messages.erase(xid);
						mq_locked.erase(xid);
					}
				}
				if (xrequest) {
					if (ReplyWithCode(xrequest, 200, "OK", request->GetContent(), { {"X-MSG-FORWARD-UID",std::to_string(request->GetMsgId()).c_str()} }, request->GetHeaderOption("Content-Type", "application/json").str())) {
						ReplyWithCode(unknown, 200, "OK", "");
					}
					else {
						ReplyWithCode(unknown, 499, "Unable to sent client response", "");
					}
				}
				else {
					ReplyWithCode(unknown, 300, "Multiple Replays. Reply Ignored.", "");
				}
			}
			catch (std::exception& ex) {
				ReplyWithCode(unknown, 400, "Bad Request. Invalid `X-MSG-FORWARD-UID` header", "");
			}
		}
		else {
			ReplyWithCode(unknown, 400, "Bad Request. Header `X-MSG-FORWARD-UID` not found", "");
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
	ReplyWithCode(unknown, 200, "OK", "", { {"X-MSG-HEAD-UID",std::to_string(mid).c_str()} });
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

