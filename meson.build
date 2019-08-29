project('m0dular-csgo', 'cpp', 'c', default_options : ['c_std=c11', 'cpp_std=c++17', 'b_ndebug=if-release'])

pymod = import('python')
py_installation = pymod.find_installation()

randomize_link_order = get_option('randomize_link_order')

#TODO: Enable warnings for as many things as possible
compile_args = [] #['-Weverything', '-Wno-padded', '-Wno-cast-align', '-Wno-weak-vtables', '-Wno-c++98-compat', '-Wno-old-style-cast']
linker_args = []
combined_args = []
external_link = []
lib_dependencies = []
global_defines = ['AIMBOT_THREADING', '_HAS_STATIC_RTTI=0']

if get_option('testing_features') == true
  global_defines += ['TESTING_FEATURES', 'PT_VISUALS']
endif

if get_option('performance_profiling') == true
  global_defines += ['MTR_ENABLED=1']
endif


network_links = ['-lcrypto', '-lssl', '-lz', '-lboost_system']

core_files = ['core_unity/main_stub.cpp', 'core_unity/tracing_all.cpp', 'features/spread.cpp', 'features/visuals.cpp', 'features/impacts.cpp', 'features/resolver.cpp', 'features/antiaim.cpp', 'features/cameramodes.cpp', 'features/nosmoke.cpp', 'features/aimlog.cpp', 'features/legitbot.cpp', 'features/ragebot.cpp', 'features/glow.cpp', 'core/temporary_animations.cpp', 'core/settings.cpp', 'core/binds.cpp', 'core/shmfs.cpp', 'impl/aimbot.cpp']
gui_files = ['gui/gamemenu.cpp', 'gui/menu/menu.cpp', 'gui/menu/imgui/imgui.cpp', 'gui/menu/imgui/imgui_widgets.cpp', 'gui/menu/imgui/imgui_draw.cpp']

framework_thread_utils = ['core_unity/framework_threads.cpp']
framework_other_utils = ['core_unity/framework_other_utils.cpp']
framework_other_files = ['sdk/source_csgo/baseentity.cpp', 'sdk/source_shared/netvars.cpp',  'sdk/framework/features/aimbot.cpp', 'sdk/features/resolver.cpp']
keycode_files = ['modules/keycode/keyid.c']

#core/init.cpp has to be the last one to have proper initialization
file_list = core_files + framework_other_files

menulib_files = ['core/settings.cpp', 'core/binds.cpp', 'core/shmfs.cpp', 'menu_lib/csgo_module.cpp', 'menu_lib/settings.cpp']
client_file_list = ['client/main.cpp', 'client/loader.cpp', 'client/windows_loader.cpp', 'client/server_comm.cpp', 'client/server_comm_handlers.cpp']

if get_option('buildtype').startswith('debug')
  global_defines += 'DEBUG'
endif

#pedantic_args = ['-Wpedantic', '-Wno-language-extension-token', '-Wno-gnu-anonymous-struct', '-Wno-extra-semi', '-Wno-gnu-include-next', '-Wno-nested-anon-types', '-Wno-c99-extensions', '-Wno-microsoft-enum-value', '-Wno-dollar-in-identifier-extension']

includes = ['pch', 'gui/menu/imgui']
system_includes = []
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
      combined_args += ['-target', 'i386-pc-windows-msvc19.14.26428', '-Wno-msvc-not-found', '-D_CRT_SECURE_NO_WARNINGS', '-D_WINSOCK_DEPRECATED_NO_WARNINGS', '-fms-extensions', '-fms-compatibility', '-fms-compatibility-version=19.14.26428', '-fuse-ld=lld', '-Wno-non-virtual-dtor']

      bf_dir = get_option('msvc_dir')

      bf_includes = ['clang', 'msvc', 'ucrt', 'um', 'shared', 'winrt', 'boost', 'openssl', 'zlib']
      bf_libraries = ['clang']
      bf_libraries_cpu = ['msvc', 'ucrt', 'um', 'shared', 'winrt', 'openssl', 'boost', 'zlib']

      foreach p : bf_includes
        system_includes += join_paths(bf_dir, 'include', p)
      endforeach

      foreach p : bf_libraries
        libraries += join_paths(bf_dir, 'lib', p)
      endforeach

      foreach p : bf_libraries_cpu
        libraries += join_paths(bf_dir, 'lib', p, cpu_family)
      endforeach

      linker_args += '-Wl,-lldmap'
    endif
  elif host_machine.system() == 'linux'
    linker_args += '-fuse-ld=lld'
    combined_args += ['-target', 'x86_64-linux']
  endif
endif

add_inc_dir = get_option('additional_include_dir')

if add_inc_dir != ''
  includes += [add_inc_dir]
endif

#Global compile time and link-time arguments
if cc.get_id() == 'msvc'
  compile_args += ['-FIglobal_defines.h', '-std:c++17', '-GR-']

  linker_args += '-MACHINE:X86'
else
  compile_args += ['-include', 'global_defines.h']
  compile_args += ['-fno-rtti', '-Wno-non-virtual-dtor', '-Wno-strict-aliasing']

  foreach p : libraries
    linker_args += '-L'+p
  endforeach
endif

if host_machine.system() == 'linux' or host_machine.system() == 'darwin'
  gui_files += ['gui/menu/sdlmenu.cpp', 'gui/menu/imgui/examples/imgui_impl_opengl2.cpp', 'gui/menu/imgui/examples/imgui_impl_sdl.cpp']
  lib_dependencies += [dependency('sdl2'), dependency('gl')]
  external_link += ['dl', 'rt']
  linker_args += ['-Wl,-Map=libm0dular.map', '-fvisibility=hidden']
  compile_args += '-fvisibility=hidden'
