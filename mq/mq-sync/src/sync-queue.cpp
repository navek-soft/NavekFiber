#include "sync-queue.h"
#include "trace.h"
#include <utils/option.h>
#include <json/writer.hpp>
#include <interface/ISerialize.h>


using namespace Dom;
using namespace std;

SyncQueueHandler::SyncQueueHandler() {
	dbg_trace("");
}
SyncQueueHandler::~SyncQueueHandler() {
	Finalize(nullptr);
	dbg_trace("");
}

inline void SyncQueueHandler::UnSerialize() {
	Dom::Interface<ISerialize> serialize;
	if (serialize.QueryInterface((IUnknown*)mq_kernel)) {
		char* id, *value;
		size_t nCount = 0;
		while (serialize->Read(std::string(mq_name + ".locked").c_str(),&id,&value)) {
			free(id);
			free(value);
		}
		while (serialize->Read(std::string(mq_name + ".message").c_str(), &id, &value)) {
			nCount++;
			free(id);
			free(value);
		}
		if (nCount) {
			log_print("<Kernel.UnSerialize> %ld messages restore", nCount);
		}
		return;
	}
	dbg_trace("<Kernel.Warning> Serialization not support");
	
}

inline void SyncQueueHandler::Serialize() {
	Dom::Interface<ISerialize> serialize;
	if (serialize.QueryInterface(mq_kernel)) {
		{
			std::unique_lock<std::mutex> lock(mq_sync);
			if (!mq_locked.empty()) {
				for (auto&& mid : mq_locked) {
					serialize->Write(std::string(mq_name+".locked").c_str(),"id",std::to_string(mid).c_str());
				}
			}
			if (!mq_messages.empty()) {
				for (auto&& mid : mq_messages) {
					serialize->Write(std::string(mq_name + ".message").c_str(), "id", std::to_string(mid.first).c_str());
				}
			}
			if (!mq_messages.empty()) {
				log_print("<Kernel.Serialize> %ld messages store", mq_messages.size());
			}
		}
		return;
	}
	dbg_trace("<Kernel.Warning> Serialization not support");
}

inline bool SyncQueueHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, const zcstring& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		volatile size_t QueueCount = 0, LockedCount = 0, MessagesCount = 0;
		GetStats(QueueCount, LockedCount, MessagesCount);
		headers.emplace("X-MQ-CLASS", "Sync.MQ.Queue");
		headers.emplace("X-MQ-QUEUE-COUNT", std::to_string(QueueCount).c_str());
		headers.emplace("X-MQ-PENDING-COUNT", std::to_string(LockedCount).c_str());
		headers.emplace("X-MQ-MESSAGES-COUNT", std::to_string(MessagesCount).c_str());
		if(!ContentType.empty()) headers.emplace("Content-Type", ContentType.c_str());
		return request->Reply(Code, Message, Body.c_str(), Body.length(), headers, true);
	}
	return false;
}

inline bool SyncQueueHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		volatile size_t QueueCount = 0, LockedCount = 0, MessagesCount = 0;
		GetStats(QueueCount, LockedCount, MessagesCount);
		headers.emplace("X-MQ-CLASS", "Sync.MQ.Queue");
		headers.emplace("X-MQ-QUEUE-COUNT", std::to_string(QueueCount).c_str());
		headers.emplace("X-MQ-PENDING-COUNT", std::to_string(LockedCount).c_str());
		headers.emplace("X-MQ-MESSAGES-COUNT", std::to_string(MessagesCount).c_str());
		if (!ContentType.empty()) headers.emplace("Content-Type", ContentType.c_str());
		return request->Reply(Code, Message, Body.data(), Body.length(), headers, true);
	}
	return false;
}

bool SyncQueueHandler::Initialize(const char* Name, IUnknown* pIKernel, IUnknown* pIConfig) {
	mq_name = Name;
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
		UnSerialize();
		return true;
	}
	return false;
}

const IMQ::Info& SyncQueueHandler::GetInfo() const {
	static constexpr IMQ::Info module_info = { "Sync.MQ.Queue","0.0.1" };
	return module_info;
}

void SyncQueueHandler::Finalize(IUnknown*) {
	Serialize();
}

void SyncQueueHandler::Get(IUnknown* unknown) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		uint64_t muid = 0;
		Interface<IRequest> xrequest;
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
			xrequest->SetTelemetry(IRequest::EmplaceAt);
			ReplyWithCode(unknown, 200, "OK", xrequest->GetContent(), 
				{ {"X-MSG-FORWARD-UID",std::to_string(muid).c_str()} }, 
				xrequest->GetHeaderOption("Content-Type", "application/json").str());
		}
		else {
			ReplyWithCode(unknown, 204, "Queue is empty", "");
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
			Interface<IRequest> xrequest;
			try {
				uint64_t xid = std::stoul(x_mid.str());
				{
					unique_lock<std::mutex> lock(mq_sync);
					auto&& client = mq_messages.find(xid);
					if (client != mq_messages.end()) {
						xrequest.QueryInterface(client->second);
					}
				}
				if (xrequest) {
					xrequest->SetTelemetry(IRequest::ExtractAt);
					if (ReplyWithCode(xrequest, 200, "OK", request->GetContent(), { {"X-MSG-FORWARD-UID",std::to_string(request->GetMsgId()).c_str()} }, request->GetHeaderOption("Content-Type", "application/json").str())) {
						{
							unique_lock<std::mutex> lock(mq_sync);
							mq_messages.erase(xid);
							mq_locked.erase(xid);
						}
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
		{
			unique_lock<mutex> lock(mq_sync);
			if (!mq_options.LimitSize || mq_queue.size() < mq_options.LimitSize) {
				mq_queue.emplace(request->GetMsgId());
				if (mq_messages.emplace(request->GetMsgId(), move(request)).second) {
					return;
				}
			}
		}
		ReplyWithCode(unknown, 507, "Insufficient Storage", "", { {"X-MQ-QUEUE-LIMIT",std::to_string(mq_options.LimitSize).c_str()} });
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
	if (mid) {
		ReplyWithCode(unknown, 200, "OK", "", { {"X-MSG-HEAD-UID",std::to_string(mid).c_str()} });
	}
	else {
		ReplyWithCode(unknown, 204, "Queue is empty","");
	}
}

void SyncQueueHandler::Options(IUnknown* unknown) {
	ReplyWithCode(unknown, 200, "OK",
		"<GET> - Get message from queue head\n"\
		"<PUT> - Emplace message in queue tail.\n"\
		"<POST> - Push message reply with specific ID (X-MQ-MESSAGE-ID header option must be specified)\n"\
		"<HEAD> - Check queue status. Reply header option contain X-MQ-QUEUE-SIZE - number of queue messages, X-MQ-QUEUE-LOCKED - number of locked (in progress) messages\n"\
		"<OPTIONS> - Supported metods and this help", {}, "text/plain");
}

void SyncQueueHandler::Delete(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method DELETE Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT,GET,POST,HEAD,OPTIONS"},
			{"Allow","PUT,GET,POST,HEAD,OPTIONS"},
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
			{"Access-Control-Allow-Methods","PUT,GET,POST,HEAD,OPTIONS"},
			{"Allow","PUT,GET,POST,HEAD,OPTIONS"},
		});
}

void SyncQueueHandler::Patch(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method PATCH Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT,GET,POST,HEAD,OPTIONS"},
			{"Allow","PUT,GET,POST,HEAD,OPTIONS"},
		});
}

