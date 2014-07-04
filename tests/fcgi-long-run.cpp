// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdlib.h>
#include <unistd.h>
#include <fcgi_stdio.h>

int main() {
  while (FCGI_Accept() >= 0) {
    printf("Content-Type: text/plain\r\n");
    printf("\r\n");
    printf("Hello, World\n");
    printf("script_name: %s\n", getenv("SCRIPT_NAME"));
    printf("\n");
    fflush(stdout);

    for (int i = 8; i != 0; --i) {
      sleep(4);
      printf("%d moments to life ...\n", i);
      fflush(stdout);
    }
    printf("bye.\n");
  }
  return 0;
}
