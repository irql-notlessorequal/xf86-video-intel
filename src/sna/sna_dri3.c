/*
 * Copyright (c) 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <xf86drm.h>

#include "sna.h"

#include <xf86.h>
#include <dri3.h>
#include <misyncshm.h>
#include <misyncstr.h>
#include <drm_fourcc.h>

static DevPrivateKeyRec sna_sync_fence_private_key;
struct sna_sync_fence {
	SyncFenceSetTriggeredFunc set_triggered;
};

static inline struct sna_sync_fence *sna_sync_fence(SyncFence *fence)
{
	return dixLookupPrivate(&fence->devPrivates, &sna_sync_fence_private_key);
}

static inline void mark_dri3_pixmap(struct sna *sna, struct sna_pixmap *priv, struct kgem_bo *bo)
{
	bo->flush = true;
	if (bo->exec)
		sna->kgem.flush = 1;
	if (bo == priv->gpu_bo)
		priv->flush |= FLUSH_READ | FLUSH_WRITE;
	else
		priv->shm = true;

	sna_watch_flush(sna, 1);

	kgem_bo_submit(&sna->kgem, bo);
	kgem_bo_unclean(&sna->kgem, bo);
}

static void sna_sync_flush(struct sna *sna, struct sna_pixmap *priv)
{
	struct kgem_bo *bo = NULL;

	DBG(("%s(pixmap=%ld)\n", __FUNCTION__, priv->pixmap->drawable.serialNumber));
	assert(priv);

	if (priv->pinned & PIN_DRI3) {
		assert(priv->gpu_bo);
		assert(priv->pinned & PIN_DRI3);
		DBG(("%s: flushing prime GPU bo, handle=%ld\n", __FUNCTION__, priv->gpu_bo->handle));
		if (sna_pixmap_move_to_gpu(priv->pixmap, MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT | __MOVE_FORCE)) {
			sna_damage_all(&priv->gpu_damage, priv->pixmap);
			bo = priv->gpu_bo;
		}
	} else {
		assert(priv->cpu_bo);
		assert(IS_STATIC_PTR(priv->ptr));
		DBG(("%s: flushing prime CPU bo, handle=%ld\n", __FUNCTION__, priv->cpu_bo->handle));
		if (sna_pixmap_move_to_cpu(priv->pixmap, MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT))
			bo = priv->cpu_bo;
	}

	if (bo != NULL) {
		kgem_bo_submit(&sna->kgem, bo);
		kgem_bo_unclean(&sna->kgem, bo);
	}
}

static void
sna_sync_fence_set_triggered(SyncFence *fence)
{
	struct sna *sna = to_sna_from_screen(fence->pScreen);
	struct sna_sync_fence *sna_fence = sna_sync_fence(fence);

	DBG(("%s()\n", __FUNCTION__));
	sna_accel_flush(sna);

	fence->funcs.SetTriggered = sna_fence->set_triggered;
	sna_fence->set_triggered(fence);
	sna_fence->set_triggered = fence->funcs.SetTriggered;
	fence->funcs.SetTriggered = sna_sync_fence_set_triggered;
}

static void
sna_sync_create_fence(ScreenPtr screen, SyncFence *fence, Bool initially_triggered)
{
	struct sna *sna = to_sna_from_screen(screen);
	SyncScreenFuncsPtr funcs = miSyncGetScreenFuncs(screen);

	DBG(("%s()\n", __FUNCTION__));

	funcs->CreateFence = sna->dri3.create_fence;
	sna->dri3.create_fence(screen, fence, initially_triggered);
	sna->dri3.create_fence = funcs->CreateFence;
	funcs->CreateFence = sna_sync_create_fence;

	sna_sync_fence(fence)->set_triggered = fence->funcs.SetTriggered;
	fence->funcs.SetTriggered = sna_sync_fence_set_triggered;
}

static bool
sna_sync_open(struct sna *sna, ScreenPtr screen)
{
	SyncScreenFuncsPtr funcs;

	DBG(("%s()\n", __FUNCTION__));

	if (!miSyncShmScreenInit(screen))
		return false;

	if (!dixPrivateKeyRegistered(&sna_sync_fence_private_key)) {
		if (!dixRegisterPrivateKey(&sna_sync_fence_private_key,
					   PRIVATE_SYNC_FENCE,
					   sizeof(struct sna_sync_fence)))
			return false;
	}

	funcs = miSyncGetScreenFuncs(screen);
	sna->dri3.create_fence = funcs->CreateFence;
	funcs->CreateFence = sna_sync_create_fence;

	return true;
}

static int sna_dri3_open_device(ScreenPtr screen,
								RRProviderPtr provider,
								int *out)
{
	int fd;

	DBG(("%s()\n", __FUNCTION__));
	fd = intel_get_client_fd(to_sna_from_screen(screen)->dev);
	if (fd < 0)
		return -fd;

	*out = fd;
	return Success;
}

static PixmapPtr sna_dri3_pixmap_from_fd(ScreenPtr screen,
										 int fd,
										 CARD16 width,
										 CARD16 height,
										 CARD16 stride,
										 CARD8 depth,
										 CARD8 bpp)
{
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	struct kgem_bo *bo;
	int flags = 0;

	DBG(("%s(fd=%d, width=%d, height=%d, stride=%d, depth=%d, bpp=%d)\n",
	     __FUNCTION__, fd, width, height, stride, depth, bpp));
	if (width > INT16_MAX || height > INT16_MAX)
		return NULL;

	if ((uint32_t)width * bpp > (uint32_t)stride * 8)
		return NULL;

	if (depth < 8)
		return NULL;

	switch (bpp) {
	case 8:
	case 16:
	case 32:
		break;
	default:
		return NULL;
	}

	bo = kgem_create_for_prime(&sna->kgem, fd, (uint32_t)stride * height);
	if (bo == NULL)
		return NULL;

	/* Check for a duplicate */
	list_for_each_entry(priv, &sna->dri3.pixmaps, cow_list) {
		int other_stride = 0;
		if (bo->snoop) {
			assert(priv->cpu_bo);
			assert(IS_STATIC_PTR(priv->ptr));
			if (bo->handle == priv->cpu_bo->handle)
				other_stride = priv->cpu_bo->pitch;
		} else  {
			assert(priv->gpu_bo);
			assert(priv->pinned & PIN_DRI3);
			if (bo->handle == priv->gpu_bo->handle)
				other_stride = priv->gpu_bo->pitch;
		}
		if (other_stride) {
			pixmap = priv->pixmap;
			DBG(("%s: imported fd matches existing DRI3 pixmap=%ld\n", __FUNCTION__, pixmap->drawable.serialNumber));
			bo->handle = 0; /* fudge to prevent gem_close */
			kgem_bo_destroy(&sna->kgem, bo);
			if (width != pixmap->drawable.width ||
			    height != pixmap->drawable.height ||
			    depth != pixmap->drawable.depth ||
			    bpp != pixmap->drawable.bitsPerPixel ||
			    stride != other_stride) {
				DBG(("%s: imported fd mismatches existing DRI3 pixmap (width=%d, height=%d, depth=%d, bpp=%d, stride=%d)\n", __FUNCTION__,
				     pixmap->drawable.width,
				     pixmap->drawable.height,
				     pixmap->drawable.depth,
				     pixmap->drawable.bitsPerPixel,
				     other_stride));
				return NULL;
			}
			sna_sync_flush(sna, priv);
			pixmap->refcnt++;
			return pixmap;
		}
	}

	if (!kgem_check_surface_size(&sna->kgem,
				     width, height, bpp,
				     bo->tiling, stride, kgem_bo_size(bo))) {
		DBG(("%s: client supplied pitch=%d, size=%d too small for %dx%d surface\n",
		     __FUNCTION__, stride, kgem_bo_size(bo), width, height));
		goto free_bo;
	}

	/**
	 * If it's linear, likely it's from an external source.
	 */
	if (bo->tiling == I915_TILING_NONE)
		flags = SNA_PIXMAP_USAGE_DRI3_IMPORT;	

	pixmap = sna_pixmap_create_unattached_with_hint(screen, width, height, depth, flags);
	if (pixmap == NullPixmap)
		goto free_bo;

	if (!screen->ModifyPixmapHeader(pixmap, width, height,
					depth, bpp, stride, NULL))
		goto free_pixmap;

	priv = sna_pixmap_attach_to_bo(pixmap, bo);
	if (priv == NULL)
		goto free_pixmap;

	bo->pitch = stride;
	priv->stride = stride;

	if (bo->snoop) {
		assert(priv->cpu_bo == bo);
		pixmap->devPrivate.ptr = kgem_bo_map__cpu(&sna->kgem, priv->cpu_bo);
		if (pixmap->devPrivate.ptr == NULL)
			goto free_pixmap;

		pixmap->devKind = stride;
		priv->ptr = MAKE_STATIC_PTR(pixmap->devPrivate.ptr);
	} else {
		assert(priv->gpu_bo == bo);
		priv->create = kgem_can_create_2d(&sna->kgem,
						  width, height, depth);
		priv->pinned |= PIN_DRI3;
	}
	list_add(&priv->cow_list, &sna->dri3.pixmaps);

	mark_dri3_pixmap(sna, priv, bo);

	return pixmap;

