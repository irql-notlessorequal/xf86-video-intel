/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>
#include <xf86drm.h>

#include "sna.h"

#include <xf86.h>
#include <present.h>

static present_screen_info_rec present_info;

struct sna_present_event
{
	uint64_t		event_id;

	Bool			unflip;
	Bool			is_fake;
};

static Bool
sna_present_event_match(void *data, void *match_data)
{
    struct sna_present_event *event = data;
    uint64_t *match = match_data;

    return *match == event->event_id;
}

static void sna_present_unflip(ScreenPtr screen, uint64_t event_id);
static bool sna_present_queue(struct sna_present_event *info,
			      uint64_t last_msc);

static struct sna_present_event *info_alloc(struct sna *sna)
{
	struct sna_present_event *info;

	info = sna->present.freed_info;
	if (info) {
		sna->present.freed_info = NULL;
		return info;
	}

	return malloc(sizeof(struct sna_present_event) + sizeof(uint64_t));
}

static inline bool msc_before(uint64_t msc, uint64_t target)
{
	return (int64_t)(msc - target) < 0;
}

#define MARK_PRESENT(x) ((void *)((uintptr_t)(x) | 2))

static inline xf86CrtcPtr unmask_crtc(xf86CrtcPtr crtc)
{
	return (xf86CrtcPtr)((uintptr_t)crtc & ~1);
}

static inline xf86CrtcPtr mark_crtc(xf86CrtcPtr crtc)
{
	return (xf86CrtcPtr)((uintptr_t)crtc | 1);
}

static inline bool has_vblank(xf86CrtcPtr crtc)
{
	return (uintptr_t)crtc & 1;
}

static inline int crtc_index_from_crtc(RRCrtcPtr crtc)
{
	return crtc ? sna_crtc_index(crtc->devPrivate) : -1;
}

static uint32_t crtc_select(int index)
{
	if (index > 1)
		return index << DRM_VBLANK_HIGH_CRTC_SHIFT;
	else if (index > 0)
		return DRM_VBLANK_SECONDARY;
	else
		return 0;
}

static inline int sna_wait_vblank(struct sna *sna, union drm_wait_vblank *vbl, int index)
{
	DBG(("%s(crtc=%d, waiting until seq=%u%s)\n",
	     __FUNCTION__, index, vbl->request.sequence,
	     vbl->request.type & DRM_VBLANK_RELATIVE ? " [relative]" : ""));
	vbl->request.type |= crtc_select(index);
	return drmIoctl(sna->kgem.fd, DRM_IOCTL_WAIT_VBLANK, vbl);
}

static uint64_t gettime_ust64(void)
{
	struct timespec tv;

	/**
	 * modesetting uses coarse timing to reduce overhead, do that here too.
	 */

#ifdef __linux__
	if (clock_gettime(CLOCK_MONOTONIC_COARSE, &tv))
#elif __FreeBSD__
	if (clock_gettime(CLOCK_MONOTONIC_FAST, &tv))
#else
	if (clock_gettime(CLOCK_MONOTONIC, &tv))
#endif
		return GetTimeInMicros();

	return ust64(tv.tv_sec, tv.tv_nsec / 1000);
}

static uint32_t msc_to_delay(xf86CrtcPtr crtc, uint64_t target)
{
	const DisplayModeRec *mode = &crtc->desiredMode;
	const struct ust_msc *swap = sna_crtc_last_swap(crtc);
	int64_t delay, subframe;

	assert(mode->Clock);

	delay = target - swap->msc;
	assert(delay >= 0);
	if (delay > 1) { /* try to use the hw vblank for the last frame */
		delay--;
		subframe = 0;
	} else {
		subframe = gettime_ust64() - swap_ust(swap);
		subframe += 500;
		subframe /= 1000;
	}
	delay *= mode->VTotal * mode->HTotal / mode->Clock;
	if (subframe < delay)
		delay -= subframe;
	else
		delay = 0;

	DBG(("%s: sleep %d frames, %llu ms\n", __FUNCTION__,
	     (int)(target - swap->msc), (long long)delay));
	assert(delay >= 0);
	return MIN(delay, INT32_MAX);
}

