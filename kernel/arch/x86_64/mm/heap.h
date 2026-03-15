// PolandOS — Alokator sterty jadra
#pragma once
#include "../../../../include/types.h"

void  heap_init(u64 heap_start, u64 heap_size);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);