free_pixmap:
	screen->DestroyPixmap(pixmap);
free_bo:
	kgem_bo_destroy(&sna->kgem, bo);
	return NULL;
}

static int sna_dri3_fd_from_pixmap(ScreenPtr screen,
								   PixmapPtr pixmap,
								   CARD16 *stride,
								   CARD32 *size)
{
	struct sna *sna = to_sna_from_screen(screen);
	struct sna_pixmap *priv;
	struct kgem_bo *bo = NULL;
	int fd;

	DBG(("%s(pixmap=%ld, width=%d, height=%d)\n", __FUNCTION__,
	     pixmap->drawable.serialNumber, pixmap->drawable.width, pixmap->drawable.height));
	if (pixmap == sna->front && sna->flags & SNA_TEAR_FREE) {
		DBG(("%s: DRI3 protocol cannot support TearFree frontbuffers\n", __FUNCTION__));
		return -1;
	}

	priv = sna_pixmap(pixmap);
	if (priv && IS_STATIC_PTR(priv->ptr) && priv->cpu_bo) {
		if (sna_pixmap_move_to_cpu(pixmap, MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT))
			bo = priv->cpu_bo;
	} else {
		priv = sna_pixmap_move_to_gpu(pixmap, MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT | __MOVE_FORCE | __MOVE_DRI);
		if (priv != NULL) {
			sna_damage_all(&priv->gpu_damage, pixmap);
			bo = priv->gpu_bo;
		}
	}
	if (bo == NULL) {
		DBG(("%s: pixmap not supported by GPU\n", __FUNCTION__));
		return -1;
	}
	assert(priv != NULL);

	if (bo->pitch > UINT16_MAX) {
		DBG(("%s: pixmap pitch (%d) too large for DRI3 protocol\n",
		     __FUNCTION__, bo->pitch));
		return -1;
	}

	if (bo->tiling && !sna->kgem.can_fence) {
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_NONE)) {
			DBG(("%s: unable to discard GPU tiling (%d) for DRI3 protocol\n",
			     __FUNCTION__, bo->tiling));
			return -1;
		}
		bo = priv->gpu_bo;
	}

	fd = kgem_bo_export_to_prime(&sna->kgem, bo);
	if (fd == -1) {
		DBG(("%s: exporting handle=%d to fd failed\n", __FUNCTION__, bo->handle));
		return -1;
	}

	if (bo == priv->gpu_bo)
		priv->pinned |= PIN_DRI3;
	list_move(&priv->cow_list, &sna->dri3.pixmaps);

	mark_dri3_pixmap(sna, priv, bo);

	*stride = (priv->pinned & PIN_DRI3) ? priv->gpu_bo->pitch : priv->cpu_bo->pitch;
	*size = kgem_bo_size((priv->pinned & PIN_DRI3) ? priv->gpu_bo : priv->cpu_bo);
	DBG(("%s: exporting %s pixmap=%ld, handle=%d, stride=%d, size=%d\n",
	     __FUNCTION__,
	     (priv->pinned & PIN_DRI3) ? "GPU" : "CPU", pixmap->drawable.serialNumber,
	     (priv->pinned & PIN_DRI3) ? priv->gpu_bo->handle : priv->cpu_bo->handle,
	     *stride, *size));
	return fd;
}