static bool sna_present_queue(struct sna_present_event *info,
			      uint64_t last_msc)
{
	union drm_wait_vblank vbl;
	int delta = info->target_msc - last_msc;

	DBG(("%s: target msc=%llu, seq=%u (last_msc=%llu), delta=%d\n",
	     __FUNCTION__,
	     (long long)info->target_msc,
	     (unsigned)info->target_msc,
	     (long long)last_msc,
	     delta));
	assert(info->target_msc - last_msc < 1ull<<31);
	assert(delta >= 0);

	VG_CLEAR(vbl);
	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	vbl.request.sequence = info->target_msc;
	vbl.request.signal = (uintptr_t)MARK_PRESENT(info);
	if (delta > 2 ||
	    sna_wait_vblank(info->sna, &vbl, sna_crtc_index(info->crtc))) {
		DBG(("%s: vblank enqueue failed, faking delta=%d\n", __FUNCTION__, delta));
		if (!sna_fake_vblank(info))
			return false;
	} else {
		add_to_crtc_vblank(info, delta);
	}

	info->queued = true;
	return true;
}

static RRCrtcPtr
sna_present_get_crtc(WindowPtr window)
{
	struct sna *sna = to_sna_from_drawable(&window->drawable);
	BoxRec box;
	xf86CrtcPtr crtc;

	DBG(("%s: window=%ld (pixmap=%ld usage=%u), box=(%d, %d)x(%d, %d)\n",
	     __FUNCTION__, window->drawable.id, 
		 get_window_pixmap(window)->drawable.serialNumber,
		 get_window_pixmap(window)->usage_hint,
	     window->drawable.x, window->drawable.y,
	     window->drawable.width, window->drawable.height));

	box.x1 = window->drawable.x;
	box.y1 = window->drawable.y;
	box.x2 = box.x1 + window->drawable.width;
	box.y2 = box.y1 + window->drawable.height;

	crtc = sna_covering_crtc(sna, &box, NULL);
	if (crtc)
		return crtc->randr_crtc;

	return NULL;
}

static int
sna_present_get_ust_msc(RRCrtcPtr crtc, CARD64 *ust, CARD64 *msc)
{
	struct sna *sna = to_sna_from_screen(crtc->pScreen);
	union drm_wait_vblank vbl;

	DBG(("%s(crtc=%d)\n", __FUNCTION__, sna_crtc_index(crtc->devPrivate)));
	if (sna_crtc_has_vblank(crtc->devPrivate)) {
		DBG(("%s: vblank active, reusing last swap msc/ust\n",
		     __FUNCTION__));
		goto last;
	}

	VG_CLEAR(vbl);
	vbl.request.type = (uint32_t)DRM_VBLANK_RELATIVE;
	vbl.request.sequence = 0;
	if (sna_wait_vblank(sna, &vbl, sna_crtc_index(crtc->devPrivate)) == 0) {
		*ust = ust64(vbl.reply.tval_sec, vbl.reply.tval_usec);
		*msc = sna_crtc_record_vblank(crtc->devPrivate, &vbl);

		add_keepalive(sna, crtc->devPrivate, *msc + 1);
	} else {
		const struct ust_msc *swap;
last:
		swap = sna_crtc_last_swap(crtc->devPrivate);
		*ust = swap_ust(swap);
		*msc = swap->msc;
	}

	DBG(("%s: crtc=%d, tv=%d.%06d seq=%d msc=%lld\n", __FUNCTION__,
	     sna_crtc_index(crtc->devPrivate),
	     (int)(*ust / 1000000), (int)(*ust % 1000000),
	     vbl.reply.sequence, (long long)*msc));

	return Success;
}

