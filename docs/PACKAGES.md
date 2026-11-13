# HG5585F 固件软件包关系说明

本文档说明 `.github/workflows/build-hg5585f.yml` 实际编译的每个功能，以及它背后"功能包 → 依赖包 → 内核符号"的完整链路。

**为什么需要这份文档**：这套树里有一类失败不报错、只"消失"——包的依赖是 `@某全局开关` 或依赖一个默认关闭的 bool 时，kconfig 会把包和它的符号一起丢掉，`.config` 里连 `# is not set` 都不留，最后表现为"编译成功但包没有""菜单不显示""apk 不存在"。本文档把每条链路的真实依赖写出来，改配置时照表核对即可。

标注约定：

| 标记 | 含义 |
|---|---|
| 🅵 | 编进固件镜像（`CONFIG_PACKAGE_x=y`），刷机即有 |
| 🅐 | 只编成独立 apk（`=m` 单独出包 / `is not set` 后单编），用 `apk add` 装 |
| ⚙ | 内核符号（`CONFIG_KERNEL_*` 或 `CONFIG_*`），必须在 `.config` 里真的打开，否则包选中了也打不出 `.ko` |

依赖写法约定：`+pkg` = 硬依赖；`@SYMBOL` = 全局开关门槛，**不满足时整包不可选**；`+SYM:pkg` = 仅当 SYM 打开才依赖 pkg；`PROVIDES:=` = 虚拟包名，可被别的包当依赖满足。

---

## 0. 一句话总图

```
                         ┌──────────── HG5585F (AN7581 / aarch64 Cortex-A53 / musl) ────────────┐
 光猫本体 🅵   kmod-airoha-xpon → kmod-airoha-paged-bosa / kmod-airoha-pon-frontend → airoha-ponctl / airoha-pond
 连通性   🅵   firewall4 / dnsmasq / odhcpd / bridger / lldpd（PonWrt 自带）

 USB 根   🅵   kmod-usb3 ──→ kmod-usb-xhci-hcd + kmod-usb-xhci-mtk
                └→ kmod-usb-storage(+uas,+extras) → kmod-fs-{ext4,vfat,exfat,ntfs3} → block-mount
                └→ kmod-usb-serial(+ch341,+cp210x,+ftdi,+pl2303) / kmod-usb-acm   ← CH340 在这里
                └→ kmod-usb-net(+cdc-ether,+cdc-ncm,+rtl8152,+asix-ax88179) / kmod-usb-hid
                └→ kmod-bluetooth(+btusb,+btbcm,+btmtk,+hci-uart) → bluez 五件套 🅵
                └→ kmod-rtw88(+usb,+8822cu,+8821cu,+8723du) / kmod-rtl8xxxu / kmod-rtl8192cu 🅵 + 固件 🅵
                └→ audio-support + kmod-input-core → kmod-sound-core → kmod-usb-audio 🅵
                └→ kmod-video-core → kmod-video-videobuf2 → kmod-video-uvc 🅵
                                                          └→ camlive 🅐 → luci-app-camlive 🅐
                                                          └→ go2rtc 🅐（转 WebRTC/RTSP/HLS）
                                                                 └ ffmpeg 🅐 ← libx264 🅐 / libopus 🅐
                                                                 └ luci-app-go2rtc 🅐 ← luci-compat 🅐 ← luci-lua-runtime 🅐

 代理     🅵   sing-box ← luci-app-homeproxy(+firewall4,+kmod-nft-tproxy,+ucode-mod-digest)
                nikki ← mihomo-meta（PROVIDES:=mihomo）/ mihomo-alpha
                omcproxy ← libubox, libubus
 其它     🅵   kmod-iptv ← luci-app-iptv ／ luci-app-airoha-npu ／ luci-app-ddns ／ luci-app-ttyd
          🅐   usbutils ← libusb-1.0 + libudev + usbids ／ gensio → ser2net → luci-app-ser2net ／ alsa-utils
```

---

## 1. 光猫本体（PON）

