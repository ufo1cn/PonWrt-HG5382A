-- luci-app-go2rtc 设置页（UCI /etc/config/go2rtc，由 init 脚本生成 yaml）
local m, s, o

m = Map("go2rtc", translate("go2rtc"),
	translate("go2rtc 是一个流媒体服务：把摄像头/麦克风转成 WebRTC、RTSP、MJPEG、HLS。"
		.. "它自带一个完整的网页面板（默认 http://本机IP:1984），"
		.. "这里是把它接进固件必要的几个开关。"))

s = m:section(NamedSection, "main", "go2rtc", translate("基本设置"))
s.addremove = false
s.anonymous = true

o = s:option(Flag, "enabled", translate("启用"))
o.rmempty = false
o.default = "1"
o.description = translate("关闭后 /etc/init.d/go2rtc 不启动，面板也打不开")

o = s:option(Value, "api_port", translate("Web 面板 / API 端口"))
o.datatype = "port"
o.default = "1984"
o.description = translate("浏览器访问 http://本机IP:该端口 就是 go2rtc 自带面板")

o = s:option(Value, "rtsp_port", translate("RTSP 端口"))
o.datatype = "port"
o.default = "8554"

o = s:option(Value, "webrtc_port", translate("WebRTC 端口（UDP+TCP）"))
o.datatype = "port"
o.default = "8555"
o.description = translate("浏览器视频通话需要外部设备能访问到这个端口")

s2 = m:section(NamedSection, "main", "go2rtc", translate("视频源"))
s2.addremove = false
s2.anonymous = true

o = s2:option(Value, "camera", translate("摄像头流地址"))
o.default = "http://127.0.0.1:8556/stream"
o.description = translate("默认指向本机的 camlive（USB 摄像头）。"
	.. "http://127.0.0.1:8556/stream 里的 8556 = camlive 的 RTSP 端口 + 1。")

o = s2:option(Flag, "transcode", translate("转码成 H264（WebRTC 必需）"))
o.default = "0"
o.description = translate("WebRTC 不支持 MJPEG，浏览器要看画面必须转码；"
	.. "需要系统里装有 ffmpeg，且会占用 CPU。只做 RTSP/MJPEG 观看时可以不开。")

s3 = m:section(NamedSection, "main", "go2rtc", translate("音频（双向语音）"))
s3.addremove = false
s3.anonymous = true

o = s3:option(Value, "mic_device", translate("麦克风 ALSA 设备"))
o.placeholder = "default"
o.description = translate("USB 摄像头内置麦克风一般是 default 或 plughw:CARD=xxx。"
	.. "留空表示不采集音频。用 arecord -l 查设备名。")

o = s3:option(Value, "speaker_device", translate("喇叭 ALSA 设备"))
o.placeholder = "default"
o.description = translate("浏览器里的声音回放到光猫上的喇叭/音箱，留空表示不回放。")

return m
