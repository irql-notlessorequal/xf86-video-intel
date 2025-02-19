#if defined(sse2)
#pragma GCC push_options
#pragma GCC target("sse2,inline-all-stringops,fpmath=sse")
#pragma GCC optimize("Ofast")
#include <xmmintrin.h>

static force_inline __m128i
xmm_create_mask_32(uint32_t mask)
{
	return _mm_set_epi32(mask, mask, mask, mask);
}

static force_inline __m128i
xmm_load_128(const __m128i *src)
{
	return _mm_load_si128(src);
}

static force_inline __m128i
xmm_load_128u(const __m128i *src)
{
	return _mm_loadu_si128(src);
}

static force_inline void
xmm_save_128(__m128i *dst, __m128i data)
{
	_mm_store_si128(dst, data);
}

static force_inline void
xmm_save_128u(__m128i *dst, __m128i data)
{
	_mm_storeu_si128(dst, data);
}

static force_inline void
to_sse128xN(uint8_t *dst, const uint8_t *src, int bytes)
{
	int i;

	for (i = 0; i < bytes / 128; i++) {
		__m128i xmm0, xmm1, xmm2, xmm3;
		__m128i xmm4, xmm5, xmm6, xmm7;

		xmm0 = xmm_load_128u((const __m128i*)src + 0);
		xmm1 = xmm_load_128u((const __m128i*)src + 1);
		xmm2 = xmm_load_128u((const __m128i*)src + 2);
		xmm3 = xmm_load_128u((const __m128i*)src + 3);
		xmm4 = xmm_load_128u((const __m128i*)src + 4);
		xmm5 = xmm_load_128u((const __m128i*)src + 5);
		xmm6 = xmm_load_128u((const __m128i*)src + 6);
		xmm7 = xmm_load_128u((const __m128i*)src + 7);

		xmm_save_128((__m128i*)dst + 0, xmm0);
		xmm_save_128((__m128i*)dst + 1, xmm1);
		xmm_save_128((__m128i*)dst + 2, xmm2);
		xmm_save_128((__m128i*)dst + 3, xmm3);
		xmm_save_128((__m128i*)dst + 4, xmm4);
		xmm_save_128((__m128i*)dst + 5, xmm5);
		xmm_save_128((__m128i*)dst + 6, xmm6);
		xmm_save_128((__m128i*)dst + 7, xmm7);

		dst += 128;
		src += 128;
	}
}

static force_inline void
to_sse64(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2, xmm3, xmm4;

	xmm1 = xmm_load_128u((const __m128i*)src + 0);
	xmm2 = xmm_load_128u((const __m128i*)src + 1);
	xmm3 = xmm_load_128u((const __m128i*)src + 2);
	xmm4 = xmm_load_128u((const __m128i*)src + 3);

	xmm_save_128((__m128i*)dst + 0, xmm1);
	xmm_save_128((__m128i*)dst + 1, xmm2);
	xmm_save_128((__m128i*)dst + 2, xmm3);
	xmm_save_128((__m128i*)dst + 3, xmm4);
}

static force_inline void
to_sse32(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2;

	xmm1 = xmm_load_128u((const __m128i*)src + 0);
	xmm2 = xmm_load_128u((const __m128i*)src + 1);

	xmm_save_128((__m128i*)dst + 0, xmm1);
	xmm_save_128((__m128i*)dst + 1, xmm2);
}

static force_inline void
to_sse16(uint8_t *dst, const uint8_t *src)
{
	xmm_save_128((__m128i*)dst, xmm_load_128u((const __m128i*)src));
}

static void to_memcpy_sse2(uint8_t *dst, const uint8_t *src, unsigned len)
{
	assert(len);
	if ((uintptr_t)dst & 15) {
		if (len <= 16 - ((uintptr_t)dst & 15)) {
			memcpy(dst, src, len);
			return;
		}

		if ((uintptr_t)dst & 1) {
			assert(len >= 1);
			*dst++ = *src++;
			len--;
		}
		if ((uintptr_t)dst & 2) {
			assert(((uintptr_t)dst & 1) == 0);
			assert(len >= 2);
			*(uint16_t *)dst = *(const uint16_t *)src;
			dst += 2;
			src += 2;
			len -= 2;
		}
		if ((uintptr_t)dst & 4) {
			assert(((uintptr_t)dst & 3) == 0);
			assert(len >= 4);
			*(uint32_t *)dst = *(const uint32_t *)src;
			dst += 4;
			src += 4;
			len -= 4;
		}
		if ((uintptr_t)dst & 8) {
			assert(((uintptr_t)dst & 7) == 0);
			assert(len >= 8);
			*(uint64_t *)dst = *(const uint64_t *)src;
			dst += 8;
			src += 8;
			len -= 8;
		}
	}

	assert(((uintptr_t)dst & 15) == 0);
	while (len >= 64) {
		to_sse64(dst, src);
		dst += 64;
		src += 64;
		len -= 64;
	}
	if (len == 0)
		return;

	if (len & 32) {
		to_sse32(dst, src);
		dst += 32;
		src += 32;
	}
	if (len & 16) {
		to_sse16(dst, src);
		dst += 16;
		src += 16;
	}
	if (len & 8) {
		*(uint64_t *)dst = *(uint64_t *)src;
		dst += 8;
		src += 8;
	}
	if (len & 4) {
		*(uint32_t *)dst = *(uint32_t *)src;
		dst += 4;
		src += 4;
	}
	memcpy(dst, src, len & 3);
}

