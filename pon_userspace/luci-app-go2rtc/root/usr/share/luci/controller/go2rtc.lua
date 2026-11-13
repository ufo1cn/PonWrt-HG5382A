-- luci-app-go2rtc 控制器
module("luci.controller.go2rtc", package.seeall)

local fs = require "nixio.fs"

function index()
	if not fs.access("/etc/config/go2rtc") then
		return
	end

	entry({"admin", "services", "go2rtc"}, firstchild(), translate("go2rtc"), 65).dependent = false
	entry({"admin", "services", "go2rtc", "config"}, cbi("go2rtc"), translate("设置"), 10)
	entry({"admin", "services", "go2rtc", "panel"}, template("go2rtc/panel"), translate("面板"), 20)
	entry({"admin", "services", "go2rtc", "status"}, call("action_status")).leaf = true
end

-- 给面板页取一次运行状态，避免页面静态渲染时 I/O 阻塞
function action_status()
	local uci = require "luci.model.uci".cursor()
	local enabled = uci:get("go2rtc", "main", "enabled") or "1"
	local api_port = uci:get("go2rtc", "main", "api_port") or "1984"

	local running = false
	local l = io.open("/var/etc/go2rtc.yaml", "r")
	if l then
		l:close()
	end
	-- 用 pidfile 判断：procd 拉起的实例会写 /var/run/go2rtc.pid 吗？
	-- go2rtc 自身不写 pidfile，这里用 ps 精确匹配命令行
	local ps = luci.sys.exec("ps w | grep -c '[g]o2rtc -c /var/etc/go2rtc.yaml'") or "0"
	running = (tonumber(luci.util.trim(ps)) or 0) > 0

	luci.http.prepare_content("application/json")
	luci.http.write_json({
		enabled = enabled,
		running = running,
		api_port = api_port,
	})
end
