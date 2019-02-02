project('m0dular-csgo', 'cpp', 'c', default_options : ['c_std=c11', 'cpp_std=c++17', 'b_ndebug=if-release'])

pymod = import('python')
py_installation = pymod.find_installation()

randomize_link_order = get_option('randomize_link_order')

compile_args = []
linker_args = []
combined_args = []
external_link = []
global_defines = ['PT_VISUALS', 'AIMBOT_THREADING', '_HAS_STATIC_RTTI=0']

if get_option('performance_profiling') == true
  global_defines += ['MTR_ENABLED=1']
endif


network_links = ['-lcrypto', '-lssl', '-lz', '-lboost_system']

infoware_defines = []
infoware_link_args = []

core_files = ['core_unity/main_stub.cpp', 'core_unity/tracing_all.cpp', 'core/spread.cpp', 'core/visuals.cpp', 'core/impacts.cpp', 'core/resolver.cpp', 'core/antiaim.cpp', 'core/temporary_animations.cpp', 'core/settings.cpp', 'core/binds.cpp']

framework_thread_utils = ['core_unity/framework_threads.cpp']
framework_other_utils = ['core_unity/framework_other_utils.cpp']
framework_other_files = ['sdk/source_csgo/baseentity.cpp', 'sdk/source_shared/netvars.cpp',  'sdk/framework/features/aimbot.cpp', 'sdk/features/resolver.cpp']
keycode_files = ['modules/keycode/keyid.c']

#core/init.cpp has to be the last one to have proper initialization
file_list = core_files + framework_other_files + keycode_files

gamelib_files = ['core/settings.cpp', 'core/binds.cpp', 'client/csgo_module.cpp', 'client/settings.cpp']
client_file_list = gamelib_files + ['client/main.cpp', 'client/loader.cpp', 'client/windows_loader.cpp', 'client/server_comm.cpp', 'client/server_comm_handlers.cpp'] + keycode_files

infoware_comp_files = [
  'system/memory/memory_windows.cpp',
  'system/memory/memory_non_windows.cpp',
  'system/amounts/windows.cpp',
  'system/amounts/amounts_non_windows.cpp',
  'gpu/memory/d3d.cpp',
  'gpu/memory/blank_all.cpp',
  'gpu/memory/OpenGL.cpp',
  'gpu/memory/OpenCL.cpp',
  'cpu/frequency/frequency_non_windows.cpp',
  'cpu/frequency/frequency_windows.cpp',
  'cpu/quantities_cache/quantities_cache_windows.cpp',
  'cpu/quantities_cache/quantities_cache_non_windows.cpp',
  'cpu/architecture/architecture_windows.cpp',
  'cpu/architecture/architecture_non_windows.cpp',
  'cpu/endianness/all.cpp',
  'cpu/vendor_model_name/vendor_model_name_windows.cpp',
  'cpu/vendor_model_name/vendor_id.cpp',
  'cpu/vendor_model_name/vendor_model_name_non_windows.cpp',
  'cpu/instuction_set/instruction_set_non_windows.cpp',
  'cpu/instuction_set/instruction_set.cpp',
  'cpu/instuction_set/instruction_set_windows.cpp',
  'detail/cpuid.cpp',
  'detail/pci.generated.cpp',
  'detail/scope.cpp'
]

infoware_files = []

foreach f : infoware_comp_files
  infoware_files += [join_paths('modules/infoware/src', f)]
endforeach

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

      bf_includes = ['clang', 'msvc', 'ucrt', 'um', 'shared', 'winrt', 'boost', 'openssl', 'zlib']
      bf_libraries = ['clang']
      bf_libraries_cpu = ['msvc', 'ucrt', 'um', 'shared', 'winrt', 'openssl', 'boost', 'zlib']

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
  #We need to patch infoware as there are syntax errors. Linux/OSX loader support with GPU HWID is non-essential at this stage
  #infoware_defines += ['-DINFOWARE_USE_OPENCL']
  #infoware_link_args += ['-lOpenCL']
  external_link += ['dl', 'rt']
  linker_args += ['-Wl,-Map=libm0dular.map', '-fvisibility=hidden']
  compile_args += '-fvisibility=hidden'
else
  infoware_defines += ['-DINFOWARE_USE_D3D']
  infoware_link_args += ['-ldxgi']
  external_link += ['kernel32', 'msvcrt', 'd3d9', 'user32']
  linker_args += ['-Wl,-debugtype:all', '-Wl,-debug:full']
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
    linker_args += ['-Wl,-safeseh:no', '-Wl,-opt:REF', '-O2', '-Wl,-pdbaltpath:/home/buildslave/csgo/libm0dular.pdb']
  endif
