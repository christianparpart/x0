// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "ClientAbortAction.h"

using namespace x0;

Try<ClientAbortAction> parseClientAbortAction(const BufferRef& value) {
  if (value == "close") return ClientAbortAction::Close;

  if (value == "notify") return ClientAbortAction::Notify;

  if (value == "ignore") return ClientAbortAction::Ignore;

  return Error("Invalid argument.");
}

std::string tos(ClientAbortAction value) {
  switch (value) {
    case ClientAbortAction::Ignore:
      return "ignore";
    case ClientAbortAction::Close:
      return "close";
    case ClientAbortAction::Notify:
      return "notify";
    default:
      abort();
  }
}
