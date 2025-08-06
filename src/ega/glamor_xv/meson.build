glamoregl_src = [
    '../glamor/glamor_egl.c',
    'glamor_xf86_xv.c',
]

glamorxv = static_library(
    'glamoregl',
    glamoregl_src,

    dependencies: [
        xserver_dep,
        libdrm_dep,
        gbm_dep,
    ],
    c_args: ega_c_args,
)