| 功能 | 包 | 依赖 | 说明 |
|---|---|---|---|
| XG-PON 驱动 | `kmod-airoha-xpon` 🅵 | ⚙ `CONFIG_*`（PonWrt 自带 KCONFIG，无需手工加） | 出 `xpon*.ko`，唯一位于 `bin/targets/airoha/an7581/packages` |
| SFP/PON 前端 | `kmod-airoha-pon-frontend` 🅵 | 同上 | |
| paged BOSA | `kmod-airoha-paged-bosa` 🅵 | 同上 | 光模块访问通道 |
| 用户态工具 | `airoha-ponctl`、`airoha-pond` 🅵 | 上三者 | 配置/守护进程，缺了固件能启动但业务不通 |
| IPTV 内核钩子 | `kmod-iptv` 🅵 | iptv feed（`yveming/openwrt-iptv`，`kernel/iptv`，`PKG_NAME:=iptv`） | 包名与目录名不一致，写配置时用 `kmod-iptv` |
| IPTV 前端 | `luci-app-iptv` 🅐 | `+luci-base +kmod-iptv` | 独立包，因为它改过 Makefile 修 include 路径 |

> 机型锁定：`CONFIG_TARGET_airoha_an7581_DEVICE_fiberhome_hg5585f-ct=y`，`-ct-usb-sfp` 是唯一含 `&usb0` 的变体（USB 与 lan5 的 2.5G PCS 二选一）。本树 `CONFIG_TARGET_MULTI_PROFILE` 无效，一次只能一个机型。

---

## 2. USB 基础与存储 🅵

| 层 | 包 | 依赖 / 内核符号 |
|---|---|---|
| 主机控制器 | `kmod-usb3` | `+kmod-usb-xhci-hcd +kmod-usb-xhci-mtk`；⚙ `CONFIG_USB_XHCI_HCD`、`CONFIG_USB_XHCI_PLATFORM` |
| 直接依赖（预检会查） | `kmod-usb-xhci-hcd`、`kmod-usb-xhci-mtk` | 由 `kmod-usb3` 拉入，缺任一个 precheck 报错 |
| U 盘 | `kmod-usb-storage` | ⚙ `CONFIG_USB_STORAGE` |
| UAS / 杂项 | `kmod-usb-storage-uas`、`kmod-usb-storage-extras` | 依赖 `usb-storage` |
| 文件系统 | `kmod-fs-ext4` / `-vfat` / `-exfat` / `-ntfs3` | 各自 ⚙ `CONFIG_EXT4_FS` 等 |
| 挂载管理 | `block-mount` | 无 kmod 依赖，提供 `/etc/config/fstab` 与 hotplug 挂载 |

> ⚠️ `kmod-usb-xhci`（旧名）在这套树里**不存在**，写它 precheck 必红。
> ⚠️ `usb-modeswitch` / `usb-modeswitch-data` 在全部已配 feed 里**都不存在**，别再试。

---

## 3. USB 转串口 / CH340 🅵

| 功能 | 包 | 依赖 | 内核符号 |
|---|---|---|---|
| 串口框架 | `kmod-usb-serial` | ⚙ `CONFIG_USB_SERIAL` | 提供 `usbserial.ko`，是所有芯片驱动的公共父层 |
| **CH340/CH341**（烧录器、USB-TTL） | `kmod-usb-serial-ch341` | `+kmod-usb-serial` | ⚙ `CONFIG_USB_SERIAL_CH341` |
| CP210x | `kmod-usb-serial-cp210x` | 同上 | ⚙ `CONFIG_USB_SERIAL_CP210X` |
| FTDI | `kmod-usb-serial-ftdi` | 同上 | ⚙ `CONFIG_USB_SERIAL_FTDI_SIO`（符号名与包名不一致） |
| PL2303 | `kmod-usb-serial-pl2303` | 同上 | ⚙ `CONFIG_USB_SERIAL_PL2303` |
| CDC-ACM（部分 4G 模块/开发板） | `kmod-usb-acm` | ⚙ `CONFIG_USB_ACM` | |
| 工具 | `usbutils` 🅐 | `+libusb-1.0 +libudev +usbids` | 顺序必须 libusb-1.0 / libudev-zero / usbids → usbutils |

设备侧结果是 `/dev/ttyUSB0`（CH340）或 `/dev/ttyACM0`（ACM）。`kmod-usb-serial` 家族全部 🅵 进镜像。

---

## 4. USB 有线网卡 🅵

