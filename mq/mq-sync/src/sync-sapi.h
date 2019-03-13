#pragma once
#include <atomic>
#include <thread>
#include <dom.h>
#include <interface/IMQ.h>
#include <interface/IRequest.h>
#include <interface/ISAPI.h>
#include <interface/IKernel.h>
#include <interface/IConfig.h>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <condition_variable>

class SyncSAPIHandler : public Dom::Server::Object<SyncSAPIHandler, IMQ> {
private:
	std::atomic_bool									mq_working;
	std::atomic_int32_t									mq_workers;
	std::thread											mq_process;
	std::mutex											mq_sync;
	std::mutex											mq_qsync;
	std::condition_variable								mq_cond;
	std::queue<uint64_t>								mq_queue;
	Dom::Interface<IKernel>								mq_kernel;
	Dom::Interface<IConfig>								mq_config;
	std::string											mq_sapi_class;
	std::string											mq_sapi_script;
	std::unordered_map<uint64_t, Dom::Interface<IRequest>>	mq_messages;
	struct {
		size_t LimitTasks, LimitSize, LimitPeriodSec;
	} mq_options;


private:
	void WorkingThread(std::mutex&, std::condition_variable&, std::queue<uint64_t>&, std::atomic_bool&, std::atomic_int32_t&);
	void ExecuteThread(uint64_t msgid);
	inline void ReplyWithCode(IUnknown* unknown, size_t Code, const char* Message, std::string&& Body, std::unordered_map<const char*, const char*>&& headers = {}, const std::string& ContentType = ("application/json")) const;
public:
	SyncSAPIHandler();
	virtual ~SyncSAPIHandler();

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

	CLSID(SyncSAPI)
};