static void
memcpy_to_tiled_x__swizzle_0__sse2(const void *src, void *dst, int bpp,
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

	unsigned offset_x, length_x;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);

	if (src_x | src_y)
		src = (const uint8_t *)src + src_y * src_stride + src_x * cpp;
	width *= cpp;
	assert(src_stride >= width);

	if (dst_x & tile_mask) {
		offset_x = (dst_x & tile_mask) * cpp;
		length_x = min(tile_width - offset_x, width);
	} else
		length_x = 0;
	dst = (uint8_t *)dst + (dst_x >> tile_shift) * tile_size;

	while (height--) {
		unsigned w = width;
		const uint8_t *src_row = src;
		uint8_t *tile_row = dst;

		src = (const uint8_t *)src + src_stride;

		tile_row += dst_y / tile_height * dst_stride * tile_height;
		tile_row += (dst_y & (tile_height-1)) * tile_width;
		dst_y++;

		if (length_x) {
			to_memcpy_sse2(tile_row + offset_x, src_row, length_x);

			tile_row += tile_size;
			src_row = (const uint8_t *)src_row + length_x;
			w -= length_x;
		}
		while (w >= tile_width) {
			assert(((uintptr_t)tile_row & (tile_width - 1)) == 0);
			to_sse128xN(assume_aligned(tile_row, tile_width),
				    src_row, tile_width);
			tile_row += tile_size;
			src_row = (const uint8_t *)src_row + tile_width;
			w -= tile_width;
		}
		if (w) {
			assert(((uintptr_t)tile_row & (tile_width - 1)) == 0);
			to_memcpy_sse2(assume_aligned(tile_row, tile_width),
				  src_row, w);
		}
	}
}

static force_inline void
from_sse128xNu(uint8_t *dst, const uint8_t *src, int bytes)
{
	int i;

	assert(((uintptr_t)src & 15) == 0);

	for (i = 0; i < bytes / 128; i++) {
		__m128i xmm0, xmm1, xmm2, xmm3;
		__m128i xmm4, xmm5, xmm6, xmm7;

		xmm0 = xmm_load_128((const __m128i*)src + 0);
		xmm1 = xmm_load_128((const __m128i*)src + 1);
		xmm2 = xmm_load_128((const __m128i*)src + 2);
		xmm3 = xmm_load_128((const __m128i*)src + 3);
		xmm4 = xmm_load_128((const __m128i*)src + 4);
		xmm5 = xmm_load_128((const __m128i*)src + 5);
		xmm6 = xmm_load_128((const __m128i*)src + 6);
		xmm7 = xmm_load_128((const __m128i*)src + 7);

		xmm_save_128u((__m128i*)dst + 0, xmm0);
		xmm_save_128u((__m128i*)dst + 1, xmm1);
		xmm_save_128u((__m128i*)dst + 2, xmm2);
		xmm_save_128u((__m128i*)dst + 3, xmm3);
		xmm_save_128u((__m128i*)dst + 4, xmm4);
		xmm_save_128u((__m128i*)dst + 5, xmm5);
		xmm_save_128u((__m128i*)dst + 6, xmm6);
		xmm_save_128u((__m128i*)dst + 7, xmm7);

		dst += 128;
		src += 128;
	}
}

