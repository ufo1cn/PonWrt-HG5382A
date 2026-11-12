include $(TOPDIR)/rules.mk

PKG_NAME:=gensio
PKG_VERSION:=2.4.1
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/cminyard/gensio/releases/download/v$(PKG_VERSION)
PKG_HASH:=76b47476d4099a8c0d1d02b1f402615960690b490189d4642b18021a072c4f6e

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/gensio
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=General Stream I/O library (for ser2net 4.x)
  DEPENDS:=+libpthread +libopenssl
endef

define Package/gensio/description
  gensio library required by ser2net >=4.0
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/gensio* $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libgensio*.so* $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/gensio.pc $(1)/usr/lib/pkgconfig/
endef

define Package/gensio/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libgensio*.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,gensio))
