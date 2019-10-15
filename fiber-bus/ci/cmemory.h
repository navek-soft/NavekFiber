#pragma once
#include <mutex>

namespace ibus {
	class cmemory {
	private:
		class pool {
			typedef uint8_t T;
			typedef T value_type;
			typedef T* pointer;
			pointer* PoolHead{ nullptr }, * PoolTail{ nullptr }, * PoolIt{ nullptr };
			std::mutex	PoolLock;
		public:
			pool(const size_t pool_size) {
				PoolIt = PoolHead = ::new pointer[pool_size];
				PoolTail = PoolHead + pool_size;
			}
			~pool() {
				while (PoolIt != PoolHead) {
					::delete[](*(--PoolIt));
				}
				delete[] PoolHead;
			}

			template<typename C>
			inline C* alloc()
			{
				C* ptr = nullptr;
				PoolLock.lock();
				if (PoolIt != PoolHead) { ptr = (C*)(*(--PoolIt)); PoolLock.unlock(); }
				else { PoolLock.unlock(); ptr = ((C*)::new uint8_t[sizeof(C)]); }
				::new (ptr) C;
				return ptr;
			}
			template<typename C>
			inline C* alloc(const C& val)
			{
				C* ptr = nullptr;
				PoolLock.lock();
				if (PoolIt != PoolHead) { ptr = (C*)(*(--PoolIt)); PoolLock.unlock(); }
				else { PoolLock.unlock(); ptr = ((C*)::new uint8_t[sizeof(C)]); }
				::new (ptr) C(val);
				return ptr;
			}
			template<typename C>
			inline C* alloc(const C&& val)
			{
				C* ptr = nullptr;
				PoolLock.lock();
				if (PoolIt != PoolHead) { ptr = (C*)(*(--PoolIt)); PoolLock.unlock(); }
				else { PoolLock.unlock(); ptr = ((C*)::new uint8_t[sizeof(C)]); }
				::new (ptr) C(std::move(val));
				return ptr;
			}
			template<typename C>
			inline void free(C* ptr)
			{
				ptr->~C();
				PoolLock.lock();
				if (PoolIt < PoolTail) {
					*PoolIt = (pointer)ptr;
					PoolIt++;
					PoolLock.unlock();
					return;
				}
				PoolLock.unlock();
				::delete[]((pointer)ptr);
			}
		};
		static inline constexpr size_t mempoolFramePoolSize = 8192;
		static inline pool mempoolFrames { mempoolFramePoolSize };
	public:
		struct frame {
			ssize_t length;
			uint8_t	data[8192];
			frame() : length(0) { ; }

			static inline void* operator new(std::size_t sz) {
				return cmemory::mempoolFrames.alloc<frame>();
			}
			static inline void operator delete(void* ptr, std::size_t sz) {
				return cmemory::mempoolFrames.free<frame>((frame*)ptr);
			}
		};
	};
}