#if DRI3_SCREEN_INFO_VERSION >= 2
static force_inline
uint64_t kgem_bo_modifier(struct kgem_bo *bo)
{
	switch (bo->tiling) {
	case I915_TILING_X: return I915_FORMAT_MOD_X_TILED;
	case I915_TILING_Y: return I915_FORMAT_MOD_Y_TILED;
	default:            return DRM_FORMAT_MOD_LINEAR;
	}
}

static force_inline
int modifier_to_tiling(uint64_t modifier)
{
	switch (modifier) {
	case I915_FORMAT_MOD_X_TILED: return I915_TILING_X;
	case I915_FORMAT_MOD_Y_TILED: return I915_TILING_Y;
	default:                      return I915_TILING_NONE;
	}
}

static int sna_dri3_fds_from_pixmap(ScreenPtr screen,
                                    PixmapPtr pixmap,
                                    int *fds,
                                    uint32_t *strides,
                                    uint32_t *offsets,
                                    uint64_t *modifier)
{
	struct sna *sna = to_sna_from_screen(screen);
	struct sna_pixmap *priv;
	struct kgem_bo *bo = NULL;
	int fd;

	DBG(("%s(pixmap=%ld, width=%d, height=%d)\n", __FUNCTION__,
	     pixmap->drawable.serialNumber,
	     pixmap->drawable.width, pixmap->drawable.height));

	if (pixmap == sna->front && sna->flags & SNA_TEAR_FREE) {
		DBG(("%s: DRI3 protocol cannot support TearFree frontbuffers\n", __FUNCTION__));
		return 0;
	}

	priv = sna_pixmap(pixmap);
	if (priv && IS_STATIC_PTR(priv->ptr) && priv->cpu_bo) {
		if (sna_pixmap_move_to_cpu(pixmap, MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT))
			bo = priv->cpu_bo;
	} else {
		priv = sna_pixmap_move_to_gpu(pixmap,
		                              MOVE_READ | MOVE_WRITE | MOVE_ASYNC_HINT |
		                              __MOVE_FORCE | __MOVE_DRI);
		if (priv != NULL) {
			sna_damage_all(&priv->gpu_damage, pixmap);
			bo = priv->gpu_bo;
		}
	}
	if (bo == NULL) {
		DBG(("%s: pixmap not supported by GPU\n", __FUNCTION__));
		return 0;
	}
	assert(priv != NULL);

	if (bo->pitch > UINT16_MAX) {
		DBG(("%s: pixmap pitch (%d) too large for DRI3 protocol\n",
		     __FUNCTION__, bo->pitch));
		return 0;
	}

	/*
	 * Tiled BOs require fencing for CPU access, but if the hardware can't
	 * fence, strip tiling before export so the importer can safely access it.
	 * Note: with an explicit modifier we could leave tiling intact since the
	 * importer knows what it's getting — but we stay conservative here.
	 */
	if (bo->tiling && !sna->kgem.can_fence) {
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_NONE)) {
			DBG(("%s: unable to discard GPU tiling (%d) for DRI3 protocol\n",
			     __FUNCTION__, bo->tiling));
			return 0;
		}
		bo = priv->gpu_bo;
	}

	fd = kgem_bo_export_to_prime(&sna->kgem, bo);
	if (fd == -1) {
		DBG(("%s: exporting handle=%d to fd failed\n", __FUNCTION__, bo->handle));
		return 0;
	}

	if (bo == priv->gpu_bo)
		priv->pinned |= PIN_DRI3;
	list_move(&priv->cow_list, &sna->dri3.pixmaps);

	mark_dri3_pixmap(sna, priv, bo);

	fds[0]     = fd;
	strides[0] = (priv->pinned & PIN_DRI3) ? priv->gpu_bo->pitch : priv->cpu_bo->pitch;
	offsets[0] = 0;
	*modifier  = kgem_bo_modifier(bo);

	DBG(("%s: exporting %s pixmap=%ld, handle=%d, stride=%d, modifier=0x%llx\n",
	     __FUNCTION__,
	     (priv->pinned & PIN_DRI3) ? "GPU" : "CPU",
	     pixmap->drawable.serialNumber,
	     (priv->pinned & PIN_DRI3) ? priv->gpu_bo->handle : priv->cpu_bo->handle,
	     strides[0], (long long)*modifier));
	return 1;
}

