ega_sources = [
    'ms_compat.c',
    'driver.c',
    'drmmode_display.c',
    'dumb_bo.c',
    'pageflip.c',
    'present.c',
    'vblank.c'
]

git_version_h = vcs_tag(input : 'git_version.h.in', output : 'git_version.h',
            fallback : 'not compiled from git',
            command : [ 'git', 'describe' ] )

libdrm_dep = dependency('libdrm', version: '>= 2.4.109')
xserver_dep = dependency('xorg-server', version: '>= 21.1')
gbm_dep = dependency('gbm', version: '>= 17.1')
epoxy_dep = dependency('epoxy', version: '>= 1.5.4')

ega_c_args = []
ega_c_args += '-DWITH_LIBDRM=1'
ega_c_args += '-DGLAMOR_HAS_EGL_QUERY_DMABUF=1'
ega_c_args += '-DGLAMOR_HAS_EGL_QUERY_DRIVER=1'
ega_c_args += '-DGLXEXT=1'
ega_c_args += '-DGLAMOR=1'
ega_c_args += '-DGLAMOR_HAS_GBM=1'
ega_c_args += '-DGLAMOR_HAS_GBM_LINEAR=1'
ega_c_args += '-DGBM_BO_WITH_MODIFIERS=1'
ega_c_args += '-DMS_DRI3=1'

subdir('glamor')

ega = static_library('ega',
            [ git_version_h, ega_sources ],
            link_with: [glamor],
            dependencies : [
				libdrm_dep,
				xserver_dep,
				gbm_dep,
				epoxy_dep
			],
            include_directories : inc,
            c_args : ega_c_args,
            install : false)