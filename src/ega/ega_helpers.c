#include "ega_helpers.h"

static void ega_copy_window(WindowPtr win, DDXPointRec old_origin, RegionPtr src_region)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(win->drawable.pScreen);
	ega* ega = ega_get_screen_private(scrn);

	if (ega->blitter_available)
	{
		assert(!"TODO");
	}
	else
	{
		glamor_copy_window(win, old_origin, src_region);
	}
}

static void ega_get_image(DrawablePtr pDrawable, int x, int y, int w, int h, unsigned int format, unsigned long planeMask, char *d)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(pDrawable->pScreen);
	ega* ega = ega_get_screen_private(scrn);

	if (ega->blitter_available)
	{
		assert(!"TODO");
	}
	else
	{
		fbGetImage(pDrawable, x, y, w, h, format, planeMask, d);
	}
}

static void ega_get_spans(DrawablePtr pDrawable, int wMax, DDXPointPtr ppt, int *pwidth, int nspans, char *pdstStart)
{
	assert(!"TODO");
}

static void ega_load_helpers(ScreenPtr screen)
{
	screen->CopyWindow = ega_copy_window;
	screen->GetImage = ega_get_image;
	screen->GetSpans = ega_get_spans;
}