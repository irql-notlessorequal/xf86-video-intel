#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "intel_options.h"
#include "ega.h"
#include "ega_cpu_arch.h"
#include "ega_helpers.h"

Bool ega_init_driver(ScrnInfoPtr scrn, int entity_num)
{
	scrn->PreInit = ega_pre_init;
	scrn->ScreenInit = ega_init_screen;
	return TRUE;
}

Bool ega_pre_init(ScrnInfoPtr scrn, int flags)
{
	ScreenPtr screen = scrn->pScreen;
	pointer glamor_module;
	CARD32 version;
	ega* ega;
	struct intel_device *device;
	int fd;

	if (!ega_cpu_arch_load())
	{
		return FALSE;
	}

	device = intel_get_device(scrn, &fd);
	if (!fd) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to claim DRM device.\n");
		return FALSE;
	}

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,20,99,0,0)
	if (scrn->depth < 24) {
#else
	if (scrn->depth < 15) {
#endif
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Depth %d not supported with glamor, disabling\n",
			   scrn->depth);
		return FALSE;
	}

	if ((glamor_module = xf86LoadSubModule(scrn, GLAMOR_EGL_MODULE_NAME)))
	{
		version = xf86GetModuleVersion(glamor_module);

		if (version < MODULE_VERSION_NUMERIC(0, 3, 1))
		{
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Incompatible glamor version, required >= 0.3.0.\n");
			return FALSE;
		}
		else
		{
			if (scrn->depth == 30 &&
			    version < MODULE_VERSION_NUMERIC(1, 0, 1)) {
				xf86DrvMsg(scrn->scrnIndex, X_WARNING,
					   "Depth 30 requires glamor >= 1.0.1 (xserver 1.20),"
					   " can't enable glamor\n");
				return FALSE;
			}

			if (glamor_egl_init(scrn, fd)) {
				xf86DrvMsg(scrn->scrnIndex, X_INFO,
					   "glamor detected, initialising EGL layer.\n");
			} else {
				xf86DrvMsg(scrn->scrnIndex, X_ERROR,
					   "glamor detected, failed to initialize EGL.\n");
				return FALSE;
			}
		}

		OptionInfoPtr options = intel_options_get(scrn);
		if (((uintptr_t)scrn->driverPrivate) & 3)
		{
			ega = xnfcalloc(sizeof(*ega), 1);
			if (ega == NULL)
				return FALSE;

			ega->Options = options;
			ega->dev = device;
			ega->info = (void *)((uintptr_t)scrn->driverPrivate & ~3);

			scrn->driverPrivate = ega;
		}

		if (!ega->info->allow_ega)
		{
			xf86DrvMsg(scrn->scrnIndex, X_ERROR, "EGA not available on this GPU, please try SNA or UXA instead.\n");
			return FALSE;
		}

		return TRUE;
	} else {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR, "glamor not available\n");
		return FALSE;
	}
}

Bool ega_init_screen(ScreenPtr screen, int argc, char **argv)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
	ega* ega = ega_get_screen_private(scrn);

	int flags = GLAMOR_USE_EGL_SCREEN | GLAMOR_USE_SCREEN | GLAMOR_USE_PICTURE_SCREEN | GLAMOR_INVERTED_Y_AXIS;
	int dri_level = intel_option_cast_to_unsigned(ega->Options, OPTION_DRI, DEFAULT_DRI_LEVEL);

	if (dri_level <= 1)
	{
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "EGA does not support DRI 1, this is likely a configuration error.\n");
		return FALSE;
	}
	else if (dri_level == 2)
	{
		flags |= GLAMOR_NO_DRI3;
	}

	if (!glamor_init(screen, flags))
	{
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to initialize EGA.\n");
		return FALSE;
	}

#ifdef ENABLE_BROKEN_EGA
	Bool use_hardware_xvmc = ega->info->gen < 0120;
#else
	Bool use_hardware_xvmc = FALSE;
#endif

	xf86DrvMsg(scrn->scrnIndex, X_INFO, "Using hardware offloading for XvMC? %s", (use_hardware_xvmc ? "YES" : "NO"));

	if (use_hardware_xvmc)
	{
//		intel_video_overlay_setup_image(screen);
//		ega->native_xvmc_available = 1;
		assert(!"TODO");
	}
	else
	{
		ega->native_xvmc_available = 0;
		if (!glamor_egl_init_textured_pixmap(screen))
		{
			xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Failed to initialize textured pixmap of screen for GLAMOR.\n");
			return FALSE;
		}
	}

#ifdef ENABLE_BROKEN_EGA
	/* Allow BLT on pre-Gen 11 graphics. */
	ega->blitter_available = ega->info->gen < 0120;
#else
	ega->blitter_available = 0;
#endif


	if (ega->blitter_available || ega->native_xvmc_available)
	{
		ega_load_helpers(screen);
	}
	else
	{
		xf86DrvMsg(scrn->scrnIndex, X_INFO, "[EGA] No blitter or XvMC available, using pure GLAMOR.\n");
	}

	xf86DrvMsg(scrn->scrnIndex, X_INFO, "Using EGA.\n");
	return TRUE;
}