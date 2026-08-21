project('zenex', 'cpp',
    version      : '1.0.5',
    license      : 'MIT',
    meson_version: '>=1.0.0',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

cpp    = meson.get_compiler('cpp')
python = find_program('python3', required : false)
if not python.found()
    python = find_program('python')
endif

build_root = meson.project_build_root()
dist_dir   = join_paths(build_root, 'dist')
test_dir   = join_paths(build_root, 'test')

if host_machine.system() == 'windows'
    plat_suffix = 'win64'
    static_ext  = '.lib'
    shared_ext  = '.dll'
    exe_ext     = '.exe'
elif host_machine.system() == 'linux'
    plat_suffix = 'linux64'
    static_ext  = '.a'
    shared_ext  = '.so'
    exe_ext     = ''
else
    plat_suffix = 'unknown'
    static_ext  = '.a'
    shared_ext  = '.so'
    exe_ext     = ''
endif

copy_rename_script = '''
import shutil, os, sys
src, dest_dir, dest_name = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(dest_dir, exist_ok=True)
shutil.copy(src, os.path.join(dest_dir, dest_name))
open(sys.argv[4], "w").close()
'''

conf = configuration_data()
conf.set('PROJECT_VERSION', meson.project_version())

version_h = configure_file(
    input        : 'version.h.in',
    output       : 'version.h',
    configuration: conf,
    install_dir  : join_paths(get_option('includedir'), 'zenex'),
    install      : true,
)

if cpp.get_id() != 'clang' and cpp.get_id() != 'clang-cl'
    error(
        '\n\nzenex requires Clang and detected "' + cpp.get_id() + '" instead.\n\n' +
        'Configure with the matching native file for your platform:\n' +
        '  Windows : meson setup build --native-file build-cfg/windows-clang.ini\n' +
        '  Linux   : meson setup build --native-file build-cfg/linux-clang.ini\n\n' +
        'If Clang is not installed yet:\n' +
        '  Windows : winget install LLVM.LLVM\n' +
        '  Arch    : sudo pacman -S clang\n' +
        '  Ubuntu / Debian / Mint : sudo apt install clang\n' +
        '  Fedora  : sudo dnf install clang\n'
    )
endif

is_debug = get_option('buildtype').startswith('debug')

if is_debug
    add_project_arguments('-DZENEX_DEBUG', language : 'cpp')
endif

warning_flags = cpp.get_supported_arguments([
    '-Wshadow', '-Wnon-virtual-dtor', '-Wold-style-cast', '-Wcast-align',
    '-Wunused', '-Woverloaded-virtual', '-Wconversion', '-Wsign-conversion',
    '-Wnull-dereference', '-Wdouble-promotion',
])
add_project_arguments(warning_flags, language : 'cpp')

if get_option('werror')
    add_project_arguments('-Werror', language : 'cpp')
endif

sanitize = get_option('sanitizer')
if sanitize != 'none'
    sanitize_flags = ['-fsanitize=' + sanitize, '-fno-omit-frame-pointer']
    add_project_arguments(sanitize_flags, language : 'cpp')
    add_project_link_arguments(sanitize_flags, language : 'cpp')
endif

if get_option('lto') and not is_debug
    add_project_arguments('-flto', language : 'cpp')
    add_project_link_arguments('-flto', language : 'cpp')
endif

clang_format = find_program('clang-format', required : false)
clang_tidy   = find_program('clang-tidy', required : false)

zenex_inc = include_directories('.')

zenex_sources = files(
    'source/lexer.cpp',
)

building_dll_args = ['-DZENEX_BUILDING_DLL']

zenex_static = static_library('zenex_static', zenex_sources,
    include_directories : zenex_inc,
    cpp_args            : building_dll_args,
    install             : true,
)

zenex_dynamic = shared_library('zenex_dynamic', zenex_sources,
    include_directories : zenex_inc,
    cpp_args            : building_dll_args,
    install             : true,
)

static_consume_args = ['-DZENEX_STATIC']

zenex_dep = declare_dependency(
    link_with           : zenex_static,
    include_directories : zenex_inc,
    compile_args        : static_consume_args,
)

zenex_dynamic_dep = declare_dependency(
    link_with           : zenex_dynamic,
    include_directories : zenex_inc,
)

ver = meson.project_version()
name_prefix = 'zenex-' + ver + '-' + plat_suffix

custom_target('zenex_static_dist',
    input  : zenex_static,
    output : 'zenex_static.stamp',
    command: [python, '-c', copy_rename_script, '@INPUT@', dist_dir,
              name_prefix + '-static' + static_ext, '@OUTPUT@'],
    build_by_default: true,
)

custom_target('zenex_dynamic_dist',
    input  : zenex_dynamic,
    output : 'zenex_dynamic.stamp',
    command: [python, '-c', copy_rename_script, '@INPUT@', dist_dir,
              name_prefix + '-dynamic' + shared_ext, '@OUTPUT@'],
    build_by_default: true,
)

install_subdir('zenex', install_dir : get_option('includedir'))

pkg = import('pkgconfig')
pkg.generate(zenex_static,
    name       : 'zenex',
    description: 'A lexer and parser library for C++20',
    version    : meson.project_version(),
)

zenex_test = executable('ztest',
    '.tests/test.cpp',
    dependencies : zenex_dep,
)

custom_target('test_dist',
    input  : zenex_test,
    output : 'test.stamp',
    command: [python, '-c', copy_rename_script, '@INPUT@', test_dir,
              'test-' + ver + '-' + plat_suffix + exe_ext, '@OUTPUT@'],
    build_by_default: true,
)

test('zenex_test', zenex_test,
    timeout    : 60,
    is_parallel: false,
)

summary({
    'compiler'    : cpp.get_id() + ' ' + cpp.version(),
    'buildtype'   : get_option('buildtype'),
    'unity'       : get_option('unity'),
    'werror'      : get_option('werror'),
    'sanitizer'   : sanitize,
    'lto'         : get_option('lto'),
    'clang-format': clang_format.found() ? clang_format.full_path() : 'not found',
    'clang-tidy'  : clang_tidy.found()   ? clang_tidy.full_path()   : 'not found',
}, section : 'zenex')
