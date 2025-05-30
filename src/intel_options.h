#ifndef INTEL_OPTIONS_H
#define INTEL_OPTIONS_H

#include <xorg-server.h>
#include <xf86.h>
#include <xf86Opt.h>

/*
 * Note: "ColorKey" is provided for compatibility with the i810 driver.
 * However, the correct option name is "VideoKey".  "ColorKey" usually
 * refers to the tranparency key for 8+24 overlays, not for video overlays.
 */

enum intel_options {
	OPTION_ACCEL_ENABLE,
	OPTION_ACCEL_METHOD,
	OPTION_BACKLIGHT,
	OPTION_EDID,
	OPTION_DRI,
	OPTION_PRESENT,
	OPTION_VIDEO_KEY,
	OPTION_COLOR_KEY,
	OPTION_TILING_2D,
	OPTION_TILING_FB,
	OPTION_ROTATION,
	OPTION_VSYNC,
	OPTION_PAGEFLIP,
	OPTION_SWAPBUFFERS_WAIT,
	OPTION_TRIPLE_BUFFER,
	OPTION_PREFER_OVERLAY,
	OPTION_HOTPLUG,
	OPTION_REPROBE,
#if defined(XvMCExtension) && defined(ENABLE_XVMC)
	OPTION_XVMC,
#define INTEL_XVMC 1
#endif
#ifdef USE_SNA
	OPTION_THROTTLE,
	OPTION_ZAPHOD,
	OPTION_VIRTUAL,
	OPTION_TEAR_FREE,
	OPTION_CRTC_PIXMAPS,
	OPTION_ASYNC_SWAP,
	OPTION_SNA_PRIME_SYNC,
	OPTION_ENABLE_Y_SCANOUT,
	OPTION_ENABLE_Y_COLOR_TILING,
#endif
#ifdef USE_UXA
	OPTION_FALLBACKDEBUG,
	OPTION_DEBUG_FLUSH_BATCHES,
	OPTION_DEBUG_FLUSH_CACHES,
	OPTION_DEBUG_WAIT,
	OPTION_BUFFER_CACHE,
#endif
#ifdef USE_EGA
	/**
	 * Allows the BLT (Blitter) engine to be used to offload memory copies and transfers away from the CPU.
	 */
	OPTION_ENABLE_BLITTER,
#endif
	NUM_OPTIONS,
};

extern const OptionInfoRec intel_options[];
OptionInfoPtr intel_options_get(ScrnInfoPtr scrn);
unsigned intel_option_cast_to_unsigned(OptionInfoPtr, int id, unsigned val);
Bool intel_option_cast_to_bool(OptionInfoPtr, int id, Bool val);

#endif /* INTEL_OPTIONS_H */
