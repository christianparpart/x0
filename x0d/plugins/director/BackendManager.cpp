// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "BackendManager.h"
#include <x0/JsonWriter.h>

BackendManager::BackendManager(x0::HttpWorker* worker, const std::string& name) :
#ifndef XZERO_NDEBUG
    x0::Logging("BackendManager/%s", name.c_str()),
#endif
    worker_(worker),
    name_(name),
    connectTimeout_(x0::TimeSpan::fromSeconds(10)),
    readTimeout_(x0::TimeSpan::fromSeconds(120)),
    writeTimeout_(x0::TimeSpan::fromSeconds(10)),
    transferMode_(TransferMode::Blocking),
    clientAbortAction_(ClientAbortAction::Close),
    load_()
{
}

BackendManager::~BackendManager()
{
}

void BackendManager::log(x0::LogMessage&& msg)
{
    msg.addTag(name_);
    worker_->log(std::move(msg));
}

TransferMode makeTransferMode(const std::string& value)
{
    if (value == "file")
        return TransferMode::FileAccel;

    if (value == "memory")
        return TransferMode::MemoryAccel;

    if (value == "blocking")
        return TransferMode::Blocking;

    // default to "blocking" if value is not recognized.
    return TransferMode::Blocking;
}

std::string tos(TransferMode value)
{
    static const std::string s[] = {
        "blocking",
        "memory",
        "file"
    };
    return s[static_cast<size_t>(value)];
}

namespace x0 {
    x0::JsonWriter& operator<<(x0::JsonWriter& json, const TransferMode& mode)
    {
        switch (mode) {
        case TransferMode::FileAccel:
            json.value("file");
            break;
        case TransferMode::MemoryAccel:
            json.value("memory");
            break;
        case TransferMode::Blocking:
            json.value("blocking");
            break;
        }
        return json;
    }
}
