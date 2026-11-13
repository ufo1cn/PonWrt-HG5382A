# binaries/ — 预编译二进制归档

这里存放**已经编译好、可直接拷到光猫上运行**的二进制/软件包，用于在 CI 不方便产出、
或需要立刻在真机上验证时取用。源码与打包脚本仍在 `pon_userspace/` 里，两者保持同版本。

## 目录约定

```
binaries/
└── <包名>/
    ├── README.md                 说明、安装步骤、使用条件、已知限制
    ├── SHA256SUMS                校验值（下载后必查）
    ├── <包名>-<版本>-<架构>      可执行文件本体
    └── <包名>.yaml.example       配套配置样例（如需要）
```

命名统一为 `<包名>-<版本>-<架构>`，架构用 PonWrt 的工具链名（本仓库为 `aarch64-cortex-a53`）。
文件在 git 里不保留可执行位，拷到设备后**必须** `chmod +x`。

## 当前收录

| 包 | 版本 | 架构 | 大小 | 用途 |
|---|---|---|---|---|
| [go2rtc](go2rtc/) | v1.9.15 (`990f4de3`) | aarch64-cortex-a53 | 4.93 MB（upx 压缩） | 摄像头/麦克风的 WebRTC/RTSP/HTTP 流媒体服务，配合 camlive 做远程视频聊天 |

## 通用校验方式

```sh
# 在光猫上（或先在本机校验再从本机 scp）
sha256sum go2rtc-1.9.15-aarch64-cortex-a53
# 与 binaries/<包名>/SHA256SUMS 里的值逐位比对
```

## 通用注意事项

- 这些二进制是**为 musl 系统编译的纯静态程序**（`CGO_ENABLED=0`），不链接 libc，
  因此在 musl（OpenWrt/PonWrt）和 glibc 系统上都能直接运行。
- 目标机型为 `airoha/an7581`（Cortex-A53，ARMv8.0）。基线是 ARMv8.0，不含 v8.1 的
  CRC32/加密扩展，A53 上不会出现非法指令。
- 拷进 `/usr/bin` 前先确认目标分区可写：squashfs 只读根需要 `mount -o remount,rw /`
  或者放到 `/overlay/upper/usr/bin/` 下（重启后仍在）。
