#include "ega_pure.h"
#include "ega_drm.h"

static Bool ega_pure_close_screen(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	if (pScrn->vtSema) {
		LeaveVT(pScrn);
	}

	pScrn->vtSema = FALSE;
	return (*pScreen->CloseScreen)(pScreen);
}

static Bool ega_load_pure(ScreenPtr screen, ega_drm_metadata drm_meta)
{
	ScrnInfoPtr scrn = drm_meta->scrn;

	scrn->displayWidth = scrn->virtualX;
	if (!ega_create_initial_bos(scrn, drm_meta))
	{
		xf86DrvMsg(scrn->scrnIndex, X_ERROR, "[ega_load_pure] ega_create_initial_bos() returned FALSE.\n");
		return FALSE;
	}

	miClearVisualTypes();

	if (!miSetVisualTypes(scrn->depth, miGetDefaultVisualMask(scrn->depth), scrn->rgbBits, scrn->defaultVisual))
	{
		return FALSE;
	}

	if (!miSetPixmapDepths())
	{
		return FALSE;
	}

	scrn->memPhysBase = 0;
	scrn->fbOffset = 0;

	if (!fbScreenInit(screen, NULL, scrn->virtualX, scrn->virtualY, scrn->xDpi, scrn->yDpi, scrn->displayWidth, scrn->bitsPerPixel))
	{
		return FALSE;
	}

	/* Fix up visuals if we're running in HDR or equivalent. */
	if (scrn->bitsPerPixel > 8) {
		VisualPtr visual;

		visual = screen->visuals + screen->numVisuals;
		while (--visual >= screen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed = scrn->offset.red;
				visual->offsetGreen = scrn->offset.green;
				visual->offsetBlue = scrn->offset.blue;
				visual->redMask = scrn->mask.red;
				visual->greenMask = scrn->mask.green;
				visual->blueMask = scrn->mask.blue;
			}
		}
	}

	if (!fbPictureInit(screen, NULL, 0))
	{
		return FALSE;
	}

	screen->CloseScreen = ega_pure_close_screen;
	return TRUE;
}