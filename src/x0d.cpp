// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroDaemon.h>
#include <base/DebugLogger.h>

int main(int argc, char *argv[]) {
  base::DebugLogger::get().configure("XZERO_DEBUG");
  x0d::XzeroDaemon daemon(argc, argv);
  return daemon.run();
}