project('m0dular-csgo', 'cpp', 'c', default_options : ['c_std=c11', 'cpp_std=c++14'])

compile_args = ['-v']
linker_args = ['-v']

core_files = ['core/init.cpp']
framework_files = ['framework/utils/threading.cpp', 'framework/utils/mutex.cpp', 'framework/utils/pattern_scan.cpp', 'framework/utils/semaphores.cpp', 'framework/utils/handles.cpp']

file_list = core_files + framework_files

includes = []
libraries = []
deps = []

cc = meson.get_compiler('cpp')

if meson.is_cross_build()
	add_args = []
	if host_machine.system() == 'windows'
		linker_args += '-fuse-ld=lld'
		cpu_family = host_machine.cpu_family()
		bf_dir = get_option('msvc_dir')
		includes += join_paths(bf_dir, 'include/clang')
		includes += join_paths(bf_dir, 'include/msvc')
		includes += join_paths(bf_dir, 'include/ucrt')
		includes += join_paths(bf_dir, 'include/um')
		includes += join_paths(bf_dir, 'include/shared')
		includes += join_paths(bf_dir, 'include/winrt')
		libraries += join_paths(bf_dir, 'lib/clang')
		libraries += join_paths(bf_dir, join_paths('lib/msvc', cpu_family))
		libraries += join_paths(bf_dir, join_paths('lib/ucrt', cpu_family))
		libraries += join_paths(bf_dir, join_paths('lib/um', cpu_family))
		libraries += join_paths(bf_dir, join_paths('lib/shared', cpu_family))
		libraries += join_paths(bf_dir, join_paths('lib/winrt', cpu_family))
		linker_args += '-Bdynamic'
		linker_args += '-lkernel32'
		linker_args += '-lmsvcrt'
		linker_args += '-Wl,-debug'
		add_args = ['-target', 'i386-pc-windows-msvc19.14.26428', '-Wno-msvc-not-found', '-D_CRT_SECURE_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS', '-fms-extensions', '-fms-compatibility', '-fms-compatibility-version=19.14.26428', '-Wno-expansion-to-defined', '-Wno-nonportable-include-path', '-Wno-pragma-pack', '-Wno-ignored-attributes', '-Wno-ignored-pragma-intrinsic', '-Wno-int-to-void-pointer-cast', '-fuse-ld=lld', '-flto']
	elif host_machine.system() == 'linux'
		linker_args += '-fuse-ld=lld'
		add_args = ['-target', 'x86_64-linux', '-fuse-ld=lld', '-flto']
	endif
	foreach p :  add_args
		compile_args += p
		linker_args += p
	endforeach
endif

foreach p : libraries
	linker_args += '-L'+p
endforeach

message(compile_args)
message(linker_args)
message(libraries)
message(includes)

shared_library('m0dular', files(file_list), cpp_args : compile_args, link_args : linker_args, include_directories : include_directories(includes))
