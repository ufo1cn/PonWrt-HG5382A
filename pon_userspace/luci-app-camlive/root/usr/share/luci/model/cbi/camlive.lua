local m = Map("camlive", translate("CamLive USB摄像头RTSP推流"),
    translate("USB UVC摄像头RTSP实时推流服务"))
local s = m:section(TypedSection, "main", "")
s.anonymous = true

s:option(Flag, "enable", translate("启用"))
s:option(Value, "resolution", translate("分辨率"))
s:option(Value, "fps", translate("帧率"))
s:option(Value, "rtsp_port", translate("RTSP端口"))

return m
