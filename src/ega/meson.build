config.set('USE_EGA', 1)

if cc.has_member('struct sysinfo', 'totalram',
		 prefix : '#include <sys/sysinfo.h>')
  config.set('HAVE_STRUCT_SYSINFO_TOTALRAM', 1)
endif

git_version_h = vcs_tag(input : 'git_version.h.in', output : 'git_version.h',
			fallback : 'not compiled from git',
			command : [ 'git', 'describe' ] )

config.set('HAVE_DOT_GIT', 1)

if cc.has_header('alloca.h')
  config.set('HAVE_ALLOCA_H', 1)
endif

ega_deps = [
  cc.find_library('m', required : true),
  dependency('threads', required : true),
  dependency('xorg-server', required : true),
  dependency('libdrm', required : true),
]

sna_sources = [
	'ega_driver.c',
	'ega_cpu_arch.c,
	'ega_helpers.c',
	'ega_video.c',
	'ega_xvmc_overlay.c'
]

ega = static_library('ega',
		     [ git_version_h, ega_sources ],
		     dependencies : ega_deps,
		     link_with : [ brw, fb, ],
		     include_directories : inc,
		     c_args : [
		       '-Wno-unused-but-set-variable',
		       '-Wno-expansion-to-defined',
		       '-Wno-maybe-uninitialized',
		       '-Wno-unused-parameter',
		       '-Wno-unused-function',
		       '-Wno-unused-variable',
		       '-Wno-sign-compare',
		       '-Wno-type-limits',
		       '-Wno-clobbered',
		     ],
		     install : false)
