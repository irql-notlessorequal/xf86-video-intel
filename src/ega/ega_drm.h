#include "ega.h"
#include "dumb_bo.h"

#include <gbm.h>
#include <xf86drmMode.h>

#ifndef EGA_DRM_METADATA
#define EGA_DRM_METADATA 1

typedef struct
{
	unsigned int width;
	unsigned int height;

	struct gbm_bo *gbm;
	unsigned int using_modifiers : 1;
} *ega_bo;

typedef struct
{
	int fd;
	struct gbm_device *gbm;

	ScrnInfoPtr scrn;
	ega_bo front_bo;

	unsigned int kbpp;
	unsigned int min_cursor_width;
	unsigned int min_cursor_height;
	unsigned int max_cursor_width;
	unsigned int max_cursor_height;
} *ega_drm_metadata;

typedef struct
{
	drmModeCrtcPtr mode_crtc;

	struct dumb_bo *cursor_bo;
} *ega_crtc_ptr;

#endif

Bool ega_create_initial_bos(ScrnInfoPtr pScrn, ega_drm_metadata drm_meta);
uint32_t ega_bo_get_pitch(ega_bo bo);

static void ega_probe_cursor_size(xf86CrtcPtr crtc, ega_drm_metadata drm_meta);
static Bool ega_create_bo(ega_drm_metadata ega_meta, ega_bo bo, unsigned width, unsigned height, unsigned bpp, Bool is_front);