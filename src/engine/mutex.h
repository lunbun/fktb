#pragma once

#include <mutex>
#include <thread>
#include <atomic>

namespace ami {

    // Taken from https://stackoverflow.com/a/30109512.
    // This mutex class is equivalent to std::mutex, but it allows us to detect if the current thread is holding the mutex (useful for
    // debugging).
    class mutex :
        public std::mutex {
    public:
#ifndef NDEBUG
        void lock() {
            std::mutex::lock();
            m_holder = std::this_thread::get_id();
        }
#endif // #ifndef NDEBUG

#ifndef NDEBUG
        void unlock() {
            m_holder = std::thread::id();
            std::mutex::unlock();
        }
#endif // #ifndef NDEBUG

#ifndef NDEBUG
        bool try_lock() {
            if (std::mutex::try_lock()) {
                m_holder = std::thread::id();
                return true;
            }
            return false;
        }
#endif // #ifndef NDEBUG

#ifndef NDEBUG
        /**
        * @return true iff the mutex is locked by the caller of this method. */
        bool locked_by_caller() const {
            return m_holder == std::this_thread::get_id();
        }
#endif // #ifndef NDEBUG

    private:
#ifndef NDEBUG
        std::atomic<std::thread::id> m_holder = std::thread::id{ };
#endif // #ifndef NDEBUG
    };

}