static PixmapPtr sna_dri3_pixmap_from_fds(ScreenPtr screen,
                                           CARD8 num_fds,
                                           const int *fds,
                                           CARD16 width,
                                           CARD16 height,
                                           const uint32_t *strides,
                                           const uint32_t *offsets,
                                           CARD8 depth,
                                           CARD8 bpp,
                                           uint64_t modifier)
{
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	struct kgem_bo *bo;
	int tiling;

	DBG(("%s(num_fds=%d, width=%d, height=%d, depth=%d, bpp=%d, modifier=0x%llx)\n",
	     __FUNCTION__, num_fds, width, height, depth, bpp, (long long)modifier));

	if (num_fds != 1) {
		DBG(("%s: unsupported num_fds=%d\n", __FUNCTION__, num_fds));
		return NULL;
	}

	if (offsets[0] != 0) {
		DBG(("%s: unsupported non-zero offset=%u\n", __FUNCTION__, offsets[0]));
		return NULL;
	}

	switch (modifier) {
	case DRM_FORMAT_MOD_INVALID:
	case DRM_FORMAT_MOD_LINEAR:
	case I915_FORMAT_MOD_X_TILED:
	case I915_FORMAT_MOD_Y_TILED:
		break;
	default:
		DBG(("%s: unsupported modifier 0x%llx\n", __FUNCTION__, (long long)modifier));
		return NULL;
	}

	if (width > INT16_MAX || height > INT16_MAX)
		return NULL;
	if ((uint32_t)width * bpp > (uint32_t)strides[0] * 8)
		return NULL;
	if (depth < 8)
		return NULL;
	switch (bpp) {
	case 8: case 16: case 32: break;
	default: return NULL;
	}

	bo = kgem_create_for_prime(&sna->kgem, fds[0], (uint32_t)strides[0] * height);
	if (bo == NULL)
		return NULL;

	if (modifier != DRM_FORMAT_MOD_INVALID) {
		tiling = modifier_to_tiling(modifier);
		if (bo->tiling != tiling) {
			DBG(("%s: overriding tiling %d -> %d from modifier 0x%llx\n",
			     __FUNCTION__, bo->tiling, tiling, (long long)modifier));
			bo->tiling = tiling;
		}
	}

	/* Check for a duplicate, same as pixmap_from_fd */
	list_for_each_entry(priv, &sna->dri3.pixmaps, cow_list) {
		int other_stride = 0;
		if (bo->snoop) {
			assert(priv->cpu_bo);
			assert(IS_STATIC_PTR(priv->ptr));
			if (bo->handle == priv->cpu_bo->handle)
				other_stride = priv->cpu_bo->pitch;
		} else {
			assert(priv->gpu_bo);
			assert(priv->pinned & PIN_DRI3);
			if (bo->handle == priv->gpu_bo->handle)
				other_stride = priv->gpu_bo->pitch;
		}
		if (other_stride) {
			pixmap = priv->pixmap;
			DBG(("%s: imported fds match existing DRI3 pixmap=%ld\n",
			     __FUNCTION__, pixmap->drawable.serialNumber));
			bo->handle = 0;
			kgem_bo_destroy(&sna->kgem, bo);
			if (width  != pixmap->drawable.width  ||
			    height != pixmap->drawable.height  ||
			    depth  != pixmap->drawable.depth   ||
			    bpp    != pixmap->drawable.bitsPerPixel ||
			    (uint32_t)strides[0] != (uint32_t)other_stride) {
				DBG(("%s: imported fds mismatch existing DRI3 pixmap\n", __FUNCTION__));
				return NULL;
			}
			sna_sync_flush(sna, priv);
			pixmap->refcnt++;
			return pixmap;
		}
	}

	if (!kgem_check_surface_size(&sna->kgem,
				     width, height, bpp,
				     bo->tiling, strides[0], kgem_bo_size(bo))) {
		DBG(("%s: surface size check failed\n", __FUNCTION__));
		goto free_bo;
	}

	/*
	 * Only mark as a foreign import (disabling GPU rendering optimisations)
	 * when the buffer is genuinely linear — tiled buffers from Vulkan are
	 * perfectly renderable by the GPU and should be treated as such.
	 */
	pixmap = sna_pixmap_create_unattached_with_hint(screen, width, height, depth,
	                                                bo->tiling == I915_TILING_NONE
	                                                    ? SNA_PIXMAP_USAGE_DRI3_IMPORT
	                                                    : 0);
	if (pixmap == NullPixmap)
		goto free_bo;

	if (!screen->ModifyPixmapHeader(pixmap, width, height, depth, bpp, strides[0], NULL))
		goto free_pixmap;

	priv = sna_pixmap_attach_to_bo(pixmap, bo);
	if (priv == NULL)
		goto free_pixmap;

	bo->pitch = strides[0];
	priv->stride = strides[0];

	if (bo->snoop) {
		assert(priv->cpu_bo == bo);
		pixmap->devPrivate.ptr = kgem_bo_map__cpu(&sna->kgem, priv->cpu_bo);
		if (pixmap->devPrivate.ptr == NULL)
			goto free_pixmap;
		pixmap->devKind = strides[0];
		priv->ptr = MAKE_STATIC_PTR(pixmap->devPrivate.ptr);
	} else {
		assert(priv->gpu_bo == bo);
		priv->create = kgem_can_create_2d(&sna->kgem, width, height, depth);
		priv->pinned |= PIN_DRI3;
	}
	list_add(&priv->cow_list, &sna->dri3.pixmaps);

	mark_dri3_pixmap(sna, priv, bo);

	return pixmap;

free_pixmap:
	screen->DestroyPixmap(pixmap);
free_bo:
	kgem_bo_destroy(&sna->kgem, bo);
	return NULL;
}

