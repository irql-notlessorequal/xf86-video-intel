#if defined(sse4_2)

#pragma GCC push_options
#pragma GCC target("inline-all-stringops")
#pragma GCC optimize("Ofast")

#if !defined(__x86_64__)
static __attribute__((always_inline)) inline void *do_memcpy(void *dest, const void *src, size_t n)
{
	int d0, d1, d2;
	asm volatile(
		"rep ; movsl\n\t"
		"movl %4,%%ecx\n\t"
		"rep ; movsb\n\t"
		: "=&c" (d0), "=&D" (d1), "=&S" (d2)
		: "0" (n >> 2), "g" (n & 3), "1" (dest), "2" (src)
		: "memory");

	return dest;
}
#else
static __attribute__((always_inline)) inline void *do_memcpy(void *dest, const void *src, size_t n)
{
	long d0, d1, d2;
	asm volatile(
		"rep ; movsq\n\t"
		"movq %4,%%rcx\n\t"
		"rep ; movsb\n\t"
		: "=&c" (d0), "=&D" (d1), "=&S" (d2)
		: "0" (n >> 3), "g" (n & 7), "1" (dest), "2" (src)
		: "memory");

	return dest;
}
#endif


static void
memcpy_to_tiled_x__swizzle_0__modern(const void *src, void *dst, int bpp,
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
				do_memcpy(assume_misaligned(tile_row + x, tile_width, x),
				       src, len);

				tile_row += tile_size;
				src = (const uint8_t *)src + len;
				w -= len;
			}
		}

		while (w >= tile_width)
		{
			do_memcpy(tile_row, src, tile_width);
			tile_row += tile_size;
			src = (const uint8_t *)src + tile_width;
			w -= tile_width;
		}

		do_memcpy(tile_row, src, w);
		src = (const uint8_t *)src + src_stride + w;
		dst_y++;
	}
}

static void
memcpy_from_tiled_x__swizzle_0__modern(const void *src, void *dst, int bpp,
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
				do_memcpy(dst, assume_misaligned(tile_row + x, tile_width, x), len);

				tile_row += tile_size;
				dst = (uint8_t *)dst + len;
				w -= len;
			}

		}

		while (w >= tile_width)
		{
			do_memcpy(dst, tile_row, tile_width);

			tile_row += tile_size;
			dst = (uint8_t *)dst + tile_width;
			w -= tile_width;
		}

		do_memcpy(dst, tile_row, w);
		dst = (uint8_t *)dst + dst_stride + w;
		src_y++;
	}
}

static void
memcpy_between_tiled_x__swizzle_0__modern(const void *src, void *dst, int bpp,
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

			do_memcpy(assume_misaligned(dst_row + x, tile_width, x),
			       assume_misaligned(src_row + x, tile_width, x),
			       len);

			dst_row += tile_size;
			src_row += tile_size;
			w -= len;
		}

		while (w >= tile_width)
		{
			do_memcpy(dst_row, src_row, tile_width);

			dst_row += tile_size;
			src_row += tile_size;
			w -= tile_width;
		}

		do_memcpy(dst_row, src_row, w);
	}
}

#pragma GCC pop_options
#endif