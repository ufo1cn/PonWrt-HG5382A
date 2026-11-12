module("luci.controller.btsp", package.seeall)
function index()
    entry({"admin", "services", "btsp"}, cbi("btsp"), _("BTSP蓝牙串口透传"), 71)
        .dependent = true
        .icon = "bluetooth"
end
