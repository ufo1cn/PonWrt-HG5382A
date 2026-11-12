module("luci.controller.ser2net", package.seeall)

function index()
    entry({"admin", "services", "ser2net"}, cbi("ser2net"), _("Ser2net 4.x Serial Server"), 60).dependent = true
end
