project('m0dular-csgo', 'cpp', 'c', default_options : ['c_std=c11', 'cpp_std=c++17', 'b_ndebug=if-release'])

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'cpp')
endif

add_project_arguments('-DPT_VISUALS', language : 'cpp')

compile_args = ['-DMAX_PLAYERS=64', '-DMAX_BONES=128', '-DBACKTRACK_TICKS=64', '-DRESOLVEBASECOUNT=7', '-DMULTIPOINT_COUNT=8', '-DNUM_THREADS=4']
if not get_option('buildtype').startswith('debug')
	compile_args += ['-flto']
endif
#pedantic_args = ['-Wpedantic', '-Wno-language-extension-token', '-Wno-gnu-anonymous-struct', '-Wno-extra-semi', '-Wno-gnu-include-next', '-Wno-nested-anon-types', '-Wno-c99-extensions', '-Wno-microsoft-enum-value', '-Wno-dollar-in-identifier-extension']
linker_args = compile_args

if build_machine.system() != 'windows'
	compile_args += ['-fno-rtti', '-Wno-non-virtual-dtor', '-Wno-strict-aliasing']
	linker_args += ['-fno-rtti']
endif

core_files = ['core/hooks.cpp', 'core/fw_bridge.cpp', 'core/engine.cpp', 'core/spread.cpp', 'core/visuals.cpp', 'core/impacts.cpp', 'core/resolver.cpp']
interface_files = ['interfaces/tracing.cpp']
framework_files = ['sdk/framework/utils/threading.cpp', 'sdk/framework/utils/mutex.cpp', 'sdk/framework/utils/pattern_scan.cpp', 'sdk/framework/utils/semaphores.cpp', 'sdk/framework/utils/handles.cpp', 'sdk/framework/utils/vfhook.cpp', 'sdk/source_csgo/baseentity.cpp', 'sdk/source_shared/netvars.cpp', 'sdk/framework/utils/crc32.cpp', 'sdk/framework/utils/md5.cpp', 'sdk/framework/features/aimbot.cpp', 'sdk/features/resolver.cpp', 'sdk/framework/utils/intersect.cpp']

#core/init.cpp has to be the last one to have proper initialization
file_list = core_files + framework_files + interface_files + ['core/init.cpp']

includes = []
libraries = []
deps = []

cc = meson.get_compiler('cpp')

if host_machine.system() != 'windows'
	compile_args += '-fvisibility=hidden'
	linker_args += '-fvisibility=hidden'
endif

if meson.is_cross_build()
	add_args = []
	if host_machine.system() == 'windows'
		cpu_family = host_machine.cpu_family()
		if build_machine.system() != 'windows'
			linker_args += '-fuse-ld=lld'
			add_args = ['-target', 'i386-pc-windows-msvc19.14.26428', '-Wno-msvc-not-found', '-D_CRT_SECURE_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS', '-fms-extensions', '-fms-compatibility', '-fms-compatibility-version=19.14.26428', '-Wno-expansion-to-defined', '-Wno-nonportable-include-path', '-Wno-pragma-pack', '-Wno-ignored-attributes', '-Wno-ignored-pragma-intrinsic', '-Wno-int-to-void-pointer-cast', '-fuse-ld=lld', '-flto', '-Wno-non-virtual-dtor', '-Wno-unused-local-typedef', '-Wno-unknown-pragmas']

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
		else
			add_args = []
			linker_args += '-Wl,-debug:full'
			linker_args += '-Wl,-v'
		endif
		linker_args += '-Bdynamic'
		linker_args += '-lkernel32'
		linker_args += '-lmsvcrt'
	elif host_machine.system() == 'linux'
		linker_args += '-fuse-ld=lld'
		add_args = ['-target', 'x86_64-linux', '-flto']
	endif
	foreach p :  add_args
		compile_args += p
		linker_args += p
	endforeach
endif

if host_machine.system() == 'linux'
  linker_args += '-ldl'
endif

foreach p : libraries
	linker_args += '-L'+p
endforeach

foreach p : includes
	compile_args += '-I'+p
endforeach

#message(compile_args)
#message(linker_args)
#message(libraries)
#message(includes)

thread_dep = dependency('threads')

shared_library('m0dular', files(file_list), cpp_args : compile_args, link_args : linker_args, dependencies : thread_dep)
