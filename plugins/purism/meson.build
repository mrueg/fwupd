cargs = ['-DG_LOG_DOMAIN="FuPluginPurism"']

shared_module('fu_plugin_purism',
  sources : [
    'fu-plugin-purism.c',
  ],
  include_directories : [
    include_directories('../..'),
    include_directories('../../src'),
    include_directories('../../libfwupd'),
  ],
  install : true,
  install_dir: plugin_dir,
  c_args : [
    cargs,
    '-DLOCALSTATEDIR="' + localstatedir + '"',
  ],
  dependencies : [
    plugin_deps,
    uuid,
  ],
)
