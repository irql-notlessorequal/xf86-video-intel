#include "ega_drm.h"

static Bool ega_create_bo(ega_drm_metadata ega_meta, ega_bo bo, unsigned width, unsigned height, unsigned bpp, Bool is_front)
{
	bo->width = width;
    bo->height = height;

#ifdef EGA_HAS_MODIFIERS
	uint32_t num_modifiers;
	uint64_t *modifiers = NULL;
#endif
	uint32_t format;

	switch (ega_meta->scrn->depth) {
		case 15:
			format = GBM_FORMAT_ARGB1555;
			break;
		case 16:
			format = GBM_FORMAT_RGB565;
			break;
		case 30:
			format = GBM_FORMAT_ARGB2101010;
			break;
		default:
			format = GBM_FORMAT_ARGB8888;
			break;
	}

#ifdef EGA_HAS_MODIFIERS
	num_modifiers = get_modifiers_set(ega_meta->scrn, format, &modifiers, FALSE, TRUE);

	if (num_modifiers > 0 && !(num_modifiers == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID))
	{
		bo->gbm = gbm_bo_create_with_modifiers(ega_meta->gbm, width, height, format, modifiers, num_modifiers);
		free(modifiers);

		if (bo->gbm)
		{
			bo->using_modifiers = TRUE;
			return TRUE;
		}
	}
#endif

	int flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
#ifdef EGA_HAS_FRONT_RENDERING
	if (is_front)
	{
		flags |= GBM_BO_USE_FRONT_RENDERING;
	}
#endif

	bo->gbm = gbm_bo_create(ega_meta->gbm, width, height, format, flags);
	bo->using_modifiers = FALSE;
	return bo->gbm != NULL;
}

uint32_t ega_bo_get_pitch(ega_bo bo)
{
	return gbm_bo_get_stride(bo->gbm);
}

static void ega_probe_cursor_size(xf86CrtcPtr crtc, ega_drm_metadata drm_meta)
{
	ega* ega = ega_get_screen_private(crtc->scrn);
	ega_crtc_ptr ega_crtc = crtc->driver_private;
	uint32_t handle = ega_crtc->cursor_bo->handle;
	int width, height, size;

	drm_meta->min_cursor_width = drm_meta->max_cursor_width;
	drm_meta->min_cursor_height = drm_meta->max_cursor_height;

	/* probe square min first */
	for (size = 1; size <= drm_meta->max_cursor_width && size <= drm_meta->max_cursor_height; size *= 2)
	{
		int ret;

		ret = drmModeSetCursor2(ega->drmSubFD, ega_crtc->mode_crtc->crtc_id,
								handle, size, size, 0, 0);
		if (ret == 0)
			break;
	}

	/* check if smaller width works with non-square */
	for (width = 1; width <= size; width *= 2)
	{
		int ret;

		ret = drmModeSetCursor2(ega->drmSubFD, ega_crtc->mode_crtc->crtc_id,
								handle, width, size, 0, 0);
		if (ret == 0)
		{
			drm_meta->min_cursor_width = width;
			break;
		}
	}

	/* check if smaller height works with non-square */
	for (height = 1; height <= size; height *= 2)
	{
		int ret;

		ret = drmModeSetCursor2(ega->drmSubFD, ega_crtc->mode_crtc->crtc_id,
								handle, size, height, 0, 0);
		if (ret == 0)
		{
			drm_meta->min_cursor_height = height;
			break;
		}
	}

	drmModeSetCursor2(ega->drmSubFD, ega_crtc->mode_crtc->crtc_id, 0, 0, 0, 0, 0);
}

Bool ega_create_initial_bos(ScrnInfoPtr pScrn, ega_drm_metadata drm_meta)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

	int width;
	int height;
	int bpp = drm_meta->kbpp;
	int i;
	int cpp = (bpp + 7) / 8;

	width = pScrn->virtualX;
	height = pScrn->virtualY;

	if (!ega_create_bo(drm_meta, drm_meta->front_bo, width, height, bpp, TRUE))
	{
		return FALSE;
	}

	pScrn->displayWidth = ega_bo_get_pitch(&drm_meta->front_bo) / cpp;

	width = drm_meta->max_cursor_width;
    height = drm_meta->max_cursor_height;
	bpp = 32;
	for (i = 0; i < xf86_config->num_crtc; i++)
	{
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		ega_crtc_ptr ega_crtc = crtc->driver_private;

		ega_crtc->cursor_bo = dumb_bo_create(drm_meta->fd, width, height, bpp);
    }

	ega_probe_cursor_size(xf86_config->crtc[0], drm_meta);
	return TRUE;
}