| 包 | 依赖 | 内核符号 | 对应芯片 |
|---|---|---|---|
| `kmod-usb-net` | ⚙ `CONFIG_USB_USBNET` | 公共层 `usbnet.ko` | — |
| `kmod-usb-net-cdc-ether` | `+kmod-usb-net` | `CONFIG_USB_NET_CDCETHER` | 通用 CDC 网卡 |
| `kmod-usb-net-cdc-ncm` | `+kmod-usb-net` | `CONFIG_USB_NET_CDC_NCM` | 部分 4G/5G 模块 |
| `kmod-usb-net-rtl8152` | `+kmod-usb-net` | `CONFIG_USB_RTL8152` | RTL8152/8153 千兆 |
| `kmod-usb-net-asix-ax88179` | `+kmod-usb-net` | `CONFIG_USB_NET_AX88179_178A` | 绿联/优越者 2.5G/千兆 |

> 历史真因：包选中了、但内核 `CONFIG_USB_USBNET` 没开 → 打包时报 `usbnet.ko is missing`。所以工作流里有 `DRIVER_KCONFIGS` 强制 `CONFIG_KERNEL_*`，并在编译后用内核 `.config` 反查。

---

## 5. USB 无线网卡（rtw88 系）🅵

| 层 | 包 | 依赖 | 内核符号 |
|---|---|---|---|
| 无线框架 | （由芯片包自动拉入）`kmod-cfg80211`、`kmod-mac80211` | ⚙ `CONFIG_CFG80211`、`CONFIG_MAC80211` | 没有这两个芯片包全是空壳 |
| rtw88 核心 | `kmod-rtw88` | HIDDEN（不能直接选，靠芯片包拉） | `CONFIG_RTW88` |
| USB 总线后端 | `kmod-rtw88-usb` | `+kmod-rtw88` | `CONFIG_RTW88_USB` | rtw88 芯片必须配它才能被 USB 识别 |
| RTL8822CU | `kmod-rtw88-8822cu` | `+kmod-rtw88-usb` | `CONFIG_RTW88_8822CU`（父 `CONFIG_RTW88_8822C`） | 双频 AC1200 常见 |
| RTL8821CU | `kmod-rtw88-8821cu` | 同上 | `CONFIG_RTW88_8821CU`（`RTW88_8821C`） | 双频 AC600 |
| RTL8723DU | `kmod-rtw88-8723du` | 同上 | `CONFIG_RTW88_8723DU`（`RTW88_8723D`） | 2.4G |
| RTL8xxxu | `kmod-rtl8xxxu` | ⚙ `CONFIG_RTL8XXXU` | 老款 8188/8192/8188eu 类 |
| RTL8192CU | `kmod-rtl8192cu` | ⚙ `CONFIG_RTL8192CU` | 另一套老驱动，与 rtl8xxxu 覆盖重叠 |
| 固件 🅵 | `rtl8822ce-firmware`、`rtl8821ce-firmware`、`rtl8723bu-firmware`、`rtl8723de-firmware` | 来自 `package/firmware/linux-firmware/realtek.mk` | **没有固件时 dmesg 会报 firmware not found，网卡不工作** |

> 名字全部核实自 `package/kernel/linux/modules/usb.mk`、`bluetooth.mk` 与 `package/kernel/mac80211/realtek.mk`。
> ⚠️ `kmod-rtl8723bu` 在这套树里**不存在**（`patches/feeds/pon_drivers/rtl8723bu/Makefile` 是死文件，没有对应内核源码）。8723BU 走 `kmod-rtw88-8723du`（D 系列）或 `rtl8xxxu`。

---

## 6. 蓝牙 🅵

### 6.1 内核层

| 包 | 依赖 | 内核符号 |
|---|---|---|
| `kmod-bluetooth` | ⚙ `CONFIG_BT` | 核心 `bluetooth.ko` |
| `kmod-btusb` | `+kmod-bluetooth` | ⚙ `CONFIG_BT_HCIBTUSB`（USB 蓝牙适配器必需） |
| `kmod-btbcm` | `+kmod-btusb` | Broadcom 固件加载（`CONFIG_BT_HCIBTUSB_BCM`） |
| `kmod-btmtk` | `+kmod-btusb` | MediaTek |
| `kmod-hci-uart` | `+kmod-bluetooth` | ⚙ `CONFIG_BT_HCIUART`（接串口蓝牙模块） |

布尔子选项（`CONFIG_BT_HCIBTUSB_RTL`、`CONFIG_BT_BREDR`、`CONFIG_BT_LE`）由 kmod 包自带的 KCONFIG 负责，**不要**用 `=m` 强写，否则会把 bool 塞成 module 导致编译报错。

