#pragma once

#include <string>

#include "rapidjson\document.h"

#include "Common.h"

namespace Rapcom
{
    DECLARE_SMARTPOINTER(IRapcomListener);
    class IRapcomListener
    {
    public:
        // Fired when the config has been changed.
        virtual void OnConfigChange(rapidjson::Document& oldConfig, rapidjson::Document& newConfig) = 0;

        // Fired when a command has been issued
        virtual void OnCommand(rapidjson::Document& command) = 0;
    };
}