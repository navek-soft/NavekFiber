#pragma once
#include <dom.h>
#include <interface/IMQ.h>
#include <interface/IRequest.h>
#include <interface/ISAPI.h>
#include <interface/IKernel.h>
#include <interface/IConfig.h>
#include <mutex>
#include <queue>
#include <unordered_set>

using namespace std;
using request_list = unordered_map<uint64_t, Dom::Interface<IRequest>>;

class SyncQueueHandler : public Dom::Client::Embedded<SyncQueueHandler, IMQ> {
private:
	mutex						mq_sync;
	queue<uint64_t>				mq_queue;
	unordered_set<uint64_t>		mq_locked;
	request_list				mq_messages;
	std::string					mq_name;
	struct {
		size_t LimitTasks, LimitSize, LimitPeriodSec;
	} mq_options;

	Dom::Interface<IKernel> mq_kernel;
	Dom::Interface<IConfig> mq_config;

private:
	inline bool ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers = {}, const std::string& ContentType = {});
	inline bool ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, const zcstring& Body, std::unordered_map<const char*, const char*>&& headers = {}, const std::string& ContentType = {});
	inline void GetStats(volatile size_t& QueueCount, volatile size_t& LockedCount, volatile size_t& MessagesCount) {
		{
			std::unique_lock<std::mutex> lock(mq_sync);
			QueueCount = mq_queue.size();
			LockedCount = mq_locked.size();
			MessagesCount = mq_messages.size();
		}
	}

	inline void UnSerialize();
	inline void Serialize();

public:
	SyncQueueHandler();
	virtual ~SyncQueueHandler();

	virtual bool Initialize(const char*, Dom::IUnknown* /* kernelObject */, Dom::IUnknown* /* optionsList */);
	virtual const Info&	GetInfo() const;
	virtual void Finalize(Dom::IUnknown*);

	virtual void Get(Dom::IUnknown* /* Request */);
	virtual void Post(Dom::IUnknown* /* Request */);
	virtual void Put(Dom::IUnknown* /* Request */);
	virtual void Head(Dom::IUnknown* /* Request */);
	virtual void Options(Dom::IUnknown* /* Request */);
	virtual void Delete(Dom::IUnknown* /* Request */);
	virtual void Trace(Dom::IUnknown* /* Request */);
	virtual void Connect(Dom::IUnknown* /* Request */);
	virtual void Patch(Dom::IUnknown* /* Request */);

	CLSID(SyncQueue)
};