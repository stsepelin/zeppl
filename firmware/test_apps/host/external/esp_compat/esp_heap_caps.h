#pragma once
// Host stand-in for ESP-IDF's heap_caps allocator: the capability flags are
// meaningless on host, so every call maps to plain libc allocation (see
// lvgl_stub.c). Tests can inject allocation failure via
// heap_caps_stub_fail_next() to exercise the widgets' alloc-failed paths.

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM  0
#define MALLOC_CAP_8BIT    0
#define MALLOC_CAP_DEFAULT 0

void *heap_caps_malloc(size_t size, int caps);
void *heap_caps_calloc(size_t n, size_t size, int caps);
void  heap_caps_free(void *ptr);

// Test hooks: fail the next `n` heap_caps allocations, optionally after
// letting `skip` of them succeed first.
void heap_caps_stub_fail_next(int n);
void heap_caps_stub_fail_after(int skip, int n);