static void
sna_present_vblank_handler(uint64_t msc, uint64_t usec, void *data)
{
	struct sna_present_event *info = data;

	present_event_notify(info->event_id, usec, msc);
}

static void
sna_present_vblank_abort(void *data)
{
	struct sna_present_event *info = data;

	free(info);
}

static int
sna_present_queue_vblank(RRCrtcPtr crtc, uint64_t event_id, uint64_t msc)
{
	struct sna *sna = to_sna_from_screen(crtc->pScreen);
	struct sna_present_event *event;
	uint32_t seq;

	if (!sna_crtc_is_on(crtc->devPrivate))
		return BadAlloc;

	event = calloc(1, sizeof(struct sna_present_event));
	if (!event)
		return BadAlloc;

	event->event_id = event_id;

	seq = ms_drm_queue_alloc(crtc->devPrivate, event,
							 sna_present_vblank_handler,
							 sna_present_vblank_abort);
	if (!seq)
	{
		free(event);
		return BadAlloc;
	}

	return Success;
}

static void
sna_present_abort_vblank(RRCrtcPtr crtc, uint64_t event_id, uint64_t msc)
{
	DBG(("%s(crtc=%d, event=%lld, msc=%lld)\n",
	     __FUNCTION__, crtc_index_from_crtc(crtc),
	     (long long)event_id, (long long)msc));
}

static void
sna_present_flush(WindowPtr window)
{
	ScreenPtr screen = window->drawable.pScreen;
	struct sna *sna = to_sna_from_screen(screen);

	sna_accel_flush(sna);
}

static bool
check_flip__crtc(struct sna *sna,
		 RRCrtcPtr crtc)
{
	if (!sna_crtc_is_on(crtc->devPrivate)) {
		DBG(("%s: CRTC off\n", __FUNCTION__));
		return false;
	}

	assert(sna->scrn->vtSema);

	if (!sna->mode.front_active) {
		DBG(("%s: DPMS off, no flips\n", __FUNCTION__));
		return FALSE;
	}

	if (sna->mode.rr_active) {
		DBG(("%s: RandR transformation active\n", __FUNCTION__));
		return false;
	}

	return true;
}

static Bool sna_present_check_flip2(RRCrtcPtr crtc, WindowPtr window, PixmapPtr pixmap, Bool sync_flip, PresentFlipReason *reason)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *flip;

	DBG(("%s(crtc=%d, pixmap=%ld, usage=%u, sync_flip=%d)\n",
	     __FUNCTION__,
	     crtc_index_from_crtc(crtc),
	     pixmap->drawable.serialNumber,
		 pixmap->usage_hint,
	     sync_flip));

	if (!sna->scrn->vtSema) {
		DBG(("%s: VT switched away, no flips\n", __FUNCTION__));
		return FALSE;
	}

	if (sna->flags & SNA_NO_FLIP) {
		DBG(("%s: flips not suported\n", __FUNCTION__));
		return FALSE;
	}

	if (sync_flip) {
		if ((sna->flags & SNA_HAS_FLIP) == 0) {
			DBG(("%s: sync flips not suported\n", __FUNCTION__));
			return FALSE;
		}
	} else {
		if ((sna->flags & SNA_HAS_ASYNC_FLIP) == 0) {
			DBG(("%s: async flips not suported\n", __FUNCTION__));
			return FALSE;
		}
	}

	if (!check_flip__crtc(sna, crtc)) {
		DBG(("%s: flip invalid for CRTC\n", __FUNCTION__));
		return FALSE;
	}

	if (!sync_flip && pixmap->usage_hint == SNA_PIXMAP_USAGE_DRI3_IMPORT) {
		DBG(("%s: flip invalid for modifier\n", __FUNCTION__));
		return FALSE;
	}

	flip = sna_pixmap(pixmap);
	if (flip == NULL) {
		DBG(("%s: unattached pixmap\n", __FUNCTION__));
		return FALSE;
	}

	if (flip->cpu_bo && IS_STATIC_PTR(flip->ptr)) {
		DBG(("%s: SHM pixmap\n", __FUNCTION__));
		return FALSE;
	}

	if (flip->pinned) {
		assert(flip->gpu_bo);
		if (sna->flags & SNA_LINEAR_FB) {
			if (flip->gpu_bo->tiling != I915_TILING_NONE) {
				DBG(("%s: pinned bo, tiling=%d needs NONE\n",
				     __FUNCTION__, flip->gpu_bo->tiling));
				*reason = PRESENT_FLIP_REASON_BUFFER_FORMAT;
				return FALSE;
			}
		} else {
			if (!sna->kgem.can_scanout_y &&
			    flip->gpu_bo->tiling == I915_TILING_Y) {
				DBG(("%s: pinned bo, tiling=%d and can't scanout Y\n",
				     __FUNCTION__, flip->gpu_bo->tiling));
				*reason = PRESENT_FLIP_REASON_BUFFER_FORMAT;
				return FALSE;
			}
		}

		if (flip->gpu_bo->pitch & 63) {
			DBG(("%s: pinned bo, bad pitch=%d\n",
			     __FUNCTION__, flip->gpu_bo->pitch));
			return FALSE;
		}
	}

	return TRUE;
}

