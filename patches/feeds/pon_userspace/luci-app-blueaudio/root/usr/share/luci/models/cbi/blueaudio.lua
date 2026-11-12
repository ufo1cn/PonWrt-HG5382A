local m = Map("blueaudio", translate("BlueALSA 蓝牙音频设置"),
    translate("切换A2DP立体声 / HFP免提（HFP支持耳机麦克风，音质降为单声道）"))
local s = m:section(TypedSection, "main", "")
s.anonymous = true

local p = s:option(ListValue, "profile", translate("音频模式"))
p:value("a2dp", translate("A2DP 立体声（听歌，无麦克风）"))
p:value("hfp", translate("HFP AG 免提（支持耳机麦，单声道）"))
p.description = translate("切换后自动重启BlueALSA；蓝牙耳机A2DP与HFP互斥，不能同时立体声+麦克风")

return m
