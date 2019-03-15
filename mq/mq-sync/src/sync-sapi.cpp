#include "sync-sapi.h"
#include "trace.h"
#include <utils/option.h>
#include <regex>

static inline bool parseSapiUri(const std::string& uri, std::string& mq, std::string& route) {
	std::regex re(R"(([\w-]+)://(.+))");
	std::smatch match;
	if (std::regex_search(uri, match, re) && match.size() > 1) {
		mq = match.str(1);
		route = match.str(2);
		return true;
	}
	return false;
}

using namespace Dom;
using namespace std;

inline void SyncSAPIHandler::ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers, const std::string& ContentType) const {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		std::string queueLength = std::to_string(mq_queue.size());
		headers.emplace("X-MQ-CLASS", "Sync.MQ.SAPI");
		headers.emplace("X-MQ-QUEUE", queueLength.c_str());
		if (!ContentType.empty()) headers.emplace("Content-Type", ContentType.c_str());
		request->Reply(Code, Message, Body.data(), Body.length(), headers, true);
	}
}


SyncSAPIHandler::SyncSAPIHandler() : mq_working(0), mq_workers(0) {
	dbg_trace("");
}

SyncSAPIHandler::~SyncSAPIHandler() {
	Finalize(nullptr);
	dbg_trace();
}

void SyncSAPIHandler::ExecuteThread(uint64_t msgid) {

	do {
		Interface<IRequest> request;
		{
			unique_lock<mutex> lock(mq_sync);
			if (mq_queue.empty()) break;
			msgid = mq_queue.front();
			mq_queue.pop();
			request = move(mq_messages.at(msgid));
		}
		try {
			ReplyWithCode(request, 200, nullptr, "{}", { {"XSample-HEADER","909"} });
			/*
			Interface<ISAPI> sapi;
			mq_sapi->Clone((IUnknown**)&sapi);
			if (!sapi) {
				printf("SyncSAPIHandler::ExecuteThread SAPI Clone error");
				break;
			}
			bool ConfirmAck = false, ConfirmReply = false;
			sapi->Execute(msgid, request, ConfirmReply, ConfirmAck);
			if (ConfirmAck)
			{
				unique_lock<mutex> lock(mq_sync);
				request->CloseConnection();
				mq_messages.erase(msgid);
				if (mq_queue.empty()) break;
			}
			else {
				ReplyWithCode(request, "503", "Service Unavailable", "SAPI Endpoint error", {
					{"X-MQ-CLASS","SyncSAPIHandler"},
					});
				request->CloseConnection();
			}
			*/
		}
		catch (...) {
			ReplyWithCode(request, 503, "SAPI Endpoint error", "");
		}
	} while (mq_working && !mq_queue.empty());
	mq_workers--;
}

void SyncSAPIHandler::WorkingThread(mutex& sync, condition_variable& cond, queue<uint64_t>& messages, atomic_bool& yet, atomic_int32_t& workers) {
	unique_lock<mutex> lock(sync);
	do {
		cond.wait(lock, [&yet, &messages]() {
			return !yet || !messages.empty();
		});
		if (!yet) break;
		if (messages.empty()) continue;
		mq_workers++;
		auto msgid = mq_queue.front();
		mq_queue.pop();
		thread(&SyncSAPIHandler::ExecuteThread, this, msgid).detach();
	} while (mq_working);
}

bool SyncSAPIHandler::Initialize(IUnknown* pIKernel, IUnknown* pIConfig) {

	if (pIKernel && pIKernel->QueryInterface(mq_kernel.guid(), mq_kernel)) {
		pIKernel->AddRef();
		if (pIConfig && pIConfig->QueryInterface(mq_config.guid(), mq_config)) {
			pIConfig->AddRef();
			mq_options.LimitPeriodSec = utils::option::time_period(mq_config->GetConfigValue("queue-limit-request", "0s"));
			try {
				mq_options.LimitSize = std::stol(mq_config->GetConfigValue("queue-limit-size", "0"));
			}
			catch (...) {
				log_print("<Runtime.Warning> Invalid `queue-limit-size` option value. Default: 0");
				mq_options.LimitSize = 0;
			}
			try {
				mq_options.LimitTasks = std::stol(mq_config->GetConfigValue("queue-limit-tasks", "0"));
			}
			catch (...) {
				log_print("<Runtime.Warning> Invalid `queue-limit-size` option value. Default: 0");
				mq_options.LimitTasks = 0;
			}
			if (!parseSapiUri(mq_config->GetConfigValue("channel-sapi", ""), mq_sapi_class, mq_sapi_script)) {
				log_print("<Runtime.Error> Invalid `channel-sapi` option value.");
				return false;
			}
		}
		mq_working = true;
		return true;
	}
	return false;
}

const IMQ::Info& SyncSAPIHandler::GetInfo() const {
	static constexpr IMQ::Info module_info = { "Sync.MQ.SAPI","0.0.1" };
	return module_info;
}

void SyncSAPIHandler::Finalize(IUnknown*) {
	if (mq_working) {
		{
			unique_lock<mutex> lock(mq_sync);
			mq_working = false;
			//mq_cond.notify_one();
		}
		mq_process.join();
	}
}

void SyncSAPIHandler::Get(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method GET Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Post(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method GET Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Put(IUnknown* unknown) {
	Interface<IRequest> request;
	if (request.QueryInterface(unknown)) {
		if (mq_working) {
			unique_lock<mutex> lock(mq_sync);
			if (!mq_options.LimitSize || mq_queue.size() < mq_options.LimitSize) {
				if (mq_messages.emplace(request->GetMsgId(), move(request)).second) {
					mq_queue.emplace(request->GetMsgId());
					if ((!mq_options.LimitTasks || mq_workers < (int32_t)mq_options.LimitTasks)) {
						mq_workers++;
						thread(&SyncSAPIHandler::ExecuteThread, this, 0).detach();
					}
					return;
				}
			}
			auto&& slimit = std::to_string(mq_options.LimitSize);
			ReplyWithCode(unknown, 507, "Insufficient Storage", "", { {"X-MQ-LIMIT",slimit.c_str()} });
		}
		else {
			request->Reply(521, "SyncQueue is down pending", "");
		}
	}
	else {
		throw std::runtime_error("IRequest not implemented");
	}
}

void SyncSAPIHandler::Head(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method HEAD Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Options(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method OPTIONS Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Delete(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method DELETE Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Trace(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method TRACE Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Connect(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method CONNECT Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

void SyncSAPIHandler::Patch(IUnknown* unknown) {
	ReplyWithCode(unknown, 405, "Method PATCH Not Allowed", "", {
			{"Access-Control-Allow-Methods","PUT"},
			{"Allow","PUT"},
		});
}

