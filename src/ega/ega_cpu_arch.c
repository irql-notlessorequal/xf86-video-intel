#include "ega_cpu_arch.h"

static bool avx2_viable = -1;

static bool ega_cpu_arch_load()
{
	return true;
}

static void ega_memcpy(void* src, void* dst, int length)
{
	if (avx2_viable)
	{
		avx2_memcpy(src, dst, length);
	}
	else
	{
		memcpy(src, dst, length);
	}
}

static void avx2_memcpy(void *pvDest, void *pvSrc, size_t nBytes) {
	assert(!"TODO");
}