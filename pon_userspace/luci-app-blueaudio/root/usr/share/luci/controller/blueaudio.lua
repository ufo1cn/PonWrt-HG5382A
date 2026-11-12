module("luci.controller.blueaudio", package.seeall)
function index()
    entry({"admin", "services", "blueaudio"}, cbi("blueaudio"), _("Blue蓝牙音频"), 70)
        .dependent = true
        .icon = "bluetooth"
end
