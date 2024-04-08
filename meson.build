project('tqftpserv',
        'c',
        default_options: [
            'warning_level=1',
            'buildtype=release',
        ])

prefix = get_option('prefix')
systemd = dependency('systemd')
systemd_system_unit_dir = get_option('systemd-unit-prefix')
if systemd_system_unit_dir == ''
        systemd_system_unit_dir = systemd.get_variable(
                pkgconfig : 'systemdsystemunitdir',
                pkgconfig_define: ['prefix', prefix])
endif

qrtr_dep = dependency('qrtr')

tqftpserv_srcs = ['translate.c',
                  'tqftpserv.c']
executable('tqftpserv',
           tqftpserv_srcs,
           dependencies : qrtr_dep,
           install : true)

systemd_unit_conf = configuration_data()
systemd_unit_conf.set('prefix', prefix)
configure_file(
               input : 'tqftpserv.service.in',
               output : 'tqftpserv.service',
               configuration : systemd_unit_conf,
               install_dir : systemd_system_unit_dir)