### 6.2 用户态栈（feed: `immortalwrt/packages` → `utils/bluez`）

| 包 | 依赖 |
|---|---|
| `bluez-libs` | `+libpthread +USB_SUPPORT:kmod-bluetooth` |
| `bluez-utils` | `+bluez-libs` |
| `bluez-utils-btmon` | `+bluez-libs +glib2` |
| `bluez-utils-extra` | `+bluez-libs +libpthread +librt +glib2 +libncurses +libreadline +dbus` |
| `bluez-daemon` | `+bluez-libs +bluez-utils +bluez-utils-extra +glib2 +glib2 +libncurses +libreadline +dbus +libical` |

> ⚠️ **没有名为 `bluez` 的包**。写 `CONFIG_PACKAGE_bluez` 会让预检直接红——这正是历史上踩过的坑。本地 `luci-app-btsp` 原本 `+bluez`，工作流里用 sed 改成 `+bluez-daemon`。

### 6.3 蓝牙音频 / 串口转发（本仓库自建，🅐 未进镜像）

| 包 | 依赖 | 现状 |
|---|---|---|
| `luci-app-blueaudio` | `+bluez-alsa +bluez-daemon +dbus +kmod-bluetooth` | 工作流 sed 摘掉了 `+bluez-alsa`（feed 里没有该包），所以**音频后端能力实际缺失**，只能做配对/管理 |
| `luci-app-btsp` | `+ser2net +bluez` → sed 改 `+bluez-daemon` | 把蓝牙串口服务暴露出去 |

---

## 7. USB 音频（麦克风/喇叭）🅵

这条链路有一个**硬依赖陷阱**，历史上一度整段静默消失：

```
audio-support（bool 全局开关，默认 n）
        ↑ 硬依赖，没有 " +" 号
kmod-sound-core  ──→ ⚙ CONFIG_SND / CONFIG_SND_PCM
        ↑
kmod-usb-audio   ──→ ⚙ CONFIG_SND_USB_AUDIO   （USB 声卡/UVC 摄像头麦克风）
        ↑ 软依赖 +kmod-media-controller（modules/ 里没有 media.mk，不影响）
alsa-utils 🅐    ──→ +alsa-lib +alsa-ucm-conf +libncursesw +libpthread
```

| 包 | 关键点 |
|---|---|
| `audio-support` 🅵 | `kmod-sound-core` 的 `DEPENDS:=audio-support +kmod-input-core` 里 `audio-support` **没有 `+`**，是"必须已启用"的硬条件；默认 n 时 kconfig 把 `kmod-sound-core`/`kmod-usb-audio` 判为不可选，`.config` 里连 `# is not set` 都不留 |
| `kmod-input-core` 🅵 | 同上，硬依赖，必须显式 `=y` |
| `kmod-sound-core` 🅵 | ⚙ `CONFIG_SND`、`CONFIG_SND_PCM` |
| `kmod-usb-audio` 🅵 | ⚙ `CONFIG_SND_USB_AUDIO`，出 `snd-usb-audio.ko` |
| `alsa-utils` 🅐 | 提供 `arecord/aplay/amixer`，核对设备 `arecord -l` / `aplay -l` |

> `FEATURES:=dt squashfs nand ramdisk gpio`（`target/linux/airoha/Makefile`）里**没有 audio**，所以这套音频能力完全靠上面三个 kmod 手写进 `.config`，不能指望 target 默认带。

---

## 8. USB 摄像头 → 远程访问 / 视频聊天

这是本仓库自己造的一条链路，分成"后端采集"和"对外发布"两段。

### 8.1 采集（camlive，本仓库 894 行 C）

| 包 | 依赖 | 说明 |
|---|---|---|
| `kmod-video-core` 🅵 | ⚙ `CONFIG_MEDIA_SUPPORT`、`CONFIG_VIDEO_DEV` | V4L2 核心 |
| `kmod-video-videobuf2` 🅵 | `+kmod-video-core` | 现代 UVC 驱动的缓冲区框架 |
| `kmod-video-uvc` 🅵 | `+kmod-video-core +kmod-video-videobuf2 +kmod-input-core` | ⚙ `CONFIG_USB_VIDEO_CLASS`，出 `uvcvideo.ko` |
| `camlive` 🅐 | `+libpthread +kmod-video-uvc` | 零外部依赖：V4L2 强制 `V4L2_PIX_FMT_MJPEG` 直通，输出 HTTP MJPEG（`/stream`）；RTSP 为附加能力 |
| `luci-app-camlive` 🅐 | `+camlive +kmod-video-uvc` | Lua 写的前端 |

