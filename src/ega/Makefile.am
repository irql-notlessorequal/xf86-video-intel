AM_CFLAGS = @CWARNFLAGS@ $(XORG_CFLAGS) $(DRM_CFLAGS) $(PCIACCESS_CFLAGS)
AM_CFLAGS += $(UDEV_CFLAGS) $(DRM_CFLAGS) $(DRMINTEL_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src

noinst_LTLIBRARIES = libega.la
libega_la_LIBADD = $(UDEV_LIBS) $(DRMINTEL_LIBS) $(DRM_LIBS)

# 	ega_video.h
#	ega_video.c
#	ega_xvmc_overlay.h
#	ega_xvmc_overlay.c

libega_la_SOURCES = \
	i830_reg.h \
	intel.h \
	ega_cpu_arch.h \
	ega_cpu_arch.c \
	ega_helpers.h \
	ega_helpers.c \
	ega_module.h \
	ega.h \
	ega_driver.c
	$(NULL)