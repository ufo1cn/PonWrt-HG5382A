# go2rtc v1.9.15 · aarch64（PonWrt / HG5585F-CT 用）

`go2rtc` 是一个纯 Go 的流媒体服务：把摄像头、麦克风、RTSP 源转成 **WebRTC / RTSP / MJPEG /
HLS / MP4**，自带一个完整的网页面板。它的用途是在浏览器里看光猫上的 USB 摄像头画面，
并做**双向语音**（浏览器麦克风 → 光猫喇叭，光猫麦克风 → 浏览器）。

本目录是**从源码编译**的产物，用于在真机上验证和救急；常规安装请优先用 CI 产出的
`go2rtc` apk 包（`pon_userspace/go2rtc/`）。

## 文件清单

| 文件 | 说明 |
|---|---|
| `go2rtc-1.9.15-aarch64-cortex-a53` | 可执行文件，UPX 压缩，4,931,572 B |
| `SHA256SUMS` | 校验值 |
| `go2rtc.yaml.example` | 配置样例（配 camlive + USB 麦克风/喇叭） |

## 构建来源（可复现）

| 项 | 值 |
|---|---|
| 上游仓库 | `https://github.com/AlexxIT/go2rtc` |
| 版本 | tag `v1.9.15`，commit `990f4de3cc21d1cca917d02d0bc40f2fabe75a49`（2026-11-05） |
| Go | `go1.26.0`（`go.mod` 要求 go 1.26.0，本机 go1.23 自动拉取工具链） |
| 编译参数 | `CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -trimpath -ldflags "-s -w"` |
| 未压缩大小 | 19,202,210 B |
| 压缩 | `upx --best --lzma`（upx 5.3.0）→ 4,931,572 B（25.68%），`upx -t` 通过 |
| sha256 | `1ec71c81b8f76c8200b9cd54af4e628d3ec240fc36c8c402635b4dd21c248430` |

复现命令：

```sh
git clone https://github.com/AlexxIT/go2rtc && cd go2rtc
git checkout v1.9.15
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -trimpath -ldflags "-s -w" -o go2rtc_linux_arm64 .
upx --best --lzma go2rtc_linux_arm64
```

上游自己也发布同版本 arm64 静态件（`go2rtc_linux_arm64`，5,116,108 B，
sha256 `af416f22b21a24ab83b8d78642190b4932ae695b4e386853f11d1b0319781763`，
也是 UPX 压过的）。本目录这份是自己编的，体积略小 185 KB；**apk 包用的是上游官方件**
（便于 `PKG_HASH` 锁定、CI 不必装 Go 1.26）。

## 使用条件（先确认这些，再动手）

1. **架构**：aarch64（ARMv8.0）。`HG5585F-CT` 与 `HG5382A` 都是 `airoha/an7581`
   （工具链 `aarch64_cortex-a53_gcc-14.4.0_musl`），两者通用。
2. **不依赖 libc**：`CGO_ENABLED=0` 全静态，`readelf` 无 `INTERP` 段、`NEEDED` 为 0，
   所以 musl / glibc 都能跑。已用 `qemu-aarch64` 实跑验证：
   `go2rtc version 1.9.15+dev.990f4de (990f4de) linux/arm64`，且 RTSP/API/WebRTC 全部
   `listen` 成功。
3. **视频源**：需要一个能出 MJPEG 的源。本仓库的 `camlive`（V4L2 UVC，HTTP MJPEG）是最省
   资源的选择；直接用 RTSP 摄像头源也可以。
4. **音频**（要做双向语音才需要）：内核必须有 `CONFIG_SND`、`CONFIG_SND_PCM`、
   `CONFIG_SND_USB_AUDIO`。go2rtc 的 ALSA 支持是纯 Go 实现，**直接 ioctl `/dev/snd`，
   不需要 libasound**；`/dev/snd/` 下看不到设备就是内核模块没装（对应 apk 是
   `kmod-sound-core` + `kmod-usb-audio`，另需 `audio-support` 才能让它们可选）。
5. **转码**（浏览器看 WebRTC 必需）：WebRTC 不支持 MJPEG，要 `camera_h264` 这类
   `ffmpeg:...#video=h264` 源，系统里得有 **ffmpeg ≥ 5.0**（OpenWrt 现为 6.x，可用；
   ffmpeg 4.4 会因为版本检查直接拒绝，实测日志 `unsupported version: 4.4.2`）。
   只做 RTSP/MJPEG 观看可以不装 ffmpeg、不开转码。
6. **端口**：API/面板 `1984`、RTSP `8554`、WebRTC `8555`（UDP+TCP）。
   `camlive` 与它同装时必须让开端口：本仓库把 camlive 的默认 RTSP 端口设为 **8556**，
   于是它的 HTTP MJPEG 端口自动为 **8557**（camlive 的规则是 HTTP = RTSP + 1）。
   所以 go2rtc 里摄像头源写 `http://127.0.0.1:8557/stream`。
   如果你沿用 camlive 的旧默认 `-p 8554`，会与 go2rtc 的 RTSP 抢端口（并且它的 HTTP 8555
   会与 WebRTC 的 TCP 8555 冲突），此时只留一个服务、或手工改其中一方。