static int sna_dri3_get_formats(ScreenPtr screen,
								CARD32 *num_formats,
								CARD32 **formats)
{
	struct sna *sna = to_sna_from_screen(screen);
	if (!sna)
		goto bail;

	const struct intel_device_info* info = sna->info;
	if (!info->formats)
		goto bail;

	*num_formats = info->formats;
	*formats = calloc(info->formats, sizeof(CARD32));
	if (!*formats)
		goto bail;

	for (size_t index = 0; index < info->formats; index++)
		(*formats)[index] = info->format_info[index].format;

	return TRUE;
bail:
    *num_formats = 0;
    return TRUE;
}

static int sna_dri3_get_modifiers(ScreenPtr screen,
								  uint32_t format,
								  uint32_t *num_modifiers,
								  uint64_t **modifiers)
{
	size_t count = 0;
	struct sna *sna = to_sna_from_screen(screen);
	if (!sna)
		goto fail;

	const struct intel_device_info* info = sna->info;
	if (!info->formats)
		goto fail;

	const struct intel_format_info* format_info = get_format(info, format);
	if (!format_info)
		goto fail;
	
	/* Count the amount of modifiers available. */
	while (format_info->modifiers[count] != 0)
		count++;

	*num_modifiers = count;
	*modifiers = malloc(count * sizeof(uint64_t));
	if (!*modifiers)
		goto bail;

	memcpy(*modifiers, format_info->modifiers, count * sizeof(uint64_t));
	return TRUE;
bail:
    *num_modifiers = 0;
    return TRUE;
fail:
	*num_modifiers = 0;
	return FALSE;
}

