project(
  'tegra_udrm_gbm',
  'c',
  version : '1.0.0',
  license : 'MIT',
  default_options : [
    'warning_level=3',
    'buildtype=debugoptimized',
    'c_std=gnu99',
  ],
)
project_description = 'mesa gbm loader for libnvgbm'

project_headers = [
  'tegra_udrm_gbm_int.h'
]

project_source_files = [
  'tegra_udrm_gbm.c'
]

cc = meson.get_compiler('c')

nvbufsurface_dep = cc.find_library('nvbufsurface', required : true)

project_dependencies = [
  dependency('libdrm'),
  nvbufsurface_dep,
  dependency('gbm', version : ['>=21.2.0'])
]

build_args = [
  '-Wno-pedantic',
]

gbm_backends_path = get_option('gbm-backends-path')
if gbm_backends_path == ''
  gbm_backends_path = join_paths(get_option('prefix'), get_option('libdir'), 'gbm')
endif

# ===================================================================

build_args += [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
]

project_target = shared_library(
  'tegra-udrm_gbm',
  project_source_files,
  name_prefix: '',
  dependencies: project_dependencies,
  install : true,
  c_args : build_args,
  gnu_symbol_visibility : 'hidden',
  install : true,
  install_dir : gbm_backends_path,
)
