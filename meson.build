# Basic library project.
#
# All of the commonly modified parts are above the line. Generally, you
# shouldn't need to modify anything below the line until your project becomes
# fairly complex.

project(
  'tegra_udrm_gbm',
  'c',
  version : '1.0.0',
  default_options : ['warning_level=3']
)
project_description = 'mesa libgbm backend for libnvgbm'

project_headers = [
  'tegra_udrm_gbm_int.h'
]

project_source_files = [
  'tegra_udrm_gbm.c'
]

cc = meson.get_compiler('c')

project_dependencies = [
  dependency('libdrm')
]

build_args = [
]


# ===================================================================

# ======
# Target
# ======

build_args += [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
]

# Only make public interfaces visible
build_args += '-DMYLIB_PUBLIC=__attribute__((visibility("default"))) -Wno-pedantic'

project_target = shared_library(
  meson.project_name(),
  project_source_files,
  dependencies: project_dependencies,
  install : true,
  c_args : build_args,
  gnu_symbol_visibility : 'hidden',
)


# =======
# Project
# =======

# Make this library usable as a Meson subproject.
project_dep = declare_dependency(
  link_with : project_target
)
set_variable(meson.project_name() + '_dep', project_dep)

# Make this library usable from the system's
# package manager.
install_headers(project_headers, subdir : meson.project_name())

#pkg_mod = import('pkgconfig')
#pkg_mod.generate(
#  name : meson.project_name(),
#  filebase : meson.project_name(),
#  description : project_description,
#  subdirs : meson.project_name(),
#  libraries : project_target,
#)