static int sna_dri3_get_drawable_modifiers(DrawablePtr drawable,
										   uint32_t format,
										   uint32_t *num_modifiers,
										   uint64_t **modifiers)
{
	return sna_dri3_get_modifiers(drawable->pScreen, format,
								  num_modifiers, modifiers);
}
#endif

static dri3_screen_info_rec sna_dri3_info = {
	.version = DRI3_SCREEN_INFO_VERSION,

	.open = sna_dri3_open_device,
	.pixmap_from_fd = sna_dri3_pixmap_from_fd,
	.fd_from_pixmap = sna_dri3_fd_from_pixmap,

#if DRI3_SCREEN_INFO_VERSION >= 2
	.pixmap_from_fds = sna_dri3_pixmap_from_fds,
	.fds_from_pixmap = sna_dri3_fds_from_pixmap,
	.get_formats = sna_dri3_get_formats,
	.get_modifiers = sna_dri3_get_modifiers,
	.get_drawable_modifiers = sna_dri3_get_drawable_modifiers,
#endif
};

bool sna_dri3_open(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s()\n", __FUNCTION__));

	if (!sna_sync_open(sna, screen)) {
		return false;
	} else {
		list_init(&sna->dri3.pixmaps);
		return dri3_screen_init(screen, &sna_dri3_info);
	}
}

void sna_dri3_close(struct sna *sna, ScreenPtr screen)
{
	SyncScreenFuncsPtr funcs;

	DBG(("%s()\n", __FUNCTION__));

	funcs = miSyncGetScreenFuncs(screen);
	if (funcs)
		funcs->CreateFence = sna->dri3.create_fence;
}
