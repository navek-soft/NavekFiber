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
	struct {
		size_t LimitTasks, LimitSize, LimitPeriodSec;
	} mq_options;

	Dom::Interface<IKernel> mq_kernel;
	Dom::Interface<IConfig> mq_config;

private:
	inline bool ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers = {}, const std::string& ContentType = {}) const;
	inline bool ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, const zcstring& Body, std::unordered_map<const char*, const char*>&& headers = {}, const std::string& ContentType = {});
public:
	SyncQueueHandler();
	virtual ~SyncQueueHandler();

	virtual bool Initialize(Dom::IUnknown* /* kernelObject */, Dom::IUnknown* /* optionsList */);
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