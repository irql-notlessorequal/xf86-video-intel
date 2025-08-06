#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

AM_CFLAGS = $(DIX_CFLAGS) $(XORG_CFLAGS) $(LIBDRM_CFLAGS) $(UDEV_CFLAGS) $(CWARNFLAGS)

AM_CPPFLAGS = \
	$(XORG_INCS) \
	-I$(top_srcdir)/glamor \
	-I$(srcdir)/../../ddc \
	-I$(srcdir)/../../dri2 \
	-I$(srcdir)/../../i2c \
	-I$(srcdir)/../../modes \
	-I$(srcdir)/../../parser \
	-I$(srcdir)/../../ramdac \
	$(NULL)

noinst_LTLIBRARIES = libega.la
libega_la_LIBADD = $(UDEV_LIBS) $(DRM_LIBS)
libega_la_ladir = @moduledir@/drivers

libega_la_SOURCES = \
	 driver.c \
	 driver.h \
	 drmmode_display.c \
	 drmmode_display.h \
	 dumb_bo.c \
	 dumb_bo.h \
	 present.c \
	 vblank.c \
	 pageflip.c \
	 $(NULL)