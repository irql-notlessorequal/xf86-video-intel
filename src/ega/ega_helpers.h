#include "ega.h"
#include "ega_cpu_arch.h"

static void ega_copy_window(WindowPtr win, DDXPointRec old_origin, RegionPtr src_region);
static void ega_get_image(DrawablePtr pDrawable, int x, int y, int w, int h, unsigned int format, unsigned long planeMask, char *d);
static void ega_get_spans(DrawablePtr pDrawable, int wMax, DDXPointPtr ppt, int *pwidth, int nspans, char *pdstStart);
static void ega_load_helpers(ScreenPtr screen);