7. **空间**：`/usr/bin` 下占 4.7 MB。flash 紧张时优先装 apk（同样大小），或放 `/tmp`
   临时验证（重启即失效）。
8. **权限**：需要以 root 跑（要开 1024 以下端口不一定，但要读 `/dev/video*`、`/dev/snd/*`
   和写 `/var/etc`）。

## 安装到光猫

### 方式 A：手工拷（最快，不经过 opkg/apk）

```sh
# 1) 在光猫上取文件（仓库是 public，直接 wget raw）
cd /tmp
wget -O go2rtc \
  https://raw.githubusercontent.com/ufo1cn/PonWrt-HG5382A/main/binaries/go2rtc/go2rtc-1.9.15-aarch64-cortex-a53

# 2) 必查校验值（应与 SHA256SUMS 一致）
sha256sum go2rtc
# 1ec71c81b8f76c8200b9cd54af4e628d3ec240fc36c8c402635b4dd21c248430

# 3) 装到可写位置（squashfs 只读根先 remount rw）
mount -o remount,rw /
chmod +x /tmp/go2rtc
cp /tmp/go2rtc /usr/bin/go2rtc

# 4) 放配置
mkdir -p /etc/go2rtc
cp /tmp/go2rtc.yaml /etc/go2rtc/go2rtc.yaml   # 参考本目录的 go2rtc.yaml.example

# 5) 前台试跑（先看日志，确认端口与源都对）
/usr/bin/go2rtc -c /etc/go2rtc/go2rtc.yaml

# 6) 浏览器打开 http://<光猫IP>:1984 看面板
```

长期运行建议用 procd 管（开机自启 + 崩溃重启），init 脚本在
`pon_userspace/go2rtc/files/go2rtc.init`，`/etc/init.d/go2rtc start` 即用。

### 方式 B：装 apk（推荐，可升级/卸载）

CI 的「独立包编译」步骤会产出 `go2rtc-1.9.15-r1.apk`，在构建产物 artifact 里下载后：

```sh
apk add --allow-untrusted go2rtc-1.9.15-r1.apk
/etc/init.d/go2rtc enable && /etc/init.d/go2rtc start
```

apk 版会装 `/etc/config/go2rtc`（UCI），启动时生成 `/var/etc/go2rtc.yaml`；配
`luci-app-go2rtc` 后可以在 LuCI 里改端口/源/麦克风喇叭，并把自带面板内嵌进页面。

## 验证是否真的能用

```sh
# 进程与端口
ps w | grep [g]o2rtc
netstat -lntup | grep -E '1984|8554|8555'

# API 版本（正常应返回 JSON，含 "version":"1.9.15..."）
wget -qO- http://127.0.0.1:1984/api

# 抓一张当前画面（最直接的判断）
wget -O /tmp/frame.jpg "http://127.0.0.1:1984/api/frame.jpeg?src=camera"
ls -la /tmp/frame.jpg     # 大于 0 且能用看图工具打开 = 链路通了

# ALSA 设备列表（双向语音前先确认麦克风/喇叭在不在）
wget -qO- http://127.0.0.1:1984/api/alsa
```

## 已知限制（重要，别踩）

1. **camlive 的 RTSP 输出对 ffmpeg 不兼容**：camlive 是把整帧 JPEG 直接塞进 RTP（省掉
   RFC2435 的熵编码重排 + 由 Q 值重建量化表），ffmpeg 的严格解包器会报
   `Received packet without a start chunk` / `Too short RTP/JPEG packet`。
   远程观看请走 **HTTP MJPEG**（`http://光猫IP:8556/stream`，浏览器和 go2rtc 都验证过能吃）
   或 **WebRTC**（经 go2rtc + ffmpeg 转 H264），不要指望 ffmpeg/VLC 直连 camlive 的 RTSP。
2. **WebRTC 要能连到 UDP 8555**：浏览器在外网时需要光猫做端口映射（或者走 Tailscale/
   WireGuard 之类组网）。只在内网用则无需映射。
3. **转码吃 CPU**：AN7581 上 MJPEG(640×480@15fps)→H264 是可行的，但同时多路会明显占
   CPU；先按单路调好再增加。
4. **UPX 压缩件**运行时会在内存里自解压（约占 19 MB 内存峰值，稳态回落到 Go 运行时水平），
   目标机不需要写权限。若遇到极少数内核 `noexec` 挂载点导致无法执行，可解压后用原版：
   `upx -d go2rtc`（在有 upx 的机器上做，解压得 19 MB 静态件）。
5. **不要把它直接暴露到公网 1984 端口**：面板没有登录认证。

## 卸载

```sh
/etc/init.d/go2rtc stop
rm -f /usr/bin/go2rtc /etc/go2rtc/go2rtc.yaml
# apk 安装的：apk del go2rtc
```
