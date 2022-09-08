if gudev.found()
cargs = ['-DG_LOG_DOMAIN="FuPluginFocalfp"']

install_data([
  'focalfp.quirk',
  ],
  install_dir: join_paths(datadir, 'fwupd', 'quirks.d')
)

shared_module('fu_plugin_focalfp',
  fu_hash,
  sources: [
    'fu-plugin-focalfp.c',
    'fu-focalfp-firmware.c',   # fuzzing
    'fu-focalfp-hid-device.c',
  ],
  include_directories: [
    root_incdir,
    fwupd_incdir,
    fwupdplugin_incdir,
  ],
  install: true,
  install_dir: plugin_dir,
  c_args: [
    cargs,
    '-DLOCALSTATEDIR="' + localstatedir + '"',
  ],
  link_with: [
    fwupd,
    fwupdplugin,
  ],
  dependencies: [
    plugin_deps,
  ],
)
endif

if get_option('tests')
  install_data(['tests/focalfp.builder.xml'],
    install_dir: join_paths(installed_test_datadir, 'tests'))
  env = environment()
  env.set('G_TEST_SRCDIR', meson.current_source_dir())
  env.set('G_TEST_BUILDDIR', meson.current_build_dir())
  e = executable(
    'focalfp-self-test',
    fu_hash,
    sources: [
      'fu-self-test.c',
      'fu-focalfp-firmware.c',
    ],
    include_directories: [
      root_incdir,
      fwupd_incdir,
      fwupdplugin_incdir,
    ],
    dependencies: [
      plugin_deps,
    ],
    link_with: [
      fwupd,
      fwupdplugin,
    ],
    install: true,
    install_dir: installed_test_bindir,
  )
  test('focalfp-self-test', e, env: env)
endif
