#
# tempinfo —— 为 LuCI 概览「温度」行提供取数脚本
#
# 背景：luci-base 的 rpcd 实现 getTempInfo() 里是
#   if (!access('/sbin/tempinfo')) return {};
#   popen('/sbin/tempinfo')
# 即：前端 10_system.js 只有在拿到 tempinfo.tempinfo 非空时才插入「温度」行。
# 内核侧温度数据本身是有的（airoha,en7581-thermal -> thermal_zone0/cpu-thermal），
# 缺的只是这个脚本，所以本包只做一件事：把 /sbin/tempinfo 装进 rootfs。
#
# 本文件用于 OpenWrt 构建树（apk 打包），由 _inject_tempinfo.py 注入后
# `make package/tempinfo/compile` 即可产出 tempinfo-<ver>-r<rel>.apk。
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tempinfo
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=ufo1cn

include $(INCLUDE_DIR)/package.mk

define Package/tempinfo
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Temperature reporter for LuCI status overview
  URL:=https://github.com/ufo1cn/PonWrt-HG5382A
endef

define Package/tempinfo/description
  Installs /sbin/tempinfo, which prints a single line such as
  "SoC 48.3 °C" (or "SoC 48.3 °C / SFP 45.0 °C" when an SFP with DDM is present).
  luci-base's getTempInfo() popen()s this script; when the file is absent,
  LuCI hides the Temperature row in Status -> Overview.
endef

# 纯脚本包：没有上游源码，不下载、不编译。
define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/tempinfo/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/tempinfo $(1)/sbin/tempinfo
endef

$(eval $(call BuildPackage,tempinfo))