else #Debug type compile time and link time arguments
  compile_args += ['-g', '-O0']
  #external_link += ['asan']

endif

foreach p : global_defines
  add_project_arguments('-D'+p, language : 'cpp')
  add_project_arguments('-D'+p, language : 'c')
endforeach

includes += ['modules/websocketpp', 'modules/infoware/include']

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
py3_dep = dependency('python3', required: false)

client_add_compile_args = ['-DM0DULAR_CLIENT']

if py3_dep.found()
  reloc_module = py_installation.extension_module('relocate', ['client/windows_loader.cpp', 'client/py_win_reloc.cpp', 'sdk/framework/utils/crc32.cpp'], dependencies: py3_dep)
endif

use_static_libs = get_option('use_static_libs')

lib_link_with = []
client_link_with = []

lib_file_list = file_list

module_identification = get_option('module_identification')

module_id = static_library('module_id', files(['bits/identify.cpp']), cpp_args: compile_args + ['-DMODULE_IDENTIFICATION="'+module_identification+'"'])

if use_static_libs
  if randomize_link_order
    shuffled_files = run_command('./random_shuffle.sh', infoware_files)
    infoware_files = shuffled_files.stdout().split(' ')
  endif

  infoware_lib = static_library('infoware', files(infoware_files), c_args : compile_args + infoware_defines, cpp_args : compile_args + infoware_defines, link_args : linker_args + infoware_link_args, include_directories : incdir)

  if randomize_link_order
    shuffled_files = run_command('./random_shuffle.sh', framework_other_utils)
    framework_other_utils = shuffled_files.stdout().split(' ')
  endif

  fwlib = static_library('m0dular-framework', files(framework_thread_utils + framework_other_utils), c_args : compile_args, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : incdir, cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

  lib_link_with = [fwlib]
  client_link_with = [fwlib, infoware_lib]
else
  lib_file_list += framework_thread_utils + framework_other_utils
  client_file_list += framework_thread_utils + framework_other_utils + infoware_files
  client_add_compile_args += infoware_defines
endif

lib_link_with += [module_id]

build_all_instruction_variants = get_option('build_all_instruction_variants')

if randomize_link_order
  shuffled_files = run_command('./random_shuffle.sh', lib_file_list)
  lib_file_list = shuffled_files.stdout().split(' ')
endif

if build_all_instruction_variants
  foreach instr : [['sse2', ['-msse2', '-mno-sse3']], ['sse3', ['-msse3', '-mno-sse4.2']], ['sse4', ['-msse4.2']], ['avx', ['-mavx', '-mno-avx2']], ['avx2', ['-mavx2']], ['avx512', ['-mavx512f', '-mavx512cd', '-mavx512er']]]
    shared_library('m0dular-' + instr[0], files(lib_file_list + ['core/init.cpp']), c_args : compile_args + ['-DM0DULAR_CHEAT'] + instr[1], cpp_args : compile_args + ['-DM0DULAR_CHEAT'] + instr[1], link_args : linker_args, dependencies : [thread_dep], link_with : lib_link_with, include_directories : incdir, cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])
  endforeach
else
  lib = shared_library('m0dular', files(lib_file_list + ['core/init.cpp']), c_args : compile_args + ['-DM0DULAR_CHEAT'], cpp_args : compile_args + ['-DM0DULAR_CHEAT'], link_args : linker_args, dependencies : [thread_dep], link_with : lib_link_with, include_directories : incdir, cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])
endif

if randomize_link_order
  shuffled_files = run_command('./random_shuffle.sh', client_file_list)
  client_file_list = shuffled_files.stdout().split(' ')
endif

client = executable('clm0dular', files(client_file_list), c_args : compile_args + client_add_compile_args, cpp_args : compile_args + client_add_compile_args, link_args : linker_args + network_links + infoware_link_args, dependencies : [thread_dep], link_with : client_link_with, include_directories : incdir, cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

bind_test = executable('keybind_test', ['core/settings.cpp', 'core/binds.cpp', 'tests/binds.cpp'] + framework_thread_utils, cpp_args : compile_args, link_args : linker_args, dependencies : [thread_dep], include_directories : incdir, cpp_pch: ['pch/cpp_pch.h', 'pch/cpp_pch.cpp'])

test('keybinds', bind_test)
