# SPDX-License-Identifier: BSD-3-Clause

project('tqftpserv',
        'c',
        version: '1.1',
        default_options: [
            'warning_level=1',
            'buildtype=release',
        ])

prefix = get_option('prefix')

zstd_dep = dependency('libzstd')

# Not required to build the executable, only to install unit file
systemd = dependency('systemd', required : false)
if systemd.found()
        systemd_system_unit_dir = get_option('systemd-unit-prefix')
        if systemd_system_unit_dir == ''
                systemd_system_unit_dir = systemd.get_variable(
                        pkgconfig : 'systemdsystemunitdir',
                        pkgconfig_define: ['prefix', prefix])
        endif
endif

qrtr_dep = dependency('qrtr')

tqftpserv_srcs = ['translate.c',
                  'tqftpserv.c',
                  'zstd-decompress.c']
executable('tqftpserv',
           tqftpserv_srcs,
           dependencies : [qrtr_dep, zstd_dep],
           install : true)

if systemd.found()
        systemd_unit_conf = configuration_data()
        systemd_unit_conf.set('prefix', prefix)
        configure_file(
                input : 'tqftpserv.service.in',
                output : 'tqftpserv.service',
                configuration : systemd_unit_conf,
                install_dir : systemd_system_unit_dir)
endif
