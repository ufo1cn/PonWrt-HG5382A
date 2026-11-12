include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-blueaudio
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Self
PKG_LICENSE:=GPLv3

LUCI_TITLE:=BlueALSA 蓝牙音频切换 (A2DP/HFP)
LUCI_DEPENDS:=+bluez-alsa +bluez-daemon +dbus +kmod-bluetooth
LUCI_PKGARCH:=all

include $(TOPDIR)/feeds/luci/luci.mk
