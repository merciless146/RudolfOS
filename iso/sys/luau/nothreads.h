// Single-threaded stand-ins for the C++ locks Luau's type checker names (Rudolf OS 1.2.5). GPL-2.0-or-later.
// WebAssembly in the page has one thread and wasi-sdk's C++ library leaves these out; the checker runs its work
// queue in place when it is given no task runner, so nothing ever waits.
#pragma once
#ifdef __cplusplus
#include <cstdlib>
#include <mutex>
#include <condition_variable>
namespace std {
struct mutex { void lock() {} void unlock() {} bool try_lock() { return true; } };
template<class M> struct lock_guard { explicit lock_guard(M&) {} };
template<class... M> struct scoped_lock { explicit scoped_lock(M&...) {} };
template<class M> struct unique_lock { explicit unique_lock(M&) {} void lock() {} void unlock() {} };
template<class M> unique_lock(M&) -> unique_lock<M>;
struct condition_variable {
	template<class L, class P> void wait(L&, P pred) { if (!pred()) abort(); }
	void notify_one() {} void notify_all() {}
};
}
#endif
