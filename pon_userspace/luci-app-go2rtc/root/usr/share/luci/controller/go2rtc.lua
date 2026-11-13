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

-- 面板页用它拿一次运行状态，避免模板渲染时阻塞
function action_status()
	local uci = require "luci.model.uci".cursor()
	local api_port = uci:get("go2rtc", "main", "api_port") or "1984"
	local enabled = uci:get("go2rtc", "main", "enabled") or "1"

	-- go2rtc 自己写 pidfile，这里按完整命令行精确匹配，避免误判
	local cnt = tonumber(luci.util.trim(
		luci.sys.exec("ps w | grep -c '[g]o2rtc -c /var/etc/go2rtc.yaml'"))) or 0

	luci.http.prepare_content("application/json")
	luci.http.write_json({
		enabled = enabled,
		running = (cnt > 0),
		api_port = api_port,
	})
end
