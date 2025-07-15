//
// Created by nev on 7/16/25.
//

#ifndef ARENA_STR_H
#define ARENA_STR_H

#include <string.h>
#include <unistd.h>
#include "arena.h"

// Base struct for Str
typedef struct Str {
    size_t size;
    char string[];
} Str;

// Pushes string to the given arena, counting on a null terminating character
static Str *PushStr(Arena *arena, const char *inStr) {
    if (!arena || !inStr) return NULL;

    // Pushes the inStr to the arena
    // returns a pointer to Str (in the arena)

    const size_t len = strlen(inStr);
    if (len == SIZE_MAX) return NULL; // overflow check

    // Allocate space for Str struct plus the string content
    Str *res = ArenaPush(arena, sizeof(Str) + len);
    if (!res) return NULL;

    res->size = len;
    memcpy(res->string, inStr, len); // don't copy null terminating character

    return res;
}

// pop a string off of the 'stack' - arena
static void PopStr(Arena *arena, const Str *str) {
    ArenaPop(arena, sizeof(Str) + str->size);
}

// prints string to fd, if fd is -1, prints to STDOUT
static void m_putstr(const Str *str, const int fd) {
    // stdin, stdout, and stderr are 0, 1, and 2, respectively
    // write requires unistd.h

    if (!str || fd < -1) return;
    // no size limit, not our problem

    int fd_new;
    if (fd == -1) fd_new = 1;
    else fd_new = dup(fd);

    size_t total = 0;
    while (total < str->size) {
        ssize_t written = write(fd_new,
                       str->string + total,
                       str->size - total);
        if (written <= 0) break;
        total += written;
    }

}
#endif //ARENA_STR_H