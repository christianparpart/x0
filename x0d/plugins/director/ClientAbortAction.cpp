#include "ClientAbortAction.h"

using namespace x0;

Try<ClientAbortAction> parseClientAbortAction(const BufferRef& value)
{
    if (value == "close")
        return ClientAbortAction::Close;

    if (value == "notify")
        return ClientAbortAction::Notify;

    if (value == "ignore")
        return ClientAbortAction::Ignore;

    return Error("Invalid argument.");
}

std::string tos(ClientAbortAction value)
{
    switch (value) {
        case ClientAbortAction::Ignore:
            return "ignore";
        case ClientAbortAction::Close:
            return "close";
        case ClientAbortAction::Notify:
            return "notify";
    }
}