static force_inline void
from_sse128xNa(uint8_t *dst, const uint8_t *src, int bytes)
{
	int i;

	assert(((uintptr_t)dst & 15) == 0);
	assert(((uintptr_t)src & 15) == 0);

	for (i = 0; i < bytes / 128; i++) {
		__m128i xmm0, xmm1, xmm2, xmm3;
		__m128i xmm4, xmm5, xmm6, xmm7;

		xmm0 = xmm_load_128((const __m128i*)src + 0);
		xmm1 = xmm_load_128((const __m128i*)src + 1);
		xmm2 = xmm_load_128((const __m128i*)src + 2);
		xmm3 = xmm_load_128((const __m128i*)src + 3);
		xmm4 = xmm_load_128((const __m128i*)src + 4);
		xmm5 = xmm_load_128((const __m128i*)src + 5);
		xmm6 = xmm_load_128((const __m128i*)src + 6);
		xmm7 = xmm_load_128((const __m128i*)src + 7);

		xmm_save_128((__m128i*)dst + 0, xmm0);
		xmm_save_128((__m128i*)dst + 1, xmm1);
		xmm_save_128((__m128i*)dst + 2, xmm2);
		xmm_save_128((__m128i*)dst + 3, xmm3);
		xmm_save_128((__m128i*)dst + 4, xmm4);
		xmm_save_128((__m128i*)dst + 5, xmm5);
		xmm_save_128((__m128i*)dst + 6, xmm6);
		xmm_save_128((__m128i*)dst + 7, xmm7);

		dst += 128;
		src += 128;
	}
}

static force_inline void
from_sse64u(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2, xmm3, xmm4;

	assert(((uintptr_t)src & 15) == 0);

	xmm1 = xmm_load_128((const __m128i*)src + 0);
	xmm2 = xmm_load_128((const __m128i*)src + 1);
	xmm3 = xmm_load_128((const __m128i*)src + 2);
	xmm4 = xmm_load_128((const __m128i*)src + 3);

	xmm_save_128u((__m128i*)dst + 0, xmm1);
	xmm_save_128u((__m128i*)dst + 1, xmm2);
	xmm_save_128u((__m128i*)dst + 2, xmm3);
	xmm_save_128u((__m128i*)dst + 3, xmm4);
}

static force_inline void
from_sse64a(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2, xmm3, xmm4;

	assert(((uintptr_t)dst & 15) == 0);
	assert(((uintptr_t)src & 15) == 0);

	xmm1 = xmm_load_128((const __m128i*)src + 0);
	xmm2 = xmm_load_128((const __m128i*)src + 1);
	xmm3 = xmm_load_128((const __m128i*)src + 2);
	xmm4 = xmm_load_128((const __m128i*)src + 3);

	xmm_save_128((__m128i*)dst + 0, xmm1);
	xmm_save_128((__m128i*)dst + 1, xmm2);
	xmm_save_128((__m128i*)dst + 2, xmm3);
	xmm_save_128((__m128i*)dst + 3, xmm4);
}

static force_inline void
from_sse32u(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2;

	xmm1 = xmm_load_128((const __m128i*)src + 0);
	xmm2 = xmm_load_128((const __m128i*)src + 1);

	xmm_save_128u((__m128i*)dst + 0, xmm1);
	xmm_save_128u((__m128i*)dst + 1, xmm2);
}

static force_inline void
from_sse32a(uint8_t *dst, const uint8_t *src)
{
	__m128i xmm1, xmm2;

	assert(((uintptr_t)dst & 15) == 0);
	assert(((uintptr_t)src & 15) == 0);

	xmm1 = xmm_load_128((const __m128i*)src + 0);
	xmm2 = xmm_load_128((const __m128i*)src + 1);

	xmm_save_128((__m128i*)dst + 0, xmm1);
	xmm_save_128((__m128i*)dst + 1, xmm2);
}

static force_inline void
from_sse16u(uint8_t *dst, const uint8_t *src)
{
	assert(((uintptr_t)src & 15) == 0);

	xmm_save_128u((__m128i*)dst, xmm_load_128((const __m128i*)src));
}

static force_inline void
from_sse16a(uint8_t *dst, const uint8_t *src)
{
	assert(((uintptr_t)dst & 15) == 0);
	assert(((uintptr_t)src & 15) == 0);

	xmm_save_128((__m128i*)dst, xmm_load_128((const __m128i*)src));
}