static void
sna_present_flip_handler(struct sna *sna, uint64_t msc,
                        uint64_t ust, void *data)
{
    struct sna_present_event *event = data;

	sna_present_vblank_handler(msc, ust, event);
}

static Bool
do_flip(struct sna *sna,
	RRCrtcPtr crtc,
	uint64_t event_id,
	uint64_t target_msc,
	struct kgem_bo *bo,
	bool async)
{
	struct sna_present_event *info;

	DBG(("%s(crtc=%d, event=%lld, handle=%d async=%d)\n",
	     __FUNCTION__,
	     crtc_index_from_crtc(crtc),
	     (long long)event_id,
	     bo->handle, async));

	info = info_alloc(sna);
	if (info == NULL)
		return FALSE;

	info->event_id = event_id;
	info->is_fake = FALSE;
	info->unflip = FALSE;

	if (!sna_page_flip(sna, bo, async, sna_present_flip_handler, info)) {
		DBG(("%s: pageflip failed\n", __FUNCTION__));
		info_free(info);
		return FALSE;
	}

	add_to_crtc_vblank(info, 1);
	return TRUE;
}

static Bool
flip__async(struct sna *sna,
	    RRCrtcPtr crtc,
	    uint64_t event_id,
	    uint64_t target_msc,
	    struct kgem_bo *bo)
{
	return do_flip(sna, crtc, event_id, target_msc, bo, true);
}

static Bool
flip(struct sna *sna,
     RRCrtcPtr crtc,
     uint64_t event_id,
     uint64_t target_msc,
     struct kgem_bo *bo)
{
	return do_flip(sna, crtc, event_id, target_msc, bo, false);
}

