#pragma once

#include <chrono>
#include <memory>
#include <thread>
#include <mutex>

#include "Common.h"

#include "internal/SharedFromThisHelper.h"

namespace Rapcom
{
    DECLARE_SMARTPOINTER(ThreadedObject);
    class ThreadedObject :
        public SharedFromThis
    {
    public:
        ThreadedObject() :
            m_isActive(false)
        {}

        ~ThreadedObject()
        {
            m_isActive = false;
            m_timerThread->join();
        }

        // Starts the thread for the object
        bool StartThread();

        // Stops the thread for the object
        void StopThread();

        // Called when the thread should do work.
        virtual bool DoThreadWork() = 0;

    private:

        // Holds a reference to us so we can clean up properly.
        ThreadedObjectPtr m_selfRef;

        // The main thread
        std::unique_ptr<std::thread> m_timerThread;

        // If we are active or not
        bool m_isActive;

        // Mutex for sync
        std::mutex m_threadMutex;

        // The main thread loop
        void ThreadLoop();
    };
}
