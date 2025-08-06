noinst_LTLIBRARIES = libglamor.la libglamor_egl_stubs.la

libglamor_la_LIBADD = $(GLAMOR_LIBS) $(GBM_LIBS)

AM_CFLAGS = $(DIX_CFLAGS) $(XORG_CFLAGS) $(DRM_CFLAGS) $(DRMINTEL_CFLAGS) $(EPOXY_CFLAGS) $(UDEV_CFLAGS) $(CWARNFLAGS) -DWITH_LIBDRM=1 \
            -DGLAMOR_HAS_EGL_QUERY_DMABUF=1 -DGLAMOR_HAS_EGL_QUERY_DRIVER=1 \
			-DGLXEXT=1 -DGLAMOR=1 -DGLAMOR_HAS_GBM=1 -DGLAMOR_HAS_GBM_LINEAR=1 \
			-DGBM_BO_WITH_MODIFIERS=1

libglamor_la_SOURCES = \
	glamor.c \
	glamor_context.h \
	glamor_copy.c \
	glamor_core.c \
	glamor_dash.c \
	glamor_debug.h \
	glamor_font.c \
	glamor_font.h \
	glamor_egl.c \
	glamor_egl.h \
	glamor_composite_glyphs.c \
	glamor_image.c \
	glamor_lines.c \
	glamor_segs.c \
	glamor_render.c \
	glamor_gradient.c \
	glamor_prepare.c \
	glamor_prepare.h \
	glamor_program.c \
	glamor_program.h \
	glamor_rects.c \
	glamor_spans.c \
	glamor_text.c \
	glamor_transfer.c \
	glamor_transfer.h \
	glamor_transform.c \
	glamor_transform.h \
	glamor_trapezoid.c \
	glamor_triangles.c\
	glamor_addtraps.c\
	glamor_glyphblt.c\
	glamor_points.c\
	glamor_priv.h\
	glamor_pixmap.c\
	glamor_largepixmap.c\
	glamor_picture.c\
	glamor_vbo.c \
	glamor_window.c\
	glamor_fbo.c\
	glamor_compositerects.c\
	glamor_utils.c\
	glamor_utils.h\
	glamor_sync.c \
	glamor.h

if XVMC
libglamor_la_SOURCES += \
	glamor_xv.c
endif

libglamor_egl_stubs_la_SOURCES = \
	glamor_egl_ext.h \
	glamor_egl.h