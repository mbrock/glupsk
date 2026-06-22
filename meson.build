project(
  'glupsk',
  'cpp',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
  ],
)

root_inc = include_directories('.')

is_wasm = host_machine.cpu_family() == 'wasm32'
core_cpp_args = [
  '-DGLUPSK_ENABLE_FILESYSTEM=' + (is_wasm ? '0' : '1'),
]

core_lib = static_library(
  'glupsk_core',
  files(
    'core/decode.cpp',
    'core/execute.cpp',
    'core/glk_events.cpp',
    'core/glk_names.cpp',
    'core/glk_text.cpp',
    'core/machine.cpp',
    'core/opcode_meta.cpp',
    'core/story.cpp',
    'core/vm_accel.cpp',
    'core/vm_frames.cpp',
    'core/vm_operands.cpp',
    'core/vm_search.cpp',
    'core/vm_strings.cpp',
  ),
  include_directories: root_inc,
  cpp_args: core_cpp_args,
)

if is_wasm
  executable(
    'glupsk',
    files('hosts/wasm/glupsk_wasm.cpp'),
    include_directories: root_inc,
    cpp_args: core_cpp_args,
    link_with: core_lib,
    name_suffix: 'wasm',
    link_args: [
      '-Wl,--no-entry',
      '-Wl,--allow-undefined',
      '-Wl,--export-dynamic',
    ],
  )
else
  glupsk_info = executable(
    'glupsk-info',
    files('tools/glupsk-info.cpp'),
    include_directories: root_inc,
    link_with: core_lib,
  )

  glupsk_trace = executable(
    'glupsk-trace',
    files('tools/glupsk-trace.cpp'),
    include_directories: root_inc,
    link_with: core_lib,
  )

  glupsk_play = executable(
    'glupsk-play',
    files('tools/glupsk-play.cpp'),
    include_directories: root_inc,
    link_with: core_lib,
  )

  test_story = executable(
    'test_story',
    files(
      'tests/test-main.cpp',
      'tests/test_decode.cpp',
      'tests/test_execute.cpp',
      'tests/test_machine.cpp',
      'tests/test_story.cpp',
    ),
    include_directories: root_inc,
    link_with: core_lib,
  )

  test(
    'story',
    test_story,
    workdir: meson.project_source_root(),
  )
endif
