project('lunasvg', 'cpp',
    version: '3.5.0',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: ['cpp_std=c++17']
)

plutovg_dep = dependency('plutovg',
    required: true,
    version: '>=1.3.0',
    fallback: ['plutovg', 'plutovg_dep']
)

lunasvg_sources = [
    'source/lunasvg.cpp',
    'source/graphics.cpp',
    'source/svgelement.cpp',
    'source/svggeometryelement.cpp',
    'source/svgpaintelement.cpp',
    'source/svgparser.cpp',
    'source/svgproperty.cpp',
    'source/svglayoutstate.cpp',
    'source/svgrenderstate.cpp',
    'source/svgtextelement.cpp'
]

lunasvg_compile_args = []
lunasvg_cpp_args = ['-DLUNASVG_BUILD']

if get_option('default_library') == 'static'
    lunasvg_compile_args += ['-DLUNASVG_BUILD_STATIC']
    lunasvg_cpp_args += ['-DLUNASVG_BUILD_STATIC']
endif

if get_option('load-system-fonts').disabled()
    lunasvg_cpp_args += ['-DLUNASVG_DISABLE_LOAD_SYSTEM_FONTS']
endif

if get_option('external-resources').disabled()
    lunasvg_cpp_args += ['-DLUNASVG_DISABLE_EXTERNAL_RESOURCES']
endif

lunasvg_deps = [plutovg_dep]
harfbuzz_dep = dependency('harfbuzz', version: '>=7.0.0', required: get_option('text-shaping'))
sheenbidi_dep = dependency('sheenbidi', version: '>=2.9.0', required: get_option('text-shaping'))
if harfbuzz_dep.found() and sheenbidi_dep.found()
    lunasvg_deps += [harfbuzz_dep, sheenbidi_dep]
    lunasvg_cpp_args += ['-DLUNASVG_ENABLE_TEXT_SHAPING']
endif

lunasvg_lib = library('lunasvg', lunasvg_sources,
    include_directories: include_directories('include', 'source'),
    dependencies: lunasvg_deps,
    version: meson.project_version(),
    cpp_args: lunasvg_cpp_args,
    gnu_symbol_visibility: 'hidden',
    install: true
)

lunasvg_dep = declare_dependency(
    link_with: lunasvg_lib,
    include_directories: include_directories('include'),
    compile_args: lunasvg_compile_args
)

meson.override_dependency('lunasvg', lunasvg_dep)
install_headers('include/lunasvg.h', subdir: 'lunasvg')

if not get_option('examples').disabled()
    subdir('examples')
endif

pkgmod = import('pkgconfig')
pkgmod.generate(lunasvg_lib,
    name: 'LunaSVG',
    description: 'SVG rendering and manipulation library',
    filebase: 'lunasvg',
    subdirs: 'lunasvg'
)
