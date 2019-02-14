#pragma once
#include "../../lib/dom/src/dom/IUnknown.h"
#include <atomic>

struct IThreadJob : virtual public Dom::IUnknown {

	virtual void Exec(const std::atomic_bool& /* DoWork */) = 0;

	IID(ThreadJob)
};

struct IThreadPool : virtual public Dom::IUnknown {

	virtual void Enqueue(Dom::IUnknown* Job) = 0;

	IID(ThreadPool)
};