static void
memcpy_from_tiled_x__swizzle_0__sse2(const void *src, void *dst, int bpp,
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

	unsigned length_x, offset_x;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);

	if (dst_x | dst_y)
		dst = (uint8_t *)dst + dst_y * dst_stride + dst_x * cpp;
	width *= cpp;
	assert(dst_stride >= width);
	if (src_x & tile_mask) {
		offset_x = (src_x & tile_mask) * cpp;
		length_x = min(tile_width - offset_x, width);
		dst_stride -= width;
		dst_stride += (width - length_x) & 15;
	} else {
		offset_x = 0;
		dst_stride -= width & ~15;
	}
	assert(dst_stride >= 0);
	src = (const uint8_t *)src + (src_x >> tile_shift) * tile_size;

	while (height--) {
		unsigned w = width;
		const uint8_t *tile_row = src;

		tile_row += src_y / tile_height * src_stride * tile_height;
		tile_row += (src_y & (tile_height-1)) * tile_width;
		src_y++;

		if (offset_x) {
			memcpy(dst, tile_row + offset_x, length_x);
			tile_row += tile_size;
			dst = (uint8_t *)dst + length_x;
			w -= length_x;
		}

		if ((uintptr_t)dst & 15) {
			while (w >= tile_width) {
				from_sse128xNu(dst,
					       assume_aligned(tile_row, tile_width),
					       tile_width);
				tile_row += tile_size;
				dst = (uint8_t *)dst + tile_width;
				w -= tile_width;
			}
			while (w >= 64) {
				from_sse64u(dst, tile_row);
				tile_row += 64;
				dst = (uint8_t *)dst + 64;
				w -= 64;
			}
			if (w & 32) {
				from_sse32u(dst, tile_row);
				tile_row += 32;
				dst = (uint8_t *)dst + 32;
			}
			if (w & 16) {
				from_sse16u(dst, tile_row);
				tile_row += 16;
				dst = (uint8_t *)dst + 16;
			}
			memcpy(dst, assume_aligned(tile_row, 16), w & 15);
		} else {
			while (w >= tile_width) {
				from_sse128xNa(assume_aligned(dst, 16),
					       assume_aligned(tile_row, tile_width),
					       tile_width);
				tile_row += tile_size;
				dst = (uint8_t *)dst + tile_width;
				w -= tile_width;
			}
			while (w >= 64) {
				from_sse64a(dst, tile_row);
				tile_row += 64;
				dst = (uint8_t *)dst + 64;
				w -= 64;
			}
			if (w & 32) {
				from_sse32a(dst, tile_row);
				tile_row += 32;
				dst = (uint8_t *)dst + 32;
			}
			if (w & 16) {
				from_sse16a(dst, tile_row);
				tile_row += 16;
				dst = (uint8_t *)dst + 16;
			}
			memcpy(assume_aligned(dst, 16),
			       assume_aligned(tile_row, 16),
			       w & 15);
		}
		dst = (uint8_t *)dst + dst_stride;
	}
}

static void
memcpy_between_tiled_x__swizzle_0__sse2(const void *src, void *dst, int bpp,
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

	unsigned ox, lx;

	DBG(("%s(bpp=%d): src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, bpp, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));
	assert(src != dst);

	width *= cpp;
	dst_stride *= tile_height;
	src_stride *= tile_height;

	assert((dst_x & tile_mask) == (src_x & tile_mask));
	if (dst_x & tile_mask) {
		ox = (dst_x & tile_mask) * cpp;
		lx = min(tile_width - ox, width);
		assert(lx != 0);
	} else
		lx = 0;

	if (dst_x)
		dst = (uint8_t *)dst + (dst_x >> tile_shift) * tile_size;
	if (src_x)
		src = (const uint8_t *)src + (src_x >> tile_shift) * tile_size;

	while (height--) {
		const uint8_t *src_row;
		uint8_t *dst_row;
		unsigned w = width;

		dst_row = dst;
		dst_row += dst_y / tile_height * dst_stride;
		dst_row += (dst_y & (tile_height-1)) * tile_width;
		dst_y++;

		src_row = src;
		src_row += src_y / tile_height * src_stride;
		src_row += (src_y & (tile_height-1)) * tile_width;
		src_y++;

		if (lx) {
			to_memcpy_sse2(dst_row + ox, src_row + ox, lx);
			dst_row += tile_size;
			src_row += tile_size;
			w -= lx;
		}
		while (w >= tile_width) {
			assert(((uintptr_t)dst_row & (tile_width - 1)) == 0);
			assert(((uintptr_t)src_row & (tile_width - 1)) == 0);
			to_sse128xN(assume_aligned(dst_row, tile_width),
				    assume_aligned(src_row, tile_width),
				    tile_width);
			dst_row += tile_size;
			src_row += tile_size;
			w -= tile_width;
		}
		if (w) {
			assert(((uintptr_t)dst_row & (tile_width - 1)) == 0);
			assert(((uintptr_t)src_row & (tile_width - 1)) == 0);
			to_memcpy_sse2(assume_aligned(dst_row, tile_width),
				  assume_aligned(src_row, tile_width),
				  w);
		}
	}
}

#pragma GCC pop_options
#endif