### 8.2 发布（go2rtc）

| 包 | 依赖 | 说明 |
|---|---|---|
| `go2rtc` 🅐 | `@aarch64`（另有 `PKGARCH:=aarch64_cortex-a53`） | 用上游 v1.9.15 官方静态件 + `PKG_HASH` 锁定；`CGO_ENABLED=0` 静态，musl/glibc 无关；UCI 驱动（`/etc/config/go2rtc` → 启动时生成 `/var/etc/go2rtc.yaml`） |
| `ffmpeg` 🅐 | `+libx264`（条件）/`FFMPEG_LIBOPUS`→`libopus` | **go2rtc 做 WebRTC 的唯一 H264 来源**；组件集必须完整 |
| `libx264` 🅐 | `+libpthread @BUILD_PATENTED` | ⚠️ 不开全局 `CONFIG_BUILD_PATENTED=y` 时 kconfig 不给符号，包直接消失 |
| `libopus` 🅐 | — | go2rtc 源码里音频是 `-c:a libopus`，没它就没有声音 |
| `luci-app-go2rtc` 🅐 | `+go2rtc` | 薄壳：启停/端口/摄像头源 + 内嵌 1984 自带面板 |
| `luci-lua-runtime` 🅐 | — | Lua 运行时（`luci-compat` 的硬依赖） |
| `luci-compat` 🅐 | `LUCI_DEPENDS:=+luci-lua-runtime` | ⚠️ **Lua 风格的 LuCI 应用没有它菜单不显示**（camlive / go2rtc 两个前端都是 Lua） |

### 8.3 端口与服务关系

| 服务 | 端口 | 用途 |
|---|---|---|
| go2rtc API/面板 | 1984 | 自带 Web UI、`/api/frame.jpeg` |
| go2rtc RTSP | 8554 | 给宽松客户端用（标准 ffmpeg 对 camlive 的 RTP/JPEG 封装不兼容） |
| go2rtc WebRTC | 8555（UDP+TCP） | 浏览器视频聊天主通道 |
| camlive RTSP | 8556 | `-p 8556`（原默认 8554 会与 go2rtc 撞） |
| camlive HTTP MJPEG | 8557 | `http://光猫:8557/stream`，go2rtc 的 `camera:` 指这里 |

数据流：`UVC → camlive(V4L2/MJPEG) → HTTP 8557 → go2rtc → WebRTC 8555 / MP4 / MJPEG / HLS`，音频走 `ALSA(麦克风) → go2rtc → ffmpeg+libopus`。

### 8.4 这条链路上已确认的 4 个 kconfig 陷阱

1. `libx264` 受 `@BUILD_PATENTED` 门控 → 不开全局开关则**静默消失**（`FFMPEG_LIBX264` 同理，它 `depends on FFMPEG_GPL && BUILD_PATENTED`）。
2. `CONFIG_FFMPEG_SELECT_COMPONENTS` **不能开**：该模式是 `--disable-everything` 后按清单 enable，而清单 `FFMPEG_DECODERS` 里**没有 mjpeg**、`FFMPEG_MUXERS` 里**没有 rtsp**，也没有对应 `CONFIG_FFMPEG_DECODER_mjpeg` / `FFMPEG_MUXER_rtsp` 符号。默认（不开）才是完整组件集，两者都有。
3. `audio-support` 硬依赖 → `kmod-usb-audio` 静默消失（见第 7 节）。
4. `luci-compat` 缺失 → Lua 前端菜单不显示，但**编译不报错**。

---

## 9. 代理与分流

