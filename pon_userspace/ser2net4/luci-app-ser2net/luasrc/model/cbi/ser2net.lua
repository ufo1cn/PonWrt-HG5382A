local m = Map("ser2net", translate("Ser2net 4.x Serial Server"),
    translate("Serial to TCP forwarding, ser2net 4.x yaml backend"))

local s = m:section(TypedSection, "port", translate("Forward Rule"))
s.anonymous = true
s.addremove = true
s.template = "cbi/tblsection"

enable = s:option(Flag, "enable", translate("Enable"))
enable.rmempty = false

dev = s:option(Value, "device", translate("Serial Device"))
dev.default = "/dev/ttyS0"
dev.rmempty = false

baud = s:option(Value, "baud", translate("Baudrate"))
baud.default = "9600"
baud.rmempty = false

databits = s:option(ListValue, "databits", translate("Data Bits"))
databits:value("5","5")
databits:value("6","6")
databits:value("7","7")
databits:value("8","8")
databits.default = "8"

stopbits = s:option(ListValue, "stopbits", translate("Stop Bits"))
stopbits:value("1","1")
stopbits:value("2","2")
stopbits.default = "1"

parity = s:option(ListValue, "parity", translate("Parity"))
parity:value("none","None")
parity:value("odd","Odd")
parity:value("even","Even")
parity.default = "none"

tcp_mode = s:option(ListValue, "tcp_mode", translate("TCP Mode"))
tcp_mode:value("raw","Raw TCP")
tcp_mode:value("telnet","Telnet")
tcp_mode.default = "raw"

port = s:option(Value, "port", translate("Listen Port"))
port.default = "2000"
port.rmempty = false

timeout = s:option(Value, "timeout", translate("Idle Timeout(sec,0=off)"))
timeout.default = "0"

return m