static struct kgem_bo *
get_flip_bo(struct sna *sna, PixmapPtr pixmap, bool async_flip)
{
	struct sna_pixmap *priv;
	DBG(("%s(pixmap=%ld usage=%u)\n", __FUNCTION__, pixmap->drawable.serialNumber, pixmap->usage_hint));

	priv = sna_pixmap_move_to_gpu(pixmap, MOVE_READ | __MOVE_SCANOUT | __MOVE_FORCE);
	if (priv == NULL) {
		DBG(("%s: cannot force pixmap to the GPU\n", __FUNCTION__));
		return NULL;
	}

	if (priv->gpu_bo->scanout)
		return priv->gpu_bo;

	if (sna->kgem.has_llc && !wedged(sna) && !priv->pinned) {
		struct kgem_bo *bo;
		uint32_t tiling;

		tiling = I915_TILING_NONE;
		if ((sna->flags & SNA_LINEAR_FB) == 0)
			tiling = I915_TILING_X;

		bo = kgem_create_2d(&sna->kgem,
				    pixmap->drawable.width,
				    pixmap->drawable.height,
				    pixmap->drawable.bitsPerPixel,
				    tiling, CREATE_SCANOUT | CREATE_CACHED);
		if (bo) {
			BoxRec box;

			box.x1 = box.y1 = 0;
			box.x2 = pixmap->drawable.width;
			box.y2 = pixmap->drawable.height;

			if (sna->render.copy_boxes(sna, GXcopy,
						   &pixmap->drawable, priv->gpu_bo, 0, 0,
						   &pixmap->drawable, bo, 0, 0,
						   &box, 1, 0)) {
				sna_pixmap_unmap(pixmap, priv);
				kgem_bo_destroy(&sna->kgem, priv->gpu_bo);

				priv->gpu_bo = bo;
			} else
				kgem_bo_destroy(&sna->kgem, bo);
		}
	}

	if (sna->flags & SNA_LINEAR_FB &&
	    priv->gpu_bo->tiling &&
	    !sna_pixmap_change_tiling(pixmap, I915_TILING_NONE)) {
		DBG(("%s: invalid tiling for scanout, user requires linear\n", __FUNCTION__));
		return NULL;
	}

	if (priv->gpu_bo->tiling == I915_TILING_Y &&
	    !sna->kgem.can_scanout_y &&
	    !sna_pixmap_change_tiling(pixmap, I915_TILING_X)) {
		DBG(("%s: invalid Y-tiling, cannot convert\n", __FUNCTION__));
		return NULL;
	}

	if (priv->gpu_bo->pitch & 63) {
		DBG(("%s: invalid pitch, no conversion\n", __FUNCTION__));
		return NULL;
	}

	return priv->gpu_bo;
}

static Bool
sna_present_flip(RRCrtcPtr crtc,
		 uint64_t event_id,
		 uint64_t target_msc,
		 PixmapPtr pixmap,
		 Bool sync_flip)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct kgem_bo *bo;

	DBG(("%s(crtc=%d, event=%lld, msc=%lld, pixmap=%ld, sync?=%d)\n",
	     __FUNCTION__,
	     crtc_index_from_crtc(crtc),
	     (long long)event_id,
	     (long long)target_msc,
	     pixmap->drawable.serialNumber, sync_flip));

	if (!check_flip__crtc(sna, crtc)) {
		DBG(("%s: flip invalid for CRTC\n", __FUNCTION__));
		return FALSE;
	}

	assert(sna->present.unflip == 0);

	if (sna->flags & SNA_TEAR_FREE) {
		DBG(("%s: disabling TearFree (was %s) in favour of Present flips\n",
		     __FUNCTION__, sna->mode.shadow_enabled ? "enabled" : "disabled"));
		sna->mode.shadow_enabled = false;
	}
	assert(!sna->mode.shadow_enabled);

	if (sna->mode.flip_active) {
		struct pollfd pfd;

		DBG(("%s: flips still pending, stalling\n", __FUNCTION__));
		pfd.fd = sna->kgem.fd;
		pfd.events = POLLIN;
		while (poll(&pfd, 1, 0) == 1)
			sna_mode_wakeup(sna);
		if (sna->mode.flip_active)
			return FALSE;
	}

	bo = get_flip_bo(sna, pixmap, !sync_flip);
	if (bo == NULL) {
		DBG(("%s: flip invalid bo\n", __FUNCTION__));
		return FALSE;
	}

	if (sync_flip)
		return flip(sna, crtc, event_id, target_msc, bo);
	else
		return flip__async(sna, crtc, event_id, target_msc, bo);
}

