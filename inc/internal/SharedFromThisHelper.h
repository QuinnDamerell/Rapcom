#pragma once

#include <memory>

namespace Rapcom
{
    class SharedFromThisVirtualBase :
        public std::enable_shared_from_this<SharedFromThisVirtualBase>
    {
    protected:
        SharedFromThisVirtualBase()
        {
        }

        virtual ~SharedFromThisVirtualBase()
        {
        }

    public:
        template <typename T>
        std::shared_ptr<T> GetSharedPtr()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

        template <typename T>
        std::weak_ptr<T> GetWeakPtr()
        {
            return std::weak_ptr<T>(GetSharedPtr<T>());
        }
    };

    class SharedFromThis : public virtual SharedFromThisVirtualBase
    {
    public:
        SharedFromThis() : SharedFromThisVirtualBase()
        {
        }

        virtual ~SharedFromThis()
        {
        }
    };
}