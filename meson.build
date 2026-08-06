project('zenex', 'cpp',
    version : '0.1.0',
    license : 'MIT',
    meson_version : '>=1.0.0',
    default_options : [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

conf = configuration_data()
conf.set('PROJECT_VERSION', meson.project_version())
version_h = configure_file(
    input: 'version.h.in',
    output: 'version.h',
    configuration: conf,
    install_dir: join_paths(get_option('includedir'), 'zenex'),
    install: true,
)
cpp = meson.get_compiler('cpp')

# --------------------------------------------------------------------------
# Toolchain check. Meson locks in the compiler at configure time via the
# native/cross file, so this cannot swap toolchains on the fly, only give
# the person a clear, actionable path to the right one.
# --------------------------------------------------------------------------
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

# --------------------------------------------------------------------------
# Build type flags
# --------------------------------------------------------------------------
is_debug = get_option('buildtype').startswith('debug')

if is_debug
    add_project_arguments('-DZENEX_DEBUG', language : 'cpp')
endif

# --------------------------------------------------------------------------
# Warnings. warning_level=3 (set above) gets the standard set; add a few
# Clang specific ones on top. werror is opt in via -Dwerror=true rather than
# forced, so a fresh clone always configures cleanly even if a new warning
# creeps in on a compiler version the code was not tested against.
# --------------------------------------------------------------------------
warning_flags = cpp.get_supported_arguments([
    '-Wshadow',
    '-Wnon-virtual-dtor',
    '-Wold-style-cast',
    '-Wcast-align',
    '-Wunused',
    '-Woverloaded-virtual',
    '-Wconversion',
    '-Wsign-conversion',
    '-Wnull-dereference',
    '-Wdouble-promotion',
])
add_project_arguments(warning_flags, language : 'cpp')

if get_option('werror')
    add_project_arguments('-Werror', language : 'cpp')
endif

# --------------------------------------------------------------------------
# Sanitizers, debug builds only, opt in. Kept separate from buildtype so a
# normal debug build stays fast and only pays the sanitizer cost when asked.
# --------------------------------------------------------------------------
sanitize = get_option('sanitizer')
if sanitize != 'none'
    sanitize_flags = ['-fsanitize=' + sanitize, '-fno-omit-frame-pointer']
    add_project_arguments(sanitize_flags, language : 'cpp')
    add_project_link_arguments(sanitize_flags, language : 'cpp')
endif

# --------------------------------------------------------------------------
# LTO, release builds only, opt in.
# --------------------------------------------------------------------------
if get_option('lto') and not is_debug
    add_project_arguments('-flto', language : 'cpp')
    add_project_link_arguments('-flto', language : 'cpp')
endif

# --------------------------------------------------------------------------
# Optional developer tooling. These must never block a normal configure if
# they are missing, so required is always false and absence is only noted
# in the summary, not treated as an error.
# --------------------------------------------------------------------------
clang_format = find_program('clang-format', required : false)
clang_tidy   = find_program('clang-tidy', required : false)

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
zenex_inc = include_directories('.')

zenex_sources = files(
    'source/lex/lexer.cpp',
    'source/lex/lexer_debug.cpp',
    'source/pratt/parser.cpp',
)

# Unity builds batch multiple .cpp files into one translation unit for a
# faster from-clean build, at the cost of losing some incremental rebuild
# granularity. It is a first class Meson feature, not a custom addition,
# toggle with -Dunity=on or -Dunity=subprojects at configure time.
zenex_lib = static_library('zenex',
    zenex_sources,
    include_directories : zenex_inc,
    install : true,
)

zenex_dep = declare_dependency(
    link_with : zenex_lib,
    include_directories : zenex_inc,
)

# --------------------------------------------------------------------------
# Install rules and pkg-config, so zenex behaves like a real installable
# library rather than something only usable from inside this exact tree.
# --------------------------------------------------------------------------
install_subdir('zenex', install_dir : get_option('includedir'))

pkg = import('pkgconfig')
pkg.generate(zenex_lib,
    name : 'zenex',
    description : 'A lexer and parser library for C++20',
    version : meson.project_version(),
)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
zenex_test = executable('zenex_test',
    '.tests/main.cpp',
    dependencies : zenex_dep,
)

test('zenex_test', zenex_test,
    timeout : 60,
    is_parallel : false,
)

# --------------------------------------------------------------------------
# Summary, printed at the end of `meson setup`, so the resolved
# configuration is visible at a glance instead of buried in flags.
# --------------------------------------------------------------------------
summary({
    'compiler'   : cpp.get_id() + ' ' + cpp.version(),
    'buildtype'  : get_option('buildtype'),
    'unity'      : get_option('unity'),
    'werror'     : get_option('werror'),
    'sanitizer'  : sanitize,
    'lto'        : get_option('lto'),
    'clang-format' : clang_format.found() ? clang_format.full_path() : 'not found',
    'clang-tidy'   : clang_tidy.found() ? clang_tidy.full_path() : 'not found',
}, section : 'zenex')
