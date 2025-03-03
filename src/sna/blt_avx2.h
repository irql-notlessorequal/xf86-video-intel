#if defined(_AVX2_TEST)
#define avx2 1
#define force_inline inline

#define DBG(x) do {} while (0)

#define min(a,b)             \
({                           \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b;       \
})

#define assume_aligned(ptr, align) __builtin_assume_aligned((ptr), (align))
#define assume_misaligned(ptr, align, offset) __builtin_assume_aligned((ptr), (align), (offset))

#include <stdint.h>
#include <assert.h>
#include <string.h>
#endif

#if defined(avx2)
#pragma GCC push_options
#pragma GCC target("avx2")
#pragma GCC optimize("Ofast")
#include <immintrin.h>

static void avx2_memcpy(void *pvDest, const void *pvSrc, size_t nBytes)
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

static void to_memcpy_avx2(void *dst, const void *src, unsigned len)
{
	assert(len);

	/**
	 * There is no point calling libc's memcpy if we are dealing
	 * with less than 8 bytes.
	 */
	if (len < 8)
	{
		unsigned char *d = dst;
		const unsigned char *s = src;

		for (int i = 0; i < len; ++i)
			*d++ = *s++;
		return;
	}

	/**
	 * We must have our pointers aligned otherwise 
	 * there is no point in this magic shortcut.
	 */
	if (((uintptr_t)dst & 31) == 0 &&
		((uintptr_t)src & 31) == 0 &&
		(len % 256) == 0)
	{
		avx2_memcpy(dst, src, len);
		return;
	}

	/**
	 * We can massively simplify the code since Haswell and newer
	 * supports ERMSB (Enhanced REP MOVSB) meaning that glibc and
	 * other libc implementations will handle most cases just fine.
	 */
	memcpy(dst, src, len);
}

static void
memcpy_to_tiled_x__swizzle_0__avx2(const void *src, void *dst, int bpp,
				   int32_t src_stride, int32_t dst_stride,
				   int16_t src_x, int16_t src_y,
				   int16_t dst_x, int16_t dst_y,
				   uint16_t width, uint16_t height)
{
	const unsigned tile_width = 512;
	const unsigned tile_height = 8;
	const unsigned tile_size = 4096;

	const unsigned cpp = bpp / 8;
	const unsigned tile_pixels = tile_width / cpp;
	const unsigned tile_shift = ffs(tile_pixels) - 1;
	const unsigned tile_mask = tile_pixels - 1;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);

	if (src_x | src_y)
		src = (const uint8_t *)src + src_y * src_stride + src_x * cpp;
	assert(src_stride >= width * cpp);
	src_stride -= width * cpp;

	while (height--)
	{
		unsigned w = width * cpp;
		uint8_t *tile_row = dst;

		tile_row += dst_y / tile_height * dst_stride * tile_height;
		tile_row += (dst_y & (tile_height-1)) * tile_width;
		if (dst_x)
		{
			tile_row += (dst_x >> tile_shift) * tile_size;
			if (dst_x & tile_mask)
			{
				const unsigned x = (dst_x & tile_mask) * cpp;
				const unsigned len = min(tile_width - x, w);
				to_memcpy_avx2(assume_misaligned(tile_row + x, tile_width, x),
				       src, len);

				tile_row += tile_size;
				src = (const uint8_t *)src + len;
				w -= len;
			}
		}

		while (w >= tile_width)
		{
			to_memcpy_avx2(assume_aligned(tile_row, tile_width),
			       src, tile_width);
			tile_row += tile_size;
			src = (const uint8_t *)src + tile_width;
			w -= tile_width;
		}

		to_memcpy_avx2(assume_aligned(tile_row, tile_width), src, w);
		src = (const uint8_t *)src + src_stride + w;
		dst_y++;
	}
}

