project('valenz', 'cpp', version: '0.1.0', meson_version: '>=1.0.0', default_options: ['cpp_std=c++23', 'buildtype=release'])
qt6 = import('qt6')
qt = dependency('qt6', version: '>=6.9', modules: ['Core', 'DBus', 'Gui', 'Network', 'Qml', 'Quick', 'QuickControls2'])
kf6_core = dependency('KF6CoreAddons', version: '>=6.13')
cpp_compiler = meson.get_compiler('cpp')
kf6_config = declare_dependency(
    include_directories: include_directories(
        get_option('prefix') / 'include' / 'KF6' / 'KConfig',
        get_option('prefix') / 'include' / 'KF6' / 'KConfigCore',
    ),
    dependencies: [cpp_compiler.find_library('KF6ConfigCore')],
)
kf6_colorscheme = declare_dependency(
    include_directories: include_directories(get_option('prefix') / 'include' / 'KF6' / 'KColorScheme'),
    dependencies: [cpp_compiler.find_library('KF6ColorScheme')],
)
kf6_i18n = declare_dependency(
    include_directories: include_directories(get_option('prefix') / 'include' / 'KF6' / 'KI18n'),
    dependencies: [cpp_compiler.find_library('KF6I18n'), cpp_compiler.find_library('KF6I18nQml')],
)
kf6_bluez = dependency('KF6BluezQt', version: '>=6.13')
mauikit = dependency('MauiKit4', version: '>=4.0.5')
calendar = dependency('MauiKitCalendar4', version: '>=4.0.5')
system = dependency('MauiKitSystemControl', version: '>=4.0.5')
layer_shell = declare_dependency(
    include_directories: include_directories(get_option('prefix') / 'include'),
    dependencies: [cpp_compiler.find_library('LayerShellQtInterface')],
)
cpp_args = []
if host_machine.cpu_family() == 'x86_64'
    cpp_args += ['-march=x86-64-v3']
endif
if cpp_compiler.get_id() == 'gcc' and cpp_compiler.has_argument('-Wno-sfinae-incomplete')
    cpp_args += ['-Wno-sfinae-incomplete']
endif
qt_sources = qt6.preprocess(qresources: 'src/qml.qrc', moc_headers: ['src/controllers/valenzbridge.h', 'src/controllers/valenzbridge_notifications.h', 'src/controllers/valenzbridge_systray.h'], moc_sources: ['src/main.cpp'])
valenz_sources = ['src/main.cpp', 'src/controllers/valenzbridge.cpp', 'src/controllers/valenzbridge_properties.cpp', 'src/controllers/valenzbridge_controlcenter.cpp', 'src/controllers/valenzbridge_controlcenter_runtime.cpp', 'src/controllers/valenzbridge_workspace.cpp', 'src/controllers/valenzbridge_mpris.cpp', 'src/controllers/valenzbridge_artwork.cpp', 'src/controllers/valenzbridge_weather.cpp', 'src/controllers/valenzbridge_config.cpp', 'src/controllers/valenzbridge_systray.cpp', 'src/controllers/valenzbridge_notifications.cpp']
executable('valenz', valenz_sources + qt_sources, dependencies: [qt, kf6_core, kf6_config, kf6_colorscheme, kf6_i18n, kf6_bluez, mauikit, calendar, system, layer_shell], include_directories: include_directories(get_option('prefix') / 'include' / 'Maui' / 'Core' / 'System'), cpp_args: cpp_args, install: true)
install_data('org.maui.valenz.desktop.in', install_dir: get_option('datadir') / 'applications', rename: 'org.maui.valenz.desktop')
