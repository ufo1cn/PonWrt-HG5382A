# Realtek RTL8723BU USB WiFi out-of-tree driver
include $(TOPDIR)/rules.mk

PKG_NAME:=rtl8723bu
PKG_VERSION:=20240101
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/lwfinger/rtl8723bu.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2024-01-01
PKG_SOURCE_VERSION:=880501055220b700899800e18b80411880444444
PKG_MIRROR_HASH:=skip

PKG_MAINTAINER:=
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/rtl8723bu
  SUBMENU:=Wireless Drivers
  TITLE:=Realtek RTL8723BU USB WiFi driver
  DEPENDS:=+kmod-usb-core
  FILES:=$(PKG_BUILD_DIR)/rtl8723bu.ko
  AUTOLOAD:=$(call AutoProbe,rtl8723bu)
endef

define Build/Compile
	$(MAKE) -C $(LINUX_DIR) \
		ARCH=$(LINUX_KARCH) \
		CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) \
		M=$(PKG_BUILD_DIR) \
		CONFIG_RTL8723BU=m \
		modules
endef

$(eval $(call KernelPackage,rtl8723bu))