static void
memcpy_from_tiled_x__swizzle_0__avx2(const void *src, void *dst, int bpp,
				     int32_t src_stride, int32_t dst_stride,
				     int16_t src_x, int16_t src_y,
				     int16_t dst_x, int16_t dst_y,
				     uint16_t width, uint16_t height)
{
	const unsigned tile_width = 512;
	const unsigned tile_height = 8;
	const unsigned tile_size = 4096;

	const unsigned cpp = bpp / 8;
	const unsigned tile_pixels = tile_width / cpp;
	const unsigned tile_shift = ffs(tile_pixels) - 1;
	const unsigned tile_mask = tile_pixels - 1;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);

	if (dst_x | dst_y)
		dst = (uint8_t *)dst + dst_y * dst_stride + dst_x * cpp;
	assert(dst_stride >= width * cpp);
	dst_stride -= width * cpp;

	while (height--)
	{
		unsigned w = width * cpp;
		const uint8_t *tile_row = src;

		tile_row += src_y / tile_height * src_stride * tile_height;
		tile_row += (src_y & (tile_height-1)) * tile_width;
		if (src_x)
		{
			tile_row += (src_x >> tile_shift) * tile_size;
			if (src_x & tile_mask)
			{
				const unsigned x = (src_x & tile_mask) * cpp;
				const unsigned len = min(tile_width - x, w);
				to_memcpy_avx2(dst, assume_misaligned(tile_row + x, tile_width, x), len);

				tile_row += tile_size;
				dst = (uint8_t *)dst + len;
				w -= len;
			}
		}

		while (w >= tile_width)
		{
			to_memcpy_avx2(dst,
			       assume_aligned(tile_row, tile_width),
			       tile_width);

			tile_row += tile_size;
			dst = (uint8_t *)dst + tile_width;
			w -= tile_width;
		}

		to_memcpy_avx2(dst, assume_aligned(tile_row, tile_width), w);
		dst = (uint8_t *)dst + dst_stride + w;
		src_y++;
	}
}

static void
memcpy_between_tiled_x__swizzle_0__avx2(const void *src, void *dst, int bpp,
					int32_t src_stride, int32_t dst_stride,
					int16_t src_x, int16_t src_y,
					int16_t dst_x, int16_t dst_y,
					uint16_t width, uint16_t height)
{
	const unsigned tile_width = 512;
	const unsigned tile_height = 8;
	const unsigned tile_size = 4096;

	const unsigned cpp = bpp / 8;
	const unsigned tile_pixels = tile_width / cpp;
	const unsigned tile_shift = ffs(tile_pixels) - 1;
	const unsigned tile_mask = tile_pixels - 1;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);
	assert((dst_x & tile_mask) == (src_x & tile_mask));

	while (height--)
	{
		unsigned w = width * cpp;
		uint8_t *dst_row = dst;
		const uint8_t *src_row = src;

		dst_row += dst_y / tile_height * dst_stride * tile_height;
		dst_row += (dst_y & (tile_height-1)) * tile_width;
		if (dst_x)
			dst_row += (dst_x >> tile_shift) * tile_size;
		dst_y++;

		src_row += src_y / tile_height * src_stride * tile_height;
		src_row += (src_y & (tile_height-1)) * tile_width;
		if (src_x)
			src_row += (src_x >> tile_shift) * tile_size;
		src_y++;

		if (dst_x & tile_mask)
		{
			const unsigned x = (dst_x & tile_mask) * cpp;
			const unsigned len = min(tile_width - x, w);

			to_memcpy_avx2(
				assume_misaligned(dst_row + x, tile_width, x),
				assume_misaligned(src_row + x, tile_width, x), len);

			dst_row += tile_size;
			src_row += tile_size;
			w -= len;
		}

		while (w >= tile_width) {
			to_memcpy_avx2(
				assume_aligned(dst_row, tile_width), assume_aligned(src_row, tile_width), tile_width);
			dst_row += tile_size;
			src_row += tile_size;
			w -= tile_width;
		}

		to_memcpy_avx2(assume_aligned(dst_row, tile_width), assume_aligned(src_row, tile_width), w);
	}
}

#pragma GCC pop_options
#endif