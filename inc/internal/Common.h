#pragma once

#include <memory>
#include <iostream>
#include <cstdint>

#define DECLARE_SMARTPOINTER(_TYPE_)			   \
class _TYPE_;									   \
typedef std::shared_ptr<_TYPE_> _TYPE_##Ptr;	   \
typedef std::weak_ptr<_TYPE_> _TYPE_##WeakPtr;     \
typedef std::unique_ptr<_TYPE_> _TYPE_##UniquePtr;

#ifdef _MSC_VER
#define PLATFORM_WINDOWS
#endif // _MSC_VER
