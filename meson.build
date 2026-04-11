project(
  'jack-warden',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2'
  ]
)

inc = include_directories('src')

udev_dep = dependency('libudev')

executable(
  'jackwarden',
  [
    'src/main.cpp',
    'src/jack_handler.cpp',
  ],
  dependencies: [udev_dep],
  include_directories: inc,
  install: true
)