| 功能 | 包 | 依赖（原文） | 来源 |
|---|---|---|---|
| sing-box 主程序 🅵 | `sing-box` | `+ca-bundle +kmod-inet-diag`，再加 `+kmod-netlink-diag +kmod-tun`，以及 `+SING_BOX_BUILD_NAIVE_OUTBOUND:libcronet` | `immortalwrt/packages` → `net/sing-box`（1.14.3） |
| naive outbound | `libcronet` 🅵 | `@!(arc\|\|armeb\|\|loongarch64\|\|mips\|\|...\|\|TARGET_gemini)` + `ATOMIC64_DEPENDS` | `immortalwrt/packages` → `libs/libcronet` |
| homeproxy 前端 🅵 | `luci-app-homeproxy` | `+sing-box +firewall4 +kmod-nft-tproxy +ucode-mod-digest` | homeproxy feed（`immortalwrt/homeproxy`，仓库本体是 LuCI 应用） |
| mihomo 内核 🅵 | `mihomo-meta` | `$(GO_ARCH_DEPENDS) +ca-bundle +ip-full +kmod-inet-diag +kmod-tun`，**`PROVIDES:=mihomo`** | nikki feed（`nikkinikki-org/OpenWrt-nikki`），1.19.34 |
| mihomo 备用内核 🅐/未选 | `mihomo-alpha` | 同上量级 | 同上（当前 `is not set`） |
| nikki 主程序 🅵 | `nikki` | `+ca-bundle +curl +yq firewall4 +ip-full +kmod-inet-diag +kmod-nft-socket +kmod-nft-tproxy +kmod-tun +kmod-dummy +mihomo` | nikki feed |
| nikki 前端 🅵 | `luci-app-nikki` | `+luci-base +nikki` | nikki feed |
| 组播代理 | `omcproxy` 🅵 | `+libubox +libubus` | **PonWrt 内树**：`package/network/services/omcproxy`（不在任何 feed 里） |
| 组播代理前端 🅵 | `luci-app-omcproxy` | `+luci-base +omcproxy` | `immortalwrt/luci` → `applications/` |

关键点：

- `nikki` 依赖的是虚拟名 **`mihomo`**，由 `mihomo-meta` 的 `PROVIDES:=mihomo` 满足——这就是为什么必须有 `mihomo-meta`，而不是随便放个 mihomo 二进制。
- **`CONFIG_SING_BOX_BUILD_NAIVE_OUTBOUND` 在 CI 里必须关**（naive outbound 依赖的 cronet 预编译件在这套工具链下编不过）。工作流把这条 `is not set` 写在**最后一次 `defconfig` 之后**，之后不再跑 defconfig，否则会被重开。
- `kmod-dummy` 只有 nikki 需要（做 fake 网卡做策略路由），别被 "dummy" 这个名字骗了。

---

## 10. 其它内置小件 🅵

| 包 | 作用 | 依赖 |
|---|---|---|
| `luci-app-ddns` + `ddns-scripts` | 动态域名 | `+ddns-scripts` |
| `luci-app-ttyd` + `ttyd` | 网页终端 | 无特殊 |
| `luci-app-airoha-npu` 🅐 | AN7581 NPU 状态/加速（工作流克隆到 `package/`） | 依赖 NPU 内核接口 |
| `luci-theme-argon` | 主题 | `+luci-base` |
| `curl` `libcurl` `libgnutls` `libnghttp2` `libpsl` `libidn2` `nettle` `libtasn1` `libunistring` `gmp` `libffi` | 证书/HTTP 栈 | 被 nikki/sing-box 等间接需要，工作流显式锁定以防被裁掉 |
| `openssh-sftp-server` | sftp 子系统 | `+openssh` |
| `kmod-tun` `kmod-nft-tproxy` `kmod-nft-socket` `kmod-inet-diag` `kmod-dummy` | 代理必备内核设施 | 见第 9 节依赖串 |

---

## 11. 独立包（不进镜像）清单与安装顺序

| 顺序 | 包 | 为什么必须在这个位置 |
|---|---|---|
| 1 | `libx264` | ffmpeg 的 H264 编码器来源 |
| 2 | `libopus` | ffmpeg 的 Opus 编码器来源 |
| 3 | `ffmpeg` | 依赖上两者，`DEPENDS` 会自动带上 |
| 4 | `alsa-utils` | `+alsa-lib +alsa-ucm-conf +libncursesw` |
| 5 | `luci-lua-runtime` | LuCI Lua 应用运行时 |
| 6 | `luci-compat` | `+luci-lua-runtime` |
| 7 | `camlive` | 采集后端 |
| 8 | `luci-app-camlive` | `+camlive` |
| 9 | `go2rtc` | 发布端 |
| 10 | `luci-app-go2rtc` | `+go2rtc` |
| 附 | `usbutils` | 顺序：`usbids` → `libudev-zero` → `libusb-1.0` → `usbutils` |
| 附 | `gensio` → `ser2net` → `luci-app-ser2net` | 串口网络化 |
| 附 | `luci-app-iptv`、`luci-app-airoha-npu`、`luci-app-blueaudio`、`luci-app-btsp` | 前端类，依赖各自后端 |
| 附 | `EXTRA_KMODS`（`kmod-usb-serial*`、`kmod-usb-net*`、`kmod-video-*`、`kmod-usb-hid`、`kmod-usbip-client`） | 以 `=m` 另外出一份 kmod apk，方便免刷机加驱动 |

