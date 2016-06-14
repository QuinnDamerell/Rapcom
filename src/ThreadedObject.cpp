#include <chrono>
#include <ratio>

#include "internal/ThreadedObject.h"

using namespace Rapcom;

bool ThreadedObject::StartThread()
{
    std::lock_guard<std::mutex> lock(m_threadMutex);

    if (m_isActive)
    {
        return false;
    }

    // Take a ref on ourself
    m_selfRef = GetSharedPtr<ThreadedObject>();

    // Setup a a thread and run it.
    m_isActive = true;
    m_timerThread.reset(new std::thread(std::bind(&ThreadedObject::ThreadLoop, this)));
    return true;
}

void ThreadedObject::StopThread()
{
    m_isActive = false;
}

void ThreadedObject::ThreadLoop()
{
    while (m_isActive)
    {
        // Check if we should keep running
        if (m_selfRef.use_count() == 1)
        {
            // If we are the last ref we should stop
            m_isActive = false;
            break;
        }

        if (!DoThreadWork())
        {
            // If returned false we are done
            m_isActive = false;
            break;
        }  
    }

    // Remove the ref
    m_selfRef = nullptr;
}
