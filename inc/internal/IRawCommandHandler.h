#pragma once

#include <string>

#include "Common.h"

namespace Rapcom
{
	DECLARE_SMARTPOINTER(RawCommandResponse);
    struct RawCommandResponse
    {
        std::string jsonResponse;
        std::string responseCode;
    };

    DECLARE_SMARTPOINTER(IRawCommandHandler);
    class IRawCommandHandler
    {
    public:
        virtual RawCommandResponse OnRawCommand(const char* jsonString, size_t length) = 0;
    };
}