project('mango', ['c', 'cpp'],
  version : '0.12.5',
)

subdir('protocols')

is_nixos = false
os_release = run_command('cat', '/etc/os-release', check: false)
if os_release.returncode() == 0
    if os_release.stdout().contains('ID=nixos')
        is_nixos = true
    endif
endif

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')

# 如果 sysconfdir 以 prefix 开头，去掉 prefix
if sysconfdir.startswith(prefix) and not is_nixos
  sysconfdir = sysconfdir.substring(prefix.length())
  # 确保 sysconfdir 是绝对路径
  if not sysconfdir.startswith('/')
    sysconfdir = '/' + sysconfdir
  endif
endif

# 打印调试信息，确认 sysconfdir 的值
# message('prefix: ' + prefix)
# message('sysconfdir: ' + sysconfdir)

cc = meson.get_compiler('c')
libm = cc.find_library('m')
xcb = dependency('xcb', required : get_option('xwayland'))
xlibs = dependency('xcb-icccm', required : get_option('xwayland'))
wayland_server_dep = dependency('wayland-server',version: '>=1.23.1')
wlroots_dep = dependency('wlroots-0.19',version: '>=0.19.0')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput',version: '>=1.27.1')
libwayland_client_dep = dependency('wayland-client')
pcre2_dep = dependency('libpcre2-8')
libscenefx_dep = dependency('scenefx-0.4',version: '>=0.4.1')


# 获取版本信息
git = find_program('git', required : false)
is_git_repo = false

# 检查当前目录是否是 Git 仓库
if git.found()
  git_status = run_command(git, 'rev-parse', '--is-inside-work-tree', check : false)
  if git_status.returncode() == 0 and git_status.stdout().strip() == 'true'
    is_git_repo = true
  endif
endif

if is_git_repo
  # 如果是 Git 目录，获取 Commit Hash 和最新的 tag
  commit_hash = run_command(git, 'rev-parse', '--short', 'HEAD', check : false).stdout().strip()
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
else
  # 如果不是 Git 目录，使用项目版本号和 "release" 字符串
  commit_hash = 'release'
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
endif

# 定义编译参数
c_args = [
  '-g',
  '-Wno-unused-function',
  '-DWLR_USE_UNSTABLE',
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(version_with_hash),
  '-DSYSCONFDIR="@0@"'.format('/etc'),
]

# 仅在 debug 选项启用时添加调试参数
if get_option('asan')
  c_args += [
    '-fsanitize=address',
    '-fno-omit-frame-pointer',
    '-fno-optimize-sibling-calls'
  ]
endif

if xcb.found() and xlibs.found()
  c_args += '-DXWAYLAND'
endif

# 链接参数（根据 debug 状态添加 ASAN）
link_args = []
if get_option('asan')
  link_args += '-fsanitize=address'
endif

executable('mango',
  'src/mango.c',
  'src/common/util.c',
  wayland_sources,
  dependencies : [
    libm,
    xcb,
    xlibs,
    libscenefx_dep,
    wayland_server_dep,
    wlroots_dep,
    xkbcommon_dep,
    libinput_dep,
    libwayland_client_dep,
    pcre2_dep,
  ],
  install : true,
  c_args : c_args,
  link_args : link_args,
)

# build mmsg
dwl_ipc_protocol = 'protocols/dwl-ipc-unstable-v2.xml'

wayland_scanner_client_header = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.h', 
  arguments: ['client-header', '@INPUT@', '@OUTPUT@']
)

wayland_scanner_private_code = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@']
)

# 在 mmsg 目标中使用生成器
executable('mmsg',
  'mmsg/mmsg.c',
  wayland_scanner_private_code.process(dwl_ipc_protocol),
  wayland_scanner_client_header.process(dwl_ipc_protocol),
  dependencies: [
    libwayland_client_dep
  ],
  install: true,
  c_args: [
    '-g',
    '-Wno-unused-function',
  ],
)

desktop_install_dir = join_paths(prefix, 'share/wayland-sessions')
portal_install_dir = join_paths(prefix, 'share/xdg-desktop-portal')
install_data('assets/mango.desktop', install_dir : desktop_install_dir)
install_data('assets/mango-portals.conf', install_dir : portal_install_dir)
install_data('assets/config.conf', install_dir : join_paths(sysconfdir, 'mango'))
