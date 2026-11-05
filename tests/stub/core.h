/* host stand-in for zeal-coreutils core.h */
#include <stddef.h>
#include <stdint.h>
void *mem_cpy(void *dst, const void *src, size_t size);
void *mem_set(void *ptr, uint8_t value, size_t size);
