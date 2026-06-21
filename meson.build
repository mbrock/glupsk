project(
  'glupsk',
  'cpp',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
  ],
)

root_inc = include_directories('.')

core_lib = static_library(
  'glupsk_core',
  files('core/story.cpp'),
  include_directories: root_inc,
)

glupsk_info = executable(
  'glupsk-info',
  files('tools/glupsk-info.cpp'),
  include_directories: root_inc,
  link_with: core_lib,
)

test_story = executable(
  'test_story',
  files('tests/test_story.cpp'),
  include_directories: root_inc,
  link_with: core_lib,
)

test(
  'story',
  test_story,
  workdir: meson.project_source_root(),
)
