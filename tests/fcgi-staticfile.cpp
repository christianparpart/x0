// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcgi_stdio.h>

int main()
{
    while (FCGI_Accept() >= 0) {
        const char *scriptname = getenv("SCRIPT_NAME");
        if (!scriptname) {
            printf("Content-Type: text/plain\r\n");
            printf("\r\n");
            printf("Invalid SCRIPT_NAME\r\n");
            continue;
        }
        const char *filename = strchr(scriptname, ':');
        if (!filename) {
            printf("Content-Type: text/plain\r\n");
            printf("\r\n");
            printf("Invalid request path (format: /prefix:/PATH)\r\n");
            continue;
        }
        ++filename;

        struct stat st;
        memset(&st, 0, sizeof(st));
#if 1
        if (stat(filename, &st) < 0) {
            printf("Content-Type: text/plain\r\n");
            printf("\r\n");
            printf("Could not stat file: %s: %s\n", filename, strerror(errno));
            continue;
        }
#endif

        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            printf("Content-Type: text/plain\r\n");
            printf("\r\n");
            printf("Could not open file: %s: %s\n", filename, strerror(errno));
            continue;
        }

        printf("Content-Type: application/octet-stream\r\n");
        if (st.st_size)
            printf("Content-Length: %lu\r\n", st.st_size);
        printf("\r\n");

        char buf[1024 * 16];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            size_t w = fwrite(buf, 1, n, stdout);
            if (n != w) {
                fprintf(stderr, "error writing file: %s (%ld, %ld): %s\n",
                        filename, n, w, strerror(errno));
                break;
            }
        }
        if (ferror(fp)) {
            fprintf(stderr, "error reading source file: %s: %s\n",
                    filename, strerror(errno));
        }

        fclose(fp);
        fflush(stdout);
    }
    return 0;
}
