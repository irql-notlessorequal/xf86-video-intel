#include "xorg-server.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86Cursor.h"
#include "xf86xv.h"
#include "xf86Crtc.h"
#include "xf86RandR12.h"

#include "intel_driver.h"

#include <gbm.h>

#ifndef HAVE_GLAMOR_FINISH
#include <GL/gl.h>
#endif

#include <xf86cmap.h>
#include <xf86drm.h>
#include <xf86RandR12.h>

/* Required. */
#define GLAMOR_FOR_XORG  1
#include <glamor.h>

#ifndef GLAMOR_INVERTED_Y_AXIS
#define GLAMOR_INVERTED_Y_AXIS 0
#endif
#include <intel_bufmgr.h>

#ifndef GLAMOR_USE_SCREEN
#define GLAMOR_USE_SCREEN 0
#endif

#ifndef GLAMOR_USE_PICTURE_SCREEN
#define GLAMOR_USE_PICTURE_SCREEN 0
#endif

#ifndef GLAMOR_EGL_MODULE_NAME
#define GLAMOR_EGL_MODULE_NAME "glamoregl"
#endif

#ifndef DEFAULT_DRI_LEVEL
#define DEFAULT_DRI_LEVEL 3
#endif

#ifndef EGA_CORE
#define EGA_CORE 1
typedef struct ega {
	struct intel_device *dev;
	const struct intel_device_info *info;
	dri_bufmgr *bufmgr;

	unsigned int blitter_available : 1 ;
	unsigned int native_xvmc_available : 1;
	unsigned int use_overlay : 1;
	unsigned int force_fallback : 1;

	OptionInfoPtr Options;

	/* Required for XvMC via UXA */
	Bool XvEnabled;		/* Xv enabled for this generation. */
	Bool XvPreferOverlay;

	int drmSubFD;
	int colorKey;
	XF86VideoAdaptorPtr adaptor;
} ega;

static inline ega * ega_get_screen_private(ScrnInfoPtr scrn)
{
	return (ega *)(scrn->driverPrivate);
}

static ModeStatus ega_valid_mode(ScrnInfoPtr arg, DisplayModePtr mode, Bool verbose, int flags);
Bool ega_init_driver(ScrnInfoPtr scrn, int entity_num);
Bool ega_pre_init(ScrnInfoPtr scrn, int flags);
Bool ega_init_screen(ScreenPtr screen, int argc, char **argv);
#endif