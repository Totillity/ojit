#include "ojit_def.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

char err_msg_buf[256] = { [0 ... 255] = 0};
uint8_t msg_size = 0;

void ojit_new_error() {
    memset(err_msg_buf, 0, 256);
    msg_size = 0;
}

void ojit_build_error_String(String str) {
    memcpy(err_msg_buf + msg_size, str->start_ptr, str->length);
    msg_size += str->length;
}

void ojit_build_error_chars(char* str) {
    size_t len = strlen(str);
    memcpy(err_msg_buf + msg_size, str, len);
    msg_size += len;
}

void ojit_build_error_char(char c) {
    err_msg_buf[msg_size] = c;
    msg_size += 1;
}

void ojit_build_error_int(int i) {
    msg_size += sprintf(err_msg_buf + msg_size, "%i", i);
}

void ojit_error() {
    puts(err_msg_buf);
    puts("\n");
}

void ojit_exit(int code) {
    exit(code);
}