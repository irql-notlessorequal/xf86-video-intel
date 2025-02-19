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

static __attribute__((target("avx2"))) void avx2_memcpy(void *pvDest, void *pvSrc, size_t nBytes)
{
	assert(nBytes % 32 == 0);
	assert(((intptr_t)pvDest & 31) == 0);
	assert(((intptr_t)pvSrc & 31) == 0);
	const __m256i *pSrc = (const __m256i*)pvSrc;
	__m256i *pDest = (__m256i*)pvDest;
	int64_t nVects = nBytes / sizeof(*pSrc);
	for (; nVects > 0; nVects--, pSrc++, pDest++) {
		const __m256i loaded = _mm256_stream_load_si256(pSrc);
		_mm256_stream_si256(pDest, loaded);
	}
	_mm_sfence();
}