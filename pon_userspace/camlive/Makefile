#
# camlive - UVC 摄像头 RTSP(RTP/JPEG over TCP) + HTTP MJPEG 推流服务
#
# 这是 luci-app-camlive 的后端：前端只负责 /etc/config/camlive + init.d，
# 真正取流并推出去的是本程序 /usr/bin/camlive。
#
include $(TOPDIR)/rules.mk

PKG_NAME:=camlive
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-3.0
PKG_MAINTAINER:=ufo1cn

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/camlive
  SECTION:=utils
  CATEGORY:=Utilities
  SUBMENU:=Multimedia
  TITLE:=CamLive UVC 摄像头 RTSP/HTTP MJPEG 推流
  DEPENDS:=+libpthread +kmod-video-uvc
endef

define Package/camlive/description
  把 UVC 摄像头（MJPEG）直接推成 RTSP 和 HTTP MJPEG 流，供远程观看。
  兼容 luci-app-camlive：camlive -r 640x480 -f 15 -p 8554
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/camlive \
		$(PKG_BUILD_DIR)/camlive.c $(TARGET_LDFLAGS) -lpthread
endef

define Package/camlive/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/camlive $(1)/usr/bin/camlive
endef

$(eval $(call BuildPackage,camlive))