**最小可用组合**：
- 只想远程看摄像头画面 → `camlive`（不需要 ffmpeg，走 HTTP MJPEG）
- 想在浏览器视频聊天（WebRTC）→ `camlive + ffmpeg + libx264 + libopus + go2rtc`
- 想要网页配置 → 再加 `luci-lua-runtime + luci-compat + luci-app-camlive + luci-app-go2rtc`

---

## 12. 改包时的工作流同步清单

新增/删除一个包，必须同步改 `.github/workflows/build-hg5585f.yml` 的**三处**，否则会出现"配置写了但没生效"或"预检误报"：

| 位置 | 变量 / 步骤 | 作用 |
|---|---|---|
| 作业级 env | `USB_CORE_PKGS` / `DRIVER_PKGS` / `DRIVER_FW_PKGS` / `BT_USER_PKGS` / `EXTRA_KMODS` | 这些变量会被第 18 步写入 `.config`、第 19 步逐个预检、第 23 步反查 `.ko`、第 25 步单独出 kmod 包，**一处改动全链路生效** |
| 作业级 env | `DRIVER_KCONFIGS`（强制 `CONFIG_KERNEL_<sym>=m`）/ `DRIVER_KCONFIG_CHECK`（编译后反查） | 解决"包选中但内核符号没开 → `.ko` 缺失" |
| 第 26 步 | `compile_dir "<pkg>" "<dir>"` 顺序 | 独立包按依赖顺序编译；`set_m` 函数负责把 `is not set` / `=y` 改成 `=m` |

命名易错清单（写错就必失败）：

| 错误写法 | 正确写法 | 原因 |
|---|---|---|
| `kmod-usb-xhci` | `kmod-usb-xhci-hcd` + `kmod-usb-xhci-mtk` | 旧名，树里没有 |
| `kmod-video-v4l2` | `kmod-video-core` | 包名不是 v4l2 |
| `CONFIG_PACKAGE_bluez` | `bluez-libs/-utils/-utils-btmon/-utils-extra/-daemon` | 没有 `bluez` 包 |
| `kmod-rtl8723bu` | `kmod-rtw88-8723du` 或 `kmod-rtl8xxxu` | 树里没有该驱动 |
| `kmod-usb-net` 单独写 | 同时写 `CONFIG_KERNEL_USB_USBNET=m` | 否则 `usbnet.ko is missing` |
| `kmod-sound-core` 单独写 | 同时写 `audio-support=y` + `kmod-input-core=y` | 硬依赖不满足会静默消失 |
| `libx264` 单独写 | 同时写 `CONFIG_BUILD_PATENTED=y` | `@BUILD_PATENTED` 门控 |
| `make -C <pkgdir> compile` | `make "<dir>/compile" V=s`（在构建根执行） | 前者缺 `$(TOPDIR)` 上下文，必然失败 |

---

## 13. 维护备忘

- 本文档的包名、依赖原文均取自各 Makefile 实际情况（`+x`/`@SYMBOL`/`PROVIDES` 逐条核对），内核符号取自 `package/kernel/linux/modules/*.mk` 与 `package/kernel/mac80211/realtek.mk`。
- 若上游 feed 更新导致依赖变化，先看 `logs/config-check.txt`（预检逐符号结果）与 `logs/extra-symbols.txt`（包是否被 kconfig 关闭以及原因），再看单包日志 `logs/<pkg>-pkg.log.txt`。
- `PonWrt1.yml`（HG5382A）逻辑上更老但已验证可用，作为"能编出 itb"的参照基线；本文件只描述 `build-hg5585f.yml`（HG5585F 电信版）。
