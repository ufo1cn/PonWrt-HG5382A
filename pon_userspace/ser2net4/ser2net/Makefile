include $(TOPDIR)/rules.mk

PKG_NAME:=ser2net
PKG_VERSION:=4.6.0
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/cminyard/ser2net/releases/download/v$(PKG_VERSION)
PKG_HASH:=908675c4eb76c6e9b9187f051a0d6b06e46604b0f926b4e3d065e8a079445454

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/ser2net
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Serial to TCP proxy (ser2net 4.x, yaml config)
  DEPENDS:=+gensio +libpthread
endef

define Package/ser2net/description
  ser2net 4.x, uses yaml configuration, supports telnet/tcp raw
endef

define Package/ser2net/conffiles
/etc/ser2net.yaml
/etc/config/ser2net
endef

define Package/ser2net/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) $(PKG_INSTALL_DIR)/usr/bin/ser2net $(1)/usr/bin/
	$(CP) $(PKG_INSTALL_DIR)/usr/bin/ser2netctl $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ser2net.init $(1)/etc/init.d/ser2net
	$(INSTALL_DIR) $(1)/etc
	$(CP) ./files/ser2net.yaml.template $(1)/etc/ser2net.yaml.template
endef

$(eval $(call BuildPackage,ser2net))
