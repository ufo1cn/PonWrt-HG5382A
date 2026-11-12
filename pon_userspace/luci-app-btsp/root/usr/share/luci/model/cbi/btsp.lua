local m = Map("btsp", translate("BTSP 蓝牙串口透传"),
    translate("将蓝牙SPP串口转为TCP网络串口服务器"))
local s = m:section(TypedSection, "main", "")
s.anonymous = true

s:option(Flag, "enable", translate("启用"))
s:option(Value, "mac", translate("蓝牙MAC地址"))
s:option(Value, "baud", translate("波特率"))
s:option(Value, "port", translate("TCP监听端口"))

return m