else
  gui_files += ['gui/menu/dx9menu.cpp', 'gui/menu/imgui/examples/imgui_impl_dx9.cpp', 'gui/menu/imgui/examples/imgui_impl_win32.cpp']
  external_link += ['kernel32', 'd3d9', 'user32']
  if build_machine.system() != 'windows'
    linker_args += ['-Wl,-debugtype:all', '-Wl,-debug:full']
  endif
  network_links += ['-lcrypt32']
endif

#Release type compile time and link time arguments
if not get_option('buildtype').startswith('debug')
  if cc.get_id() != 'msvc'
    compile_args += ['-flto=thin', '-gsplit-dwarf', '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-fno-math-errno', '-fno-ident']
    if cc.get_id() != 'gcc'
      compile_args += ['-mllvm', '-inline-threshold=500', '-fvisibility=hidden']
      linker_args += ['-fuse-ld=lld', '-flto=thin']
    endif
    linker_args += ['-fno-rtti']
  else

  endif

  if host_machine.system() != 'windows'
    linker_args += ['-Wl,--strip-all', '-Wl,--discard-all', '-Wl,--gc-sections']
    #compile_args += ['-fsanitize=address']
    #external_link += ['asan']
  else
    if build_machine.system() != 'windows'
      linker_args += ['-Wl,-safeseh:no', '-Wl,-opt:REF', '-O2', '-Wl,-pdbaltpath:/home/buildslave/csgo/libm0dular.pdb']
    endif
    external_link += ['msvcrt']
  endif
else #Debug type compile time and link time arguments
  if host_machine.system() != 'windows'
    compile_args += ['-g', '-O0']#, '-fsanitize=address']
    #external_link += ['asan']
  else
    external_link += ['msvcrtd']
  endif

endif

foreach p : global_defines
  add_project_arguments('-D'+p, language : 'cpp')
  add_project_arguments('-D'+p, language : 'c')
endforeach

incdir = [include_directories(includes)]

#include_directories(is_system: true) fails to compile
if cc.get_id() != 'msvc'
  foreach d : system_includes
    compile_args += ['-isystem'+d]
  endforeach
endif

if build_machine.system() != 'windows'
  linker_args += '-Bdynamic'
endif

foreach p : external_link
  linker_args += '-l' + p
endforeach

foreach p :  combined_args
  compile_args += p
  linker_args += p
endforeach

thread_dep = dependency('threads')
py3_dep = dependency('python3', required: false)

client_add_compile_args = ['-DM0DULAR_CLIENT']

use_static_libs = get_option('use_static_libs')

lib_link_with = []
menulib_link_with = []

lib_file_list = file_list

#Menulib does not need the ImGui menu (for now)
lib_file_list += gui_files

module_identification = get_option('module_identification')

module_id = static_library('module_id', files(['bits/identify.cpp']), cpp_args: compile_args + ['-DMODULE_IDENTIFICATION="'+module_identification+'"'])
keycode_clib = static_library('keycode_clib', files(keycode_files), c_args: compile_args)

if use_static_libs
  if randomize_link_order
    shuffled_files = run_command('./random_shuffle.sh', framework_other_utils)
    framework_other_utils = shuffled_files.stdout().split(' ')
  endif

  fwlib = static_library('m0dular-framework', files(framework_thread_utils + framework_other_utils), c_args : compile_args, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : [incdir], c_pch: [], cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

  lib_link_with = [fwlib]
  menulib_link_with = [fwlib]
else
  lib_file_list += framework_thread_utils + framework_other_utils
  menulib_files += framework_other_utils
endif

lib_link_with += [module_id, keycode_clib]
menulib_link_with += [module_id, keycode_clib]

build_all_instruction_variants = get_option('build_all_instruction_variants')

if randomize_link_order
  shuffled_files = run_command('./random_shuffle.sh', lib_file_list)
  lib_file_list = shuffled_files.stdout().split(' ')
endif

if build_all_instruction_variants
  foreach instr : [['sse2', ['-msse2', '-mno-sse3']], ['sse3', ['-msse3', '-mno-sse4.2']], ['sse4', ['-msse4.2']], ['avx', ['-mavx', '-mno-avx2']], ['avx2', ['-mavx2']], ['avx512', ['-mavx512f', '-mavx512cd', '-mavx512er']]]
    shared_library('m0dular-' + instr[0], files(lib_file_list + ['core/init.cpp']), c_args : compile_args + ['-DM0DULAR_CHEAT'] + instr[1], cpp_args : compile_args + ['-DM0DULAR_CHEAT'] + instr[1], link_args : linker_args, dependencies : [thread_dep] + lib_dependencies, link_with : lib_link_with, include_directories : [incdir], c_pch: [], cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])
  endforeach
else
  lib = shared_library('m0dular', files(lib_file_list + ['core/init.cpp']), c_args : compile_args + ['-DM0DULAR_CHEAT'], cpp_args : compile_args + ['-DM0DULAR_CHEAT'], link_args : linker_args, dependencies : [thread_dep] + lib_dependencies, link_with : lib_link_with, include_directories : [incdir], c_pch: [], cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])
endif

menulib = shared_library('mlm0dular', files(menulib_files), c_args : compile_args + client_add_compile_args, cpp_args : compile_args + client_add_compile_args, link_args : linker_args, dependencies : [thread_dep], link_with : menulib_link_with, include_directories : [incdir], c_pch: [], cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

bind_test = executable('keybind_test', ['core/settings.cpp', 'core/binds.cpp', 'core/shmfs.cpp', 'tests/binds.cpp'] + framework_thread_utils, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : [incdir], c_pch: [], cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

test('keybinds', bind_test)
