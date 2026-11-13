#
# go2rtc - WebRTC/RTSP/HTTP 流媒体服务（HG5585F / aarch64_cortex-a53）
#
# 说明：go2rtc 的 go.mod 要求 go 1.26，OpenWrt feeds 里的 golang 版本不够，
# 在 CI 里从源码编译要额外拉一套 Go 工具链（+40 分钟且版本不匹配），
# 因此这里固定使用上游同版本（v1.9.15）的官方 aarch64 静态二进制，并用
# PKG_HASH 锁定内容。本地已用 go1.26 从该 tag 源码交叉编出同架构二进制自证。
#
include $(TOPDIR)/rules.mk

PKG_NAME:=go2rtc
PKG_VERSION:=1.9.15
PKG_RELEASE:=1

PKG_SOURCE:=go2rtc_linux_arm64
PKG_SOURCE_URL:=https://github.com/AlexxIT/go2rtc/releases/download/v$(PKG_VERSION)
PKG_HASH:=af416f22b21a24ab83b8d78642190b4932ae695b4e386853f11d1b0319781763
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKGARCH:=aarch64_cortex-a53
STRIP:=true

PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/go2rtc
  SECTION:=multimedia
  CATEGORY:=Multimedia
  TITLE:=go2rtc WebRTC/RTSP/HTTP streaming server
  URL:=https://github.com/AlexxIT/go2rtc
  DEPENDS:=@aarch64
  PROVIDES:=go2rtc
endef

define Package/go2rtc/description
 轻量流媒体服务：把摄像头/麦克风转成 WebRTC、RTSP、MJPEG、HLS、MP4。
 与本仓库的 camlive（HTTP MJPEG）或 UVC 摄像头配合，可在浏览器里双向语音视频通话。
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(DL_DIR)/$(PKG_SOURCE) $(PKG_BUILD_DIR)/go2rtc
	chmod +x $(PKG_BUILD_DIR)/go2rtc
endef

define Build/Compile
	# 预编译二进制，无需编译
endef

define Package/go2rtc/conffiles
/etc/go2rtc/go2rtc.yaml
endef

define Package/go2rtc/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/go2rtc $(1)/usr/bin/go2rtc
	$(INSTALL_DIR) $(1)/etc/go2rtc
	$(INSTALL_CONF) ./files/go2rtc.yaml $(1)/etc/go2rtc/go2rtc.yaml
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/go2rtc.init $(1)/etc/init.d/go2rtc
endef

$(eval $(call BuildPackage,go2rtc))
