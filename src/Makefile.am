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

SUBDIRS = render_program legacy

# this is obnoxious:
# -module lets us name the module exactly how we want
# -avoid-version prevents gratuitous .0.0.0 version numbers on the end
# _ladir passes a dummy rpath to libtool so the thing will actually link
# TODO: -nostdlib/-Bstatic/-lgcc platform magic, not installing the .a, etc.

AM_CFLAGS = \
	@CWARNFLAGS@ \
	$(XORG_CFLAGS) \
	$(DRM_CFLAGS) \
	$(PCIACCESS_CFLAGS) \
	@NOWARNFLAGS@ \
	$(NULL)

if VALGRIND
AM_CFLAGS += $(VALGRIND_CFLAGS)
endif

intel_drv_la_LTLIBRARIES = intel_drv.la
intel_drv_la_LDFLAGS = -module -avoid-version
intel_drv_ladir = $(moduledir)/drivers
intel_drv_la_LIBADD = legacy/liblegacy.la $(PCIACCESS_LIBS) $(XORG_LIBS)

if SNA
SUBDIRS += sna
intel_drv_la_LIBADD += sna/libsna.la
endif

if UXA
SUBDIRS += uxa
intel_drv_la_LIBADD += uxa/libuxa.la
endif

if EGA
SUBDIRS += ega
intel_drv_la_LIBADD += ega/libega.la
endif

NULL:=#

intel_drv_la_SOURCES = \
	backlight.c \
	backlight.h \
	fd.h \
	fd.c \
	i915_pciids.h \
	intel_list.h \
	intel_options.h \
	intel_device.c \
	intel_driver.h \
	intel_options.c \
	intel_module.c \
	compat-api.h \
	$(NULL)

EXTRA_DIST = \
	scripts/clock.5c \
	scripts/clock-graph.5c \
	scripts/fix.5c \
	scripts/tv.5c \
	$(NULL)
