#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# 把 patches/feeds/ 下的補丁打到 PONWRT_DIR/feeds/ 的 feed 檢出上。
#
# 目錄約定（相對倉庫根目錄）：
#   patches/feeds/<feed 名>/<任意名>.patch
#   例如：patches/feeds/pon_drivers/0001-gpon-full.patch
#         patches/feeds/pon_userspace/0001-gpon-luci.patch
#
# 補丁內部目標路徑相對 feed 倉庫根（a/ 與 b/ 前綴，即 -p1），
# 所以用 `git -C <feed> apply` 應用；<feed 名> 這一層子目錄只是歸檔用。
#
# PONWRT_DIR：PonWrt 源碼目錄，相對倉庫根目錄（預設 "."；本倉庫為 "ponwrt"）。
#
# 冪等：已打過的補丁會被識別並跳過，可在 feed 已更新的檢出上重複執行。
# 請在 `./scripts/feeds update -a` 與 `./scripts/feeds install -a` 之後再運行。
# 設 FEED_PATCHES_STRICT=1 可把「feed 不存在」變成硬失敗（CI 中使用）。

set -Eeuo pipefail

# 從腳本自身位置回溯倉庫根目錄
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)/.."
cd "$REPO_ROOT" || exit 1

PONWRT_DIR="${PONWRT_DIR:-.}"

if [[ ! -d patches/feeds ]]; then
	echo "apply-feed-patches: no patches/feeds directory, nothing to do."
	exit 0
fi

applied=0
skipped=0
absent=0

while IFS= read -r -d '' patch_file; do
	rel="${patch_file#patches/feeds/}"
	feed_name="${rel%%/*}"
	feed_dir="${PONWRT_DIR}/feeds/${feed_name}"

	if [[ ! -d "$feed_dir" ]]; then
		echo "apply-feed-patches: feed '${feed_name}' not present, skipping ${patch_file}" >&2
		echo "  -> run './scripts/feeds update -a && ./scripts/feeds install -a' first" >&2
		if [[ "${FEED_PATCHES_STRICT:-0}" == "1" ]]; then
			exit 1
		fi
		absent=$((absent + 1))
		continue
	fi

	# 補丁從 stdin 送入：`git -C <feed> apply <路徑>` 會把路徑參數
	# 解析成相對 feed 目錄的路徑，用 stdin 重定向才可靠。
	if git -C "$feed_dir" apply --check < "$patch_file" 2>/dev/null; then
		echo "apply-feed-patches: applying ${patch_file}"
		git -C "$feed_dir" apply < "$patch_file"
		applied=$((applied + 1))
	elif git -C "$feed_dir" apply --reverse --check < "$patch_file" 2>/dev/null; then
		echo "apply-feed-patches: already applied, skipping ${patch_file}"
		skipped=$((skipped + 1))
	else
		echo "apply-feed-patches: ERROR ${patch_file} does not apply cleanly to ${feed_dir}" >&2
		echo "  -> the feed has moved on; rebase the patch before building" >&2
		exit 1
	fi
done < <(find patches/feeds -type f -name '*.patch' -print0 | sort -z)

echo "apply-feed-patches: ${applied} applied, ${skipped} already present, ${absent} skipped (feed absent)."
