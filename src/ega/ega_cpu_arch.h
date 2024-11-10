#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>

static bool ega_cpu_arch_load();
static void ega_memcpy(void* src, void* dst, int length);
static void avx2_memcpy(void *pvDest, void *pvSrc, size_t nBytes);