static void
sna_present_unflip(ScreenPtr screen, uint64_t event_id)
{
	struct sna *sna = to_sna_from_screen(screen);
	struct kgem_bo *bo;

	DBG(("%s(event=%lld)\n", __FUNCTION__, (long long)event_id));
	if (sna->mode.front_active == 0 || sna->mode.rr_active) {
		const struct ust_msc *swap;

		DBG(("%s: no CRTC active, perform no-op flip\n", __FUNCTION__));

notify:
		swap = sna_crtc_last_swap(sna_primary_crtc(sna));
		DBG(("%s: crtc=%d, tv=%d.%06d msc=%lld, event=%lld complete\n", __FUNCTION__,
		     -1,
		     swap->tv_sec, swap->tv_usec, (long long)swap->msc,
		     (long long)event_id));
		present_event_notify(event_id, swap_ust(swap), swap->msc);
		return;
	}

	if (sna->mode.flip_active) {
		DBG(("%s: %d outstanding flips, queueing unflip\n", __FUNCTION__, sna->mode.flip_active));
		assert(sna->present.unflip == 0);
		sna->present.unflip = event_id;
		return;
	}

	bo = get_flip_bo(sna, screen->GetScreenPixmap(screen), false);

	/* Are we unflipping after a failure that left our ScreenP in place? */
	if (!sna_needs_page_flip(sna, bo))
		goto notify;

	assert(!sna->mode.shadow_enabled);
	if (sna->flags & SNA_TEAR_FREE) {
		DBG(("%s: %s TearFree after Present flips\n",
		     __FUNCTION__, sna->mode.shadow_damage != NULL ? "enabling" : "disabling"));
		sna->mode.shadow_enabled = sna->mode.shadow_damage != NULL;
	}

	if (bo == NULL) {
reset_mode:
		DBG(("%s: failed, trying to restore original mode\n", __FUNCTION__));
		xf86SetDesiredModes(sna->scrn);
		goto notify;
	}

	assert(sna_pixmap(screen->GetScreenPixmap(screen))->pinned & PIN_SCANOUT);

	if (sna->flags & SNA_HAS_ASYNC_FLIP) {
		DBG(("%s: trying async flip restore\n", __FUNCTION__));
		if (flip__async(sna, NULL, event_id, 0, bo))
			return;
	}

	if (!flip(sna, NULL, event_id, 0, bo))
		goto reset_mode;
}

void sna_present_cancel_flip(struct sna *sna)
{
	if (sna->present.unflip) {
		const struct ust_msc *swap;

		swap = sna_crtc_last_swap(sna_primary_crtc(sna));
		present_event_notify(sna->present.unflip,
				     swap_ust(swap), swap->msc);

		sna->present.unflip = 0;
	}
}

static present_screen_info_rec present_info = {
	.version = PRESENT_SCREEN_INFO_VERSION,

	.get_crtc = sna_present_get_crtc,
	.get_ust_msc = sna_present_get_ust_msc,
	.queue_vblank = sna_present_queue_vblank,
	.abort_vblank = sna_present_abort_vblank,
	.flush = sna_present_flush,

	.capabilities = PresentCapabilityNone,
	.flip = sna_present_flip,
	.unflip = sna_present_unflip,

	/* Use the fancier check_flip2 even if we don't implement anything new here */
	.check_flip = NULL,
	.check_flip2 = sna_present_check_flip2
};

bool sna_present_open(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s(num_crtc=%d)\n", __FUNCTION__, sna->mode.num_real_crtc));

	if (sna->mode.num_real_crtc == 0)
		return false;

	sna_present_update(sna);
	list_init(&sna->present.vblank_queue);

	return present_screen_init(screen, &present_info);
}

void sna_present_update(struct sna *sna)
{
	if (sna->flags & SNA_HAS_ASYNC_FLIP)
		present_info.capabilities |= PresentCapabilityAsync;
	else
		present_info.capabilities &= ~PresentCapabilityAsync;

	DBG(("%s: has_async_flip? %d\n", __FUNCTION__,
	     !!(present_info.capabilities & PresentCapabilityAsync)));
}

void sna_present_close(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s()\n", __FUNCTION__));
}
