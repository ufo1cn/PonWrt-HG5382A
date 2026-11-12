module("luci.controller.camlive", package.seeall)
function index()
    entry({"admin", "services", "camlive"}, cbi("camlive"), _("CamLive摄像头推流"), 72)
        .dependent = true
        .icon = "camera"
end
