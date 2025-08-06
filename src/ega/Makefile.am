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

SUBDIRS = 
AM_CFLAGS = $(DIX_CFLAGS) $(XORG_CFLAGS) $(DRM_CFLAGS) $(DRMINTEL_CFLAGS) \
            $(EPOXY_CFLAGS) $(UDEV_CFLAGS) $(CWARNFLAGS) \
			-I$(top_srcdir)/src \
			-DWITH_LIBDRM=1 \
            -DGLAMOR_HAS_EGL_QUERY_DMABUF=1 \
			-DGLAMOR_HAS_EGL_QUERY_DRIVER=1 \
			-DGLXEXT=1 -DGLAMOR=1 \ 
			-DGLAMOR_HAS_GBM=1 -DGLAMOR_HAS_GBM_LINEAR=1 \
			-DGBM_BO_WITH_MODIFIERS=1

AM_CPPFLAGS = \
	$(XORG_INCS) \
	-I$(srcdir)/glamor \
	$(NULL)

noinst_LTLIBRARIES = libega.la
libega_la_LIBADD = $(UDEV_LIBS) $(DRMINTEL_LIBS) $(DRM_LIBS) $(EPOXY_LIBS)
libega_la_ladir = @moduledir@/drivers

SUBDIRS += glamor
libega_la_LIBADD += glamor/libglamor.la

libega_la_SOURCES = \
	 ega_module.h \
	 ms_compat.c \
	 ms_compat.h \
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
