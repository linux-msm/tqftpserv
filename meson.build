# SPDX-License-Identifier: BSD-3-Clause

project('tqftpserv',
        'c',
        version: '1.1.1',
        default_options: [
            'warning_level=1',
            'buildtype=release',
        ])

prefix = get_option('prefix')

tqftpserv_srcs = ['translate.c',
                  'tqftpserv.c']
tqftpserv_deps = []

zstd_dep = dependency('libzstd', required : false)
if zstd_dep.found()
  add_project_arguments('-DHAVE_ZSTD', language : 'c')
  tqftpserv_srcs += 'zstd-decompress.c'
  tqftpserv_deps += zstd_dep
endif

# Not required to build the executable,
# only to automatically retrieve the install dir for unit files
systemd = dependency('systemd', required : false)
if get_option('systemd-unit-prefix') != ''
  systemd_system_unit_dir = get_option('systemd-unit-prefix')
elif systemd.found()
  systemd_system_unit_dir = systemd.get_variable(
    pkgconfig : 'systemdsystemunitdir',
    pkgconfig_define: ['prefix', prefix])
else
  systemd_system_unit_dir = ''
endif

qrtr_dep = dependency('qrtr', required : false)
if not qrtr_dep.found()
  # Support old libqrtr that does not ship a qrtr.pc pkg-config file.
  cc = meson.get_compiler('c')
  qrtr_dep = cc.find_library('qrtr', has_headers : 'libqrtr.h')
endif
tqftpserv_deps += qrtr_dep

executable('tqftpserv',
           tqftpserv_srcs,
           dependencies : tqftpserv_deps,
           install : true)

if systemd_system_unit_dir != ''
        systemd_unit_conf = configuration_data()
        systemd_unit_conf.set('prefix', prefix)
        configure_file(
                input : 'tqftpserv.service.in',
                output : 'tqftpserv.service',
                configuration : systemd_unit_conf,
                install_dir : systemd_system_unit_dir)
endif
