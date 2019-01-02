project('m0dular-csgo', 'cpp', 'c', default_options : ['c_std=c11', 'cpp_std=c++17', 'b_ndebug=if-release'])

compile_args = []
linker_args = []
combined_args = []
external_link = []
global_defines = ['PT_VISUALS', 'AIMBOT_THREADING']

core_files = ['core_unity/main_stub.cpp', 'core_unity/tracing_all.cpp', 'core/spread.cpp', 'core/visuals.cpp', 'core/impacts.cpp', 'core/resolver.cpp', 'core/antiaim.cpp', 'core/temporary_animations.cpp', 'core/settings.cpp', 'core/binds.cpp']
framework_thread_utils = ['core_unity/framework_threads.cpp']
framework_other_utils = ['core_unity/framework_other_utils.cpp']
framework_other_files = ['sdk/source_csgo/baseentity.cpp', 'sdk/source_shared/netvars.cpp',  'sdk/framework/features/aimbot.cpp', 'sdk/features/resolver.cpp']
keycode_files = ['modules/keycode/keyid.c']

#core/init.cpp has to be the last one to have proper initialization
file_list = core_files + framework_other_files + keycode_files + ['core/init.cpp']
client_file_list = ['core/settings.cpp', 'core/binds.cpp', 'client/settings.cpp', 'client/main.cpp', 'client/loader.cpp', 'client/windows_loader.cpp'] + keycode_files

if get_option('buildtype').startswith('debug')
  global_defines += 'DEBUG'
endif

#pedantic_args = ['-Wpedantic', '-Wno-language-extension-token', '-Wno-gnu-anonymous-struct', '-Wno-extra-semi', '-Wno-gnu-include-next', '-Wno-nested-anon-types', '-Wno-c99-extensions', '-Wno-microsoft-enum-value', '-Wno-dollar-in-identifier-extension']

includes = []
libraries = []
deps = []

cc = meson.get_compiler('cpp')

#Set up cross-compile support
if meson.is_cross_build()
  if host_machine.system() == 'windows'
    cpu_family = host_machine.cpu_family()
    #In order to build for windows on non-windows OS, we have to have all required libraries and header files in one place. The root path is controlled through msvc_dir option.
    if build_machine.system() != 'windows'
      linker_args += '-fuse-ld=lld'
      combined_args += ['-target', 'i386-pc-windows-msvc19.14.26428', '-Wno-msvc-not-found', '-D_CRT_SECURE_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS', '-fms-extensions', '-fms-compatibility', '-fms-compatibility-version=19.14.26428', '-Wno-expansion-to-defined', '-Wno-nonportable-include-path', '-Wno-pragma-pack', '-Wno-ignored-attributes', '-Wno-ignored-pragma-intrinsic', '-Wno-int-to-void-pointer-cast', '-fuse-ld=lld', '-Wno-non-virtual-dtor', '-Wno-unused-local-typedef', '-Wno-unknown-pragmas', '-Wno-deprecated-declarations', '-Wno-macro-redefined', '-Wno-comment']

      bf_dir = get_option('msvc_dir')

      bf_includes = ['clang', 'msvc', 'ucrt', 'um', 'shared', 'winrt', 'boost']
      bf_libraries = ['clang']
      bf_libraries_cpu = ['msvc', 'ucrt', 'um', 'shared', 'winrt']

      foreach p : bf_includes
        includes += join_paths(bf_dir, 'include', p)
      endforeach

      foreach p : bf_libraries
        libraries += join_paths(bf_dir, 'lib', p)
      endforeach

      foreach p : bf_libraries_cpu
        libraries += join_paths(bf_dir, 'lib', p, cpu_family)
      endforeach

    endif
    linker_args += '-Wl,-lldmap'
  elif host_machine.system() == 'linux'
    linker_args += '-fuse-ld=lld'
    combined_args += ['-target', 'x86_64-linux']
  endif
endif

#Global compile time and link-time arguments
if cc.get_id() == 'msvc'
  compile_args += ['-FIglobal_defines.h', '-std:c++17']
else
  compile_args += ['-include', 'global_defines.h']
  compile_args += ['-fno-rtti', '-Wno-non-virtual-dtor', '-Wno-strict-aliasing']

  foreach p : libraries
    linker_args += '-L'+p
  endforeach
endif

if host_machine.system() == 'linux' or host_machine.system() == 'darwin'
  external_link += ['dl', 'rt']
  linker_args += ['-Wl,-Map=libm0dular.map', '-fvisibility=hidden']
  compile_args += '-fvisibility=hidden'
else
  external_link += ['kernel32', 'msvcrt', 'd3d9', 'user32']
  linker_args += ['-Wl,-debugtype:all', '-Wl,-debug:full']
endif

#Release type compile time and link time arguments
if not get_option('buildtype').startswith('debug')
  if cc.get_id() != 'msvc'
    compile_args += ['-flto=thin', '-Ofast', '-msse4.2', '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-fno-math-errno', '-fno-ident']
    if cc.get_id() != 'gcc'
      compile_args += ['-mllvm', '-inline-threshold=500', '-fvisibility=hidden']
      linker_args += ['-fuse-ld=lld', '-flto=thin']
    endif
    linker_args += ['-fno-rtti']
  else

  endif

  if host_machine.system() != 'windows'
    linker_args += ['-Wl,--strip-all', '-Wl,--discard-all', '-Wl,--gc-sections']
  else
    linker_args += ['-Wl,-safeseh:no', '-Wl,-opt:REF', '-O2']
  endif
else #Debug type compile time and link time arguments
  compile_args += ['-g', '-O0']
  #external_link += ['asan']

  #compile_args += ['-fsanitize=address']
  #external_link += ['asan']
endif

foreach p : global_defines
  add_project_arguments('-D'+p, language : 'cpp')
endforeach

incdir = include_directories(includes)

linker_args += '-Bdynamic'
foreach p : external_link
  linker_args += '-l' + p
endforeach

foreach p :  combined_args
  compile_args += p
  linker_args += p
endforeach

thread_dep = dependency('threads')

fwlib = static_library('m0dular-framework', files(framework_thread_utils + framework_other_utils), c_args : compile_args, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : incdir)
lib = shared_library('m0dular', files(file_list), c_args : compile_args + ['-DM0DULAR_CHEAT'], cpp_args : compile_args + ['-DM0DULAR_CHEAT'], link_args : linker_args, dependencies : [thread_dep], link_with : fwlib, include_directories : incdir)
client = executable('clm0dular', files(client_file_list), c_args : compile_args + ['-DM0DULAR_CLIENT'], cpp_args : compile_args + ['-DM0DULAR_CLIENT'], link_args : linker_args, dependencies : [thread_dep], link_with : fwlib, include_directories : incdir)

bind_test = executable('keybind_test', ['core/settings.cpp', 'core/binds.cpp', 'tests/binds.cpp'] + framework_thread_utils, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : incdir)

test('keybinds', bind_test)
