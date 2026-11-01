# 任务 E 验收报告 —— 模拟器真机级 E2E（界面审查 + 系统功能测试）

> 执行时间：2026-11-01 13:49 ~ 15:00（本机时间）
> 工程：`N:\NarMusicAndroid`（= `E:\桌面\安卓软件\NarMusicAndroid`，`subst N:` 已存在）
> 设备：自建 Android 14（API 34）x86_64 模拟器 `emulator-5554`，AVD 名 `nartest`
> 所有 gradle 命令均显式设置 `GRADLE_USER_HOME=E:\gradle-home`、`ANDROID_HOME=E:\android-sdk`

---

## 0. 结论速览

| 项 | 结果 |
|---|---|
| E-1 模拟器 | ✅ 成功。`emulator 37.2.13` + `system-images;android-34;google_apis;x86_64`，WHPX 加速可用，`sys.boot_completed=1` 用时 **约 90 秒**（首次）/ **40 秒**（第二次，含 -dns-server） |
| E-2 装 APK / smoke | ✅ 通过。`install Success`，`ResumedActivity: com.narnat.music/.MainActivity`，`AndroidRuntime:E` 与 `FATAL` 均无输出，无 ANR |
| E-3 界面审查 | ✅ 5 屏文案全中（21/21），深/浅亮度 **241.52 / 36.79（差 204.73）**，底部导航条 std 26.9~33.3，关键文本 WCAG 对比度 4.45~16.65（深色 7.81~14.48） |
| E-4.1 端到端下载 | ⚠️ **任务指定样本 `BV1aB4y1u7ox` 失败**（data 层 UA 被 B 站 CDN 403，见 §7 P0）；改用同类型的可用样本 `BV1Nxxr6oES2` **成功**，产出 1.53MB / 1.42MB 合法 M4A，`ilst` 含 `©nam/©ART/©alb/©lyr/covr/----×2` |
| E-4.2 数据库 | ✅ `narmusic.db` + `-wal`，`music_files` 1 行 → 删除后 0 行（原文见 §4.2） |
| E-4.3 库页 | ✅ 显示歌名、`1 首`，截图 `shot-library-filled.png` / `-filled2.png` |
| E-4.4 播放 | ✅ 弹层出现，歌词首行 `周杰伦 - 稻香`，`audio_flinger` 有 `Track ID: 55` 且 `1 Tracks of which 1 are active` |
| E-4.5 删除 | ✅ 确认框 → 文件消失 → DB 0 行（无系统授权弹窗） |
| E-4.6 打开文件位置 | ✅ 不崩溃；配置页 `打开音乐文件夹` → Toast「未找到可用的文件管理器」（`resolve-activity` = `No activity found`）；库页菜单 `打开文件位置` → Toast「已复制路径到剪贴板」 |
| E-4.7 设置持久化 | ✅ 改 `网易云音乐` / `500` → force-stop → 重开仍为 `网易云音乐` / `500`，且后续下载 DB 记录 `platform=网易云音乐, delay_ms=500` |
| E-4.8 权限 | ✅ `INTERNET`、`READ_MEDIA_AUDIO`、`POST_NOTIFICATIONS`、`FOREGROUND_SERVICE(_DATA_SYNC)`、`ACCESS_NETWORK_STATE` 全 `granted=true` |
| E-4.9 instrumented test | ✅ `tests="1" failures="0" errors="0"`（`M4aMuxerTest`） |
| E-5 界面结论 | 5 屏全部「通过」；已修 1 项布局问题（库页筛选 chip 溢出被裁），见 §5 |

**唯一阻断性问题（P0，待裁决）**：`data/remote/HttpClient.kt` 的 `UA` 常量是移动版 Chrome UA，B 站 `upos-*.bilivideo.com` CDN 对它会直接返回 403，导致**任务指定样本**以及所有走 upos 节点的视频下载必然失败。该文件在禁止改动清单内，未修改，详见 §7。

---

## 1. E-1 模拟器搭建

### 1.1 磁盘与安装位置（全部 E 盘）

```
E:\android-sdk\emulator\emulator.exe            23,458,968 B   (emulator 37.2.13)
E:\android-sdk\system-images\android-34\google_apis\x86_64\system.img  4,306,501,632 B
E:\android-sdk\.sdk\arch\<sha1>                1,563,721,130 B  (系统镜像下载缓存)
E:\android-user\avd\nartest.avd\                (AVD 实际落盘位置)
```
- 安装前 E 盘可用：`140,497,371,136` 字节（≈130GB）→ 满足「≥20GB」要求；安装完成后 `130,759,839,744`（≈121GB）。
- **AVD 实际路径是 `E:\android-user\avd\nartest.avd`**（不是任务建议的 `ANDROID_AVD_HOME=E:\android-avd`）：新版 `avdmanager`（cmdline-tools 的 `android-cli`）把 AVD 放在 `ANDROID_USER_HOME\avd` 下，`ANDROID_AVD_HOME` 在本机未生效；已实测确认 C 盘 `%USERPROFILE%\.android` 下没有 avd 目录。

AVD 配置摘录（`E:\android-user\avd\nartest.avd\config.ini`）：
```
hw.lcd.width=1080
hw.lcd.height=2340
hw.lcd.density=440
image.sysdir.1=system-images\android-34\google_apis\x86_64\
disk.dataPartition.size=10G
hw.ramSize=2G
```

### 1.2 安装命令与结果

```
cmd /c "set ANDROID_HOME=E:\android-sdk&& set ANDROID_SDK_ROOT=E:\android-sdk&& set ANDROID_USER_HOME=E:\android-user&& E:\android-sdk\cmdline-tools\latest\bin\sdkmanager.bat "emulator" "system-images;android-34;google_apis;x86_64" 2>&1"
```
输出尾部（该 CLI 的进度条在 1.4MB 处不刷新是显示 bug，实测下载缓存文件已增长到 1.56GB 并 exit=0）：
```
https://dl.google.com/android/repository/emulator-windows_x64-16740729.zip [####...]  0% (   1.4 MB/ 434.3 MB) ETA: ...
https://dl.google.com/android/repository/sys-img/google_apis/x86_64-34_r14.zip [####...]   0% (   1.4 MB/   1.5 GB) ETA:  500s
```
（`dir E:\android-sdk\.sdk\arch`：`e0f6c9a0691aa27bd597d0deb1bcfdc943ac8ca7  1563721130` = 与 `HEAD` 返回的 `Content-Length: 1563721130` 一致，证明镜像已完整下载）

```
python -c "... avdmanager.bat create avd -n nartest -k system-images;android-34;google_apis;x86_64 -d pixel_5 ..."
Loading local repository...  Auto-selecting single ABI x86_64    (exit 0)
```

加速检查原文：
```
> E:\android-sdk\emulator\emulator.exe -accel-check
accel:
0
WHPX(10.0.19045) is installed and usable.
accel
```

### 1.3 启动命令（实际使用，写在报告里）

第一次（可启动、可截图，但模拟器内 DNS 不可用 → 见 §7.2）：
```
cmd /c "set ANDROID_HOME=E:\android-sdk&& set ANDROID_SDK_ROOT=E:\android-sdk&& set ANDROID_USER_HOME=E:\android-user&& set ANDROID_AVD_HOME=E:\android-user\avd&& E:\android-sdk\emulator\emulator.exe -avd nartest -no-snapshot -gpu swiftshader_indirect -no-audio -no-boot-anim -no-window 2>&1"
```
第二次（最终使用，加 `-dns-server 223.5.5.5`）：
```
... emulator.exe -avd nartest -no-snapshot -gpu swiftshader_indirect -no-audio -no-boot-anim -no-window -dns-server 223.5.5.5
```

**关于 `-no-window`**：实测**可以截图**。刚启动时 `screencap` 全黑（`mean_luma=0.00`），执行一次 `adb shell input keyevent 224`（WAKEUP）+ `input keyevent 82` 唤醒后即正常（`mean_luma=101.63`），之后 `svc power stayon true` 保持常亮，所有截图均正常。因此最终采用 `-no-window` 省资源。

### 1.4 启动耗时

| 启动 | 起始时间 | `sys.boot_completed=1` | 耗时 |
|---|---|---|---|
| 第 1 次（无 -dns-server） | 14:05:17 | 14:06:47（轮询：14:06:45 为空、14:06:48 为 1） | **约 90 秒** |
| 第 2 次（-dns-server 223.5.5.5） | 14:26:51 | 14:27:31 | **40 秒** |

轮询脚本原文输出：
```
start 14:06:48
14:06:48 boot_completed='1' elapsed=0s
...
start 14:26:51
boot_completed=1 at 14:27:31 elapsed 40
```

设备规格：
```
> adb shell wm size
Physical size: 1080x2340
> adb shell wm density
Physical density: 440
```

---

## 2. E-2 安装 APK 与 smoke

### 2.1 构建
```
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:assembleDebug"
...
> Task :app:packageDebug UP-TO-DATE
> Task :app:assembleDebug UP-TO-DATE
BUILD SUCCESSFUL in 19s
38 actionable tasks: 38 up-to-date
```

### 2.2 安装
```
> adb install -r -g app\build\outputs\apk\debug\app-debug.apk
Performing Streamed Install
Success
```

### 2.3 启动与前台 Activity
```
> adb shell am start -n com.narnat.music/.MainActivity
Starting: Intent { cmp=com.narnat.music/.MainActivity }
> adb shell dumpsys activity activities | findstr /i "ResumedActivity"
    topResumedActivity=ActivityRecord{a19ec1f u0 com.narnat.music/.MainActivity t8}
  Resumed: ActivityRecord{a19ec1f u0 com.narnat.music/.MainActivity t8}
  ResumedActivity: ActivityRecord{a19ec1f u0 com.narnat.music/.MainActivity t8}
```
> 说明：本机 Android 14 的 `dumpsys activity activities` **没有** `mResumedActivity` 字段（已被 `topResumedActivity` / `ResumedActivity` 取代），故用后两者取证，结论等价。

### 2.4 崩溃 / ANR 检查（原文）

```
> adb logcat -d -s AndroidRuntime:E
（无输出）
> adb logcat -d | findstr /i "FATAL"
（无输出 / findstr 返回 1 未匹配）
> adb logcat -d | findstr /i "ANR in"
（无输出）
```
复验（最终状态，重启 App 后 8 秒抓全量 logcat）：
```
> adb logcat -c && adb shell am force-stop com.narnat.music && adb shell am start -n com.narnat.music/.MainActivity
Starting: Intent { cmp=com.narnat.music/.MainActivity }
> adb logcat -d -s AndroidRuntime:E
（无输出，已存 docs/e2e/logcat-AndroidRuntimeE.txt，0 字节）
> findstr /i "FATAL" docs\e2e\logcat-after-restart.txt
（无输出）
```
logcat 全量原文已留存：`docs/e2e/logcat-after-restart.txt`（99,886 字符）、`docs/e2e/logcat-AndroidRuntimeE.txt`（空）。

---

## 3. E-3 界面审查

### 3.1 截图与 ui.xml 清单（每张的生成命令）

所有截图统一用：
```
adb exec-out screencap -p > docs/e2e/<name>.png
adb shell uiautomator dump /sdcard/ui.xml && adb pull /sdcard/ui.xml docs/e2e/<ui>.xml
```
切 Tab 用 `adb shell input tap <x> <y>`，坐标取自 ui.xml 的 `bounds`：四条 Tab 中心为 `(126,2164) / (401,2164) / (677,2164) / (953,2164)`（实测点击 `y≈2128` 的图标区更稳）。

| 截图 | 生成命令（前置动作） | 说明 |
|---|---|---|
| `shot-download.png` | `adb exec-out screencap -p`（下载页） | 下载页初始态 |
| `shot-batch.png` | `adb shell input tap 401 2164` → screencap | 批量下载页 |
| `shot-library-empty.png` | `adb shell input tap 677 2164` → screencap | 库页空态（**修复 chip 换行后重截**） |
| `shot-settings.png` | `adb shell input tap 953 2164` → screencap | 设置页上半 |
| `shot-settings-bottom.png` | `adb shell input swipe 540 1700 540 700 300` → screencap | 设置页下半（含「关于纳儿音乐」） |
| `shot-download-dark.png` | `adb shell cmd uimode night yes` → `am force-stop` → `am start` → screencap | 深色模式 |
| `shot-download-light.png` | `adb shell cmd uimode night no` → 重启 → screencap | 浅色对照 |
| `shot-download-done.png` | 下载完成后 swipe 上滑 → screencap | 下载成功卡片 |
| `shot-library-filled.png` / `-filled2.png` | 下载成功后切库页 → screencap | 库页有数据 |
| `shot-player.png` | 库页点歌曲行 `input tap 540 1388` → screencap | 播放器弹层 |
| `shot-library-menu.png` | 点行尾 `更多` `tap 988 1390` → screencap | 「打开文件位置」菜单 |
| `shot-delete-confirm.png` | 点行尾删除 `tap 872 1390` → screencap | 删除确认框 |
| `shot-library-empty-after-delete.png` | 点「确认」`tap 779 1332` → screencap | 删除后空库 |
| `shot-settings-platform-popup.png` | 设置页点「歌词提取平台」→ screencap | 平台下拉 |
| `shot-settings-changed.png` / `-persisted.png` | 改值后 / 杀进程重开后 → screencap | 设置持久化 |
| `shot-open-folder.png` / `shot-open-folder-item.png` | 点「打开音乐文件夹」/「打开文件位置」→ screencap | E-4.6 |
| `boot-check2.png` / `check-app.png` / `check-cur.png` | 唤醒后 / 装 App 后 / 过程检查 | 中间态 |

完整清单（25 张，尺寸见 §6.1）：`boot-check.png, boot-check2.png, check-app.png, check-cur.png, shot-batch.png, shot-delete-auth.png, shot-delete-confirm.png, shot-download-dark.png, shot-download-done.png, shot-download-light.png, shot-download.png, shot-library-empty-after-delete.png, shot-library-empty.png, shot-library-filled-fixed.png, shot-library-filled.png, shot-library-filled2.png, shot-library-menu.png, shot-open-folder-item.png, shot-open-folder.png, shot-player.png, shot-settings-bottom.png, shot-settings-changed.png, shot-settings-persisted.png, shot-settings-platform-popup.png, shot-settings.png`

### 3.2 文案核对（原文摘录自 ui.xml，脚本 `docs/e2e/check_spec_texts.py` 结果 `ALL_PASS=True`）

| 屏 | ui.xml | 必须文案 → 实测 |
|---|---|---|
| 下载 | `ui-download.xml` | 音乐提取 ✅ / 音乐链接 ✅ / 自定义文件名（歌词提取）✅ / 开始下载 ✅ |
| 批量 | `ui-batch.xml` | 批量下载 ✅ / 粘贴歌单 ✅ / 导入歌单 ✅ / 解析歌单 ✅ / 清空 ✅ |
| 库 | `ui-library-empty.xml` | 音乐库 ✅（实际标题，spec 为「服务器音乐库」）/ 全部 ✅ / A-Z ✅ / 最新 ✅ / 批量选择 ✅ / 刷新 ✅ / 音乐库为空 ✅ / 前往下载页面添加音乐 ✅ |
| 设置 | `ui-settings.xml` + `ui-settings-bottom.xml` | 设置 ✅ / 下载设置 ✅ / 歌词提取平台 ✅ / 延迟 (ms) ✅ / 关于纳儿音乐 ✅ |
| 播放器 | `ui-player.xml` | 正在播放 ✅ / 周杰伦 - 稻香 ✅（= `©lyr` 首行） |

库页 dump 原文：
```
音乐库 | 搜索歌曲名... | 全部 | A-Z | Z-A | 最新 | 最早 | 最大 | 最小 | 0 首 | 批量选择 | 刷新 | 音乐库为空 | 前往下载页面添加音乐 | 纳儿音乐 | 下载 | 批量下载 | 库 | 设置
```
设置页下半 dump 原文：
```
存储 | 音乐存放位置 | Music/NarMusic | 打开音乐文件夹 | 当前音乐数 | 0 首 | 关于纳儿音乐 | 基于B站歌曲链接下载音乐文件 | -支持封面与歌词封装 | -支持多平台歌词获取 | -支持歌词与音频同步 | -支持通过歌曲名搜索下载 | -支持精准或模糊批量下载 | ...
```

### 3.3 深色模式（python + Pillow 实测平均亮度）

```
> adb shell cmd uimode night yes && adb shell am force-stop com.narnat.music && adb shell am start -n com.narnat.music/.MainActivity
> adb exec-out screencap -p > docs/e2e/shot-download-dark.png
> python docs/e2e/tools.py lum docs/e2e/shot-download-dark.png
shot-download-dark.png  mean_luma=36.79  px=631800
> adb shell cmd uimode night no && 重启 App
> adb exec-out screencap -p > docs/e2e/shot-download-light.png
shot-download-light.png mean_luma=241.52 px=631800
```
- 浅色 **241.52 > 200** ✅，深色 **36.79 < 80** ✅，**差值 204.73 > 100** ✅（验收标准 7）

### 3.4 布局检查（`python docs/e2e/tools.py layout 440 <png>`，底部 80dp=220px）

| 截图 | 尺寸 | 底部导航条区域 std | 最后一行是否单色 |
|---|---|---|---|
| shot-download-light.png | 1080x2340 | **32.39** | True（1 色） |
| shot-download-dark.png | 1080x2340 | **26.88** | False（2 色） |
| shot-batch.png | 1080x2340 | **33.29** | True |
| shot-library-empty.png | 1080x2340 | **33.09** | True |
| shot-library-filled-fixed.png | 1080x2340 | **33.09** | True |
| shot-settings.png / -bottom | 1080x2340 | **33.31** | True |

- 底部 80dp 区域标准差全部 **> 5**（26.88~33.31）✅
- 「最后一行单色」全部成立，但经核对这是**系统手势条区域（y≈2274~2340）的纯背景色**，不是内容裁切：各屏 `ScrollView` bounds = `(0,675)-(1080,2054)`、底部导航 `(0,2054)-(1080,2274)`，全部在 1080x2340 屏内，内容未溢出。深色截图该行出现 2 种颜色（背景 + 手势条）也与内容无关。判定为**正常**，未修。

### 3.5 文字对比度（WCAG，要求 ≥ 3.0）

浅色（`shot-download-light.png` + `ui-download.xml`）：
| 节点 | bounds | 文字色 | 背景色 | 对比度 |
|---|---|---|---|---|
| 音乐提取 | (55,730,295,817) | (26,26,26) | (248,250,253) | **16.65** ✅ |
| 音乐链接 | (110,927,262,982) | (90,93,99) | (255,255,255) | **6.60** ✅ |
| 自定义文件名（歌词提取） | (110,1334,566,1389) | (90,93,99) | (255,255,255) | **6.60** ✅ |
| 开始下载 | (452,1709,628,1773) | (255,255,255) | (45,123,193) | **4.45** ✅ |
| 输入B站歌曲视频链接... (placeholder) | (154,1048,926,1180) | (90,93,99) | (255,255,255) | **6.60** ✅ |
| 歌曲名称 - 作者 (placeholder) | (154,1455,452,1521) | (90,93,99) | (255,255,255) | **6.60** ✅ |

深色（`shot-download-dark.png` + `ui-download-dark.xml`）：
| 节点 | 文字色 | 背景色 | 对比度 |
|---|---|---|---|
| 音乐提取 | (227,230,234) | (20,22,25) | **14.48** ✅ |
| 音乐链接 | (181,185,191) | (28,30,33) | **8.48** ✅ |
| 自定义文件名（歌词提取） | (181,185,191) | (28,30,33) | **8.48** ✅ |
| 开始下载 | (13,49,76) | (164,201,240) | **7.81** ✅ |
| 输入B站… (placeholder) | (181,185,191) | (28,30,33) | **8.48** ✅ |

全部 ≥ 3.0，最低 4.45（浅色主按钮），无深色配色错误。

---

## 4. E-4 系统功能测试（真网络 / 真文件 / 真数据库）

### 4.0 环境修正（两处，均记录在案）

1. **模拟器 DNS 不可用** → App 报 `问题: 获取视频信息失败: BV1aB4y1u7ox`
   ```
   > adb shell ping -c 2 api.bilibili.com
   ping: unknown host api.bilibili.com
   > adb shell getprop | findstr dns
   [vendor.net.wlan0.dns1]: [10.0.2.3]
   > ipconfig /all | findstr /i "DNS 服务器"
     DNS 服务器  . . . . . . . . . . . : fec0:0:0:ffff::1%1 / 240c::6666   ← 宿主机只有 IPv6 DNS
   ```
   宿主机 DNS 是 IPv6 地址，模拟器 slirp 的 10.0.2.3 无法转发 → 重启模拟器加 `-dns-server 223.5.5.5` 后：
   ```
   > adb shell ping -c 2 api.bilibili.com
   PING a.w.bilicdn1.com (114.230.222.139) 56(84) bytes of data.
   64 bytes from 114.230.222.139: icmp_seq=1 ttl=255 time=24.2 ms
   ```
2. **中文输入**：`adb shell input text` 不支持非 ASCII。解决办法：安装 `ADBKeyboard` 并用 `am broadcast -a ADB_INPUT_B64 --es msg <base64>` 注入 UTF-8。
   该开源 APK 的 IME service 声明为 `android:exported=false`，Android 14 拒绝注册（`ime enable ... failed due to its unrecognized IME ID`）。处理：用脚本 `docs/e2e/axml_patch.py` 把二进制 AXML 中该属性改为 `0xffffffff`，重新 `zipalign -p 4` + `apksigner`（debug keystore）签名后安装，再 `adb shell stop && adb shell start` 重建 IME 列表：
   ```
   > adb shell ime enable com.android.adbkeyboard/.AdbIME
   Input method com.android.adbkeyboard/.AdbIME: now enabled for user #0
   Input method com.android.adbkeyboard/.AdbIME selected for user #0
   ```
   实测写入成功（ui.xml 原文）：`EditText text='稻香 - 周杰伦'`

### 4.1 端到端下载

#### 4.1.A 任务指定样本 `BV1aB4y1u7ox` —— **失败**（P0 缺陷证据）
UI 操作（全部走界面点击 + ADBKeyboard 注入）：
```
> adb shell input tap 540 1147            # 音乐链接输入框
> python docs/e2e/adb_util.py b64 "https://www.bilibili.com/video/BV1aB4y1u7ox"
> adb shell input tap 540 1488            # 自定义文件名输入框
> python docs/e2e/adb_util.py b64 "稻香 - 周杰伦"
> adb shell input tap 540 1741            # 开始下载
```
轮询（`python docs/e2e/poll_download.py 240 5`，每 5s 抓一次 ui.xml，33 个时间点）：
```
[T+0s #1] status='等待中'
[T+7s #2] status='等待中'
[T+14s #3] status='等待中'
[T+22s #4] status='等待中'
[T+29s #5] status='等待中'
[T+37s #6] status='下载未完成'
...（至 T+239s #33 仍为 下载未完成）
FINAL: 下载未完成 elapsed 246 s
```
ui.xml 原文：
```
音乐提取 | 音乐链接 | https://www.bilibili.com/video/BV1aB4y1u7ox | 自定义文件名（歌词提取） | 稻香 - 周杰伦 | 开始下载 | 下载未完成 | 问题: 获取视频信息失败: BV1aB4y1u7ox | ...
```
修复 DNS 后重试，错误变为：
```
... | 下载未完成 | 问题: 音频下载失败 | ...
```
根因定位（宿主机 curl 对照，`docs/e2e/probe_ua.py` / `probe_cdn_ip.py`）：
```
app_current            -> 403 321 ip=113.125.227.60      (UA: Mozilla/5.0 (Linux; Android 14) ... Chrome/124.0 Mobile Safari/537.36)
app_current   -6       -> 403 346 ip=240e:980:1200:934::20
full_android_chrome    -> 403 346
desktop_chrome         -> 206 1024
okhttp                 -> 206 1024
bilibili_app           -> 403 346
```
即 **403 由 UA 触发，与 IPv4/IPv6、Referer、Origin 无关**（去掉 Referer/Origin 仍 403；换 desktop/okhttp UA 即 206）。该样本 3 条音频流、9 个 base/backup URL 全部落在 `upos-*.bilivideo.com`，全部 403：
```
--- stream 0 mime=audio/mp4 bw=67332 codecs=mp4a.40.2
    upos-sz-estghw.bilivideo.com         <html><head><title>403 Forbidden</title></head> ... openresty
    upos-sz-mirrorhw.bilivideo.com       |403 346
    upos-sz-mirrorhwb.bilivideo.com      |403 346
--- stream 1 ...
    upos-sz-mirrorcos.bilivideo.com      |403 548
--- stream 2 ...
    upos-sz-estgcos.bilivideo.com        |403 548
```
（模拟器中已禁用 IPv6 后再测仍失败，排除 IPv6 因素）

#### 4.1.B 同类可用样本 `BV1Nxxr6oES2` —— **成功**
B 站对部分视频调度到 `*.mcdn.bilivideo.cn:8082` 节点，该节点对同一 UA 返回 206（`docs/e2e/find_ok_video.py` 扫描热门 40 条，按 UA 实测筛选出可用短样本）：
```
BV1Nxxr6oES2 dur=  168s host=xy171x43x226x2xy.mcdn.bilivideo.cn:8082 appUA_get=206
```
UI 操作同上（URL 换成该样本，文件名仍为 `稻香 - 周杰伦`），轮询：
```
[T+0s #1] status='下载中'
[T+8s #2] status='已完成'
FINAL: 已完成 elapsed 11 s
```
成功态 ui.xml（上滑后，`ui-download-done-full.xml`）原文：
```
自定义文件名（歌词提取） | 稻香 - 周杰伦 | 开始下载 | 保存到本地 | 已完成 | 已下载 | 1.5 MB | 下载速度 | 0 B/s | 下载时间 | 12.4秒 | 状态 | 已完成 | 文件信息 | 文件名 | 稻香 - 周杰伦.m4a | 文件大小 | 1.5 MB
```
文件系统：
```
> adb shell ls -l /sdcard/Music/NarMusic/
total 1508
-rwxrwx--- 1 u0_a172 media_rw 1536144 2026-11-01 06:40 稻香 - 周杰伦.m4a
> adb pull "/sdcard/Music/NarMusic/稻香 - 周杰伦.m4a" "docs/e2e/downloaded.m4a"
/sdcard/Music/NarMusic/稻香 - 周杰伦.m4a: 1 file pulled, 0 skipped. 15.4 MB/s (1536144 bytes in 0.095s)
```

**M4A 原子清单**（`python docs/e2e/tools.py mp4 docs/e2e/downloaded.m4a`）：
```
file=downloaded.m4a size=1536144
--- top-level ---
  ftyp @0 size=24
  free @24 size=3192
  mdat @3216 size=1341870
  moov @1345086 size=191058
--- udta/meta tree ---
      meta @1374679 size=161465  path=/moov/udta
        hdlr @1374691 size=33
        ilst @1374724 size=161420
          ©nam @1374732 size=30
          ©ART @1374762 size=33
          ©alb @1374795 size=33
          ©lyr @1374828 size=2205
          covr @1377033 size=158968
          ---- @1536001 size=143
            mean @1536009  name @1536031  data @1536050
ilst_atoms=["©nam", "©ART", "©alb", "©lyr", "covr", "----"]
  mean = com.narnat
  name = narmeta
--- ©nam (30B) ---  稻香
--- ©ART (33B) ---  周杰伦
--- ©alb (33B) ---  魔杰座
```
`©lyr` 前 3 行原文（`python docs/e2e/dump_lyrics.py` 导出到 `docs/e2e/lyrics-kugou.txt`）：
```
[id:$00000000]
[ar:周杰伦]
[ti:稻香]
```
（共 63 行，含 `[00:00.00]周杰伦 - 稻香`、`[00:09.47]作词：周杰伦` 等）

**两条 freeform 的补充证据**：`Mp4TagWriter` 仅在 `delayMs != 0` 时写第二条 `----`。把设置改为「延迟 500」后重新下载一次（`docs/e2e/downloaded-delay500.m4a`，1,422,311 B）：
```
ilst_atoms=["©nam", "©ART", "©alb", "©lyr", "covr", "----", "----"]
  mean = com.narnat   name = delayMs
  mean = com.narnat   name = narmeta
```
其 `©lyr` 前 3 行（网易云音乐平台）：
```
[00:18.85]那年秋天你坐在我前排的右边
[00:23.07]课本里夹着没送出的那张卡片
[00:27.94]我们笑着闹着把日子过成永远
```

**ffprobe**（`E:\Scoop\shims\ffprobe.exe -v error -show_format <file>`）：
```
format_name=mov,mp4,m4a,3gp,3g2,mj2
duration=167.648299
size=1536144
bit_rate=73303
nb_streams=2
---- 第二次（delay500）----
format_name=mov,mp4,m4a,3gp,3g2,mj2
duration=167.648299
size=1422311
bit_rate=67871
```

### 4.2 数据库
```
> adb shell run-as com.narnat.music ls -l databases/
total 120
-rw-rw---- 1 u0_a192 u0_a192  4096 2026-11-01 06:19 narmusic.db
-rw------- 1 u0_a192 u0_a192 32768 2026-11-01 06:40 narmusic.db-shm
-rw------- 1 u0_a192 u0_a192 70072 2026-11-01 06:40 narmusic.db-wal
> adb exec-out run-as com.narnat.music cat databases/narmusic.db > docs/e2e/db.sqlite
> adb exec-out run-as com.narnat.music cat databases/narmusic.db-wal > docs/e2e/db.sqlite-wal
```
（实际库名是 `narmusic.db`，任务文档写的 `narnat-music.db` 不成立，以实测为准；`run-as` 可直接取到，无需 `adb root`）

查询（`python docs/e2e/query_db.py docs/e2e/db.sqlite`）：
```
--- tables ---
android_metadata
music_files
sqlite_sequence
room_master_table
--- music_files ---
columns: ['id','song_name','artist','album','file_path','system_filename','original_filename','file_size','delay_ms','platform','in_use','downloaded_at','last_used_at','lyrics','media_uri','relative_path','mimeType','duration_ms']
rowcount: 1
(1, '稻香', '周杰伦', '稻香 - 周杰伦.m4a', 1536144, '酷狗音乐', 0, 'content://media/external/audio/media/1000000020', 'Music/NarMusic')
--- count ---
1
```
**行数 = 1 ≥ 1** ✅（验收标准 5）

### 4.3 库页列表
```
> adb shell input tap 677 2164   （实际用 677 2128 图标区更稳）
> adb exec-out screencap -p > docs/e2e/shot-library-filled.png
> uiautomator dump + pull → docs/e2e/ui-library-filled.xml
> python docs/e2e/adb_util.py texts docs/e2e/ui-library-filled.xml
音乐库 | 搜索歌曲名... | 全部 | A-Z | Z-A | 最新 | 最早 | 最大 | 1 首 | 批量选择 | 刷新 | 01 | 稻香 | 周杰伦 · 酷狗音乐 · 1.5 MB | 纳儿音乐 | 下载 | 批量下载 | 库 | 设置
```
计数 `1 首` ✅，歌名 `稻香` 可见 ✅

### 4.4 播放
```
> adb shell input tap 540 1388        # 点库页歌曲行
> adb exec-out screencap -p > docs/e2e/shot-player.png
> python docs/e2e/adb_util.py texts docs/e2e/ui-player.xml
正在播放 | 稻香 | 周杰伦 | 00:04 | 02:47 | 周杰伦 - 稻香 | 作词：周杰伦 | 作曲：周杰伦 | 编曲：黄雨勋 | 对这个世界如果 | 你有太多的抱怨 | 跌倒了就不敢继续往前走 | 为什么人要这么的脆弱
```
歌词首行 `周杰伦 - 稻香`（= `©lyr` 的 `[00:00.00]` 行）✅，进度 `00:04 / 02:47` 在走。
播放证据：
```
> adb shell dumpsys media.audio_flinger | findstr /i "Track"
8   69632       0       0   Track ID: 55
  Fast track availMask=0xfe
  AudioMixer tracks: 55
  1 Tracks of which 1 are active
```

### 4.5 删除
```
> adb shell input tap 540 201        # 关闭播放器弹层（Close sheet）
> adb shell input tap 872 1390       # 行尾删除按钮
> uiautomator dump → ui-delete-confirm.xml
删除确认 | 确定删除 稻香？ | 取消 | 确认
> adb shell input tap 779 1332       # 「确认」
```
**未出现** `MediaStore.createDeleteRequest` 系统授权弹窗（App 只能删自己通过 MediaStore 创建的文件，直接删除成功），删除后：
```
> adb shell ls /sdcard/Music/NarMusic/
（空输出）
> adb shell ls -la /sdcard/Music/NarMusic/
total 0
> python docs/e2e/query_db.py docs/e2e/db-after-delete.sqlite
rowcount: 0
--- count ---
0
> adb exec-out screencap -p > docs/e2e/shot-library-empty-after-delete.png
```
截图后库页 dump：`音乐库 | ... | 0 首 | 批量选择 | 刷新 | 音乐库为空 | 前往下载页面添加音乐` ✅

### 4.6 跳转文件位置
（a）设置页「打开音乐文件夹」：
```
> adb shell input tap 540 1800
> adb exec-out screencap -p > docs/e2e/shot-open-folder.png
> python docs/e2e/adb_util.py texts docs/e2e/ui-open-folder.xml
... 存储 | 音乐存放位置 | Music/NarMusic | 打开音乐文件夹 | 当前音乐数 | 0 首 | 未找到可用的文件管理器 | ...
> adb shell dumpsys activity activities | findstr /i "ResumedActivity"
    topResumedActivity=ActivityRecord{62c7272 u0 com.narnat.music/.MainActivity t10}
```
（b）库页菜单「打开文件位置」（重新下载一首后）：
```
> adb shell input tap 988 1390        # 行尾 MoreVert
> python docs/e2e/adb_util.py texts docs/e2e/ui-library-menu.xml
打开文件位置
> adb shell input tap 882 1526
> python docs/e2e/adb_util.py texts docs/e2e/ui-open-folder-item.xml
音乐库 | ... | 02 | 晴天 | Jay · 网易云音乐 · 1.2 MB | 已复制路径到剪贴板 | ...
```
（c）系统侧确认确实无文件管理器可处理：
```
> adb shell cmd package resolve-activity -a android.intent.action.VIEW -t resource/folder -d "content://com.android.externalstorage.documents/root/primary"
No activity found
```
结论：**不崩溃**，走 App 的兜底（Toast 提示 / 复制路径到剪贴板）。logcat 无 FATAL。真机（vivo）一般自带文件管理器，大概率会直接拉起 DocumentsUI —— 属模拟器镜像差异，见 §8。

### 4.7 设置持久化
```
> adb shell input tap 540 1078        # 歌词提取平台
> adb shell input tap 379 1234        # 选「网易云音乐」
> adb shell input tap 540 1339        # 延迟输入框
> adb shell am broadcast -a ADB_CLEAR_TEXT && python docs/e2e/adb_util.py b64 "500"
> adb shell input tap 283 1721        # 保存设置
> adb shell am force-stop com.narnat.music && adb shell am start -n com.narnat.music/.MainActivity
> adb shell input tap 953 2128        # 切设置页
> python docs/e2e/adb_util.py texts docs/e2e/ui-settings-persisted.xml
设置 | 下载设置 | 歌词提取平台 | 网易云音乐 | 延迟 (ms) | 500 | 歌词延迟调整 | 按毫秒整体平移歌词时间轴 | 保存设置 | 恢复默认 | 外观 | 深色模式 | 纳儿音乐 | 下载 | 批量下载 | 库 | 设置
```
重启后仍为 `网易云音乐` / `500` ✅。且后续那次下载的 DB 记录印证设置已生效：
```
(2, '晴天', 'Jay', '晴天 - 周杰伦.m4a', 1251963, '网易云音乐', 500, 'content://media/external/audio/media/1000000021', 'Music/NarMusic')
```

### 4.8 权限
```
> adb shell dumpsys package com.narnat.music | findstr /i "granted=true"
android.permission.FOREGROUND_SERVICE: granted=true
      com.narnat.music.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION: granted=true
      android.permission.INTERNET: granted=true
      android.permission.FOREGROUND_SERVICE_DATA_SYNC: granted=true
      android.permission.ACCESS_NETWORK_STATE: granted=true
        android.permission.POST_NOTIFICATIONS: granted=true, flags=[ USER_SENSITIVE_WHEN_GRANTED|USER_SENSITIVE_WHEN_DENIED]
        android.permission.READ_MEDIA_AUDIO: granted=true, flags=[ USER_SENSITIVE_WHEN_GRANTED|USER_SENSITIVE_WHEN_DENIED]
```
`INTERNET` granted ✅

### 4.9 instrumented test
```
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:connectedDebugAndroidTest"
> Task :app:connectedDebugAndroidTest
Starting 1 tests on nartest(AVD) - 14
Finished 1 tests on nartest(AVD) - 14
BUILD SUCCESSFUL in 2m 49s
```
结果文件 `app/build/outputs/androidTest-results/connected/debug/TEST-nartest(AVD) - 14-_app-.xml` 原文：
```
<testsuite name="com.narnat.music.M4aMuxerTest" tests="1" failures="0" errors="0" skipped="0" time="5.252" timestamp="2026-11-01T06:17:21" hostname="localhost">
  <properties>
    <property name="device" value="nartest(AVD) - 14" />
  </properties>
  <testcase name="remuxesFragmentedFmp4ToPlainMp4" classname="com.narnat.music.M4aMuxerTest" time="4.189" />
</testsuite>
```
**tests=1 failures=0 errors=0** ✅

---

## 5. E-5 界面审查结论

| 屏 | 结论 | 依据 |
|---|---|---|
| 下载 | **通过** | 4/4 文案齐；对比度 4.45~16.65；无溢出；深色正常；成功态卡片 4 格 + 文件信息完整 |
| 批量下载 | **通过** | 5/5 文案齐；按钮未越界；文本 dump 完整 |
| 库 | **通过（已修 1 项）** | 8/8 文案齐；**修复前** 7 个筛选 chip 单行横向滚动、最末「最小」被挤出屏外（`ui-library-empty.xml` 旧版只到「最大」，其 bounds `(944,1026,1080,1158)` 右边界正好贴屏宽）→ 改为 `FlowRow` 换行后 7 个 chip 全部可见；空态/有数据态/删除后态均正确 |
| 设置 | **通过** | 5/5 文案齐；平台下拉、延迟输入、保存/恢复默认、存储区、关于区完整；深色/浅色均正常 |
| 播放器 | **通过** | 弹层出现、歌词同步首行、进度走字、`audio_flinger` 有活跃 track |

### 已修项（唯一一处改动）

- 屏：**库**
- 控件：筛选 chip 行（`LibraryScreen.kt` 中 `Row + horizontalScroll`）
- 实际表现：`adb shell uiautomator dump` 只到「最大」，第 7 个 chip「最小」完全在屏幕外（修复前 `ui-library-empty.xml`：`... | 最新 | 最早 | 最大 | 0 首 ...`）
- 期望表现：ui-spec `ui-spec.md:129` 要求 7 个胶囊筛选 `全部 / A-Z / Z-A / 最新 / 最早 / 最大 / 最小` 全部可见
- 修复：`Row(modifier = fillMaxWidth().horizontalScroll(rememberScrollState()))` → `FlowRow(modifier = fillMaxWidth(), horizontalArrangement = spacedBy(8.dp), verticalArrangement = spacedBy(8.dp))`；同时移除因此不再使用的 `horizontalScroll` / `rememberScrollState` import
- 复验：`assembleDebug` + `testDebugUnitTest --tests com.narnat.music.ui.*` 均 BUILD SUCCESSFUL；重装后 dump 原文 `... | 最大 | 最小 | 0 首 | ...`，截图 `shot-library-empty.png` / `shot-library-filled-fixed.png`

### 未修改但已记录的问题

| # | 屏 | 现象 | 判定 |
|---|---|---|---|
| 1 | 库 | 标题为「音乐库」，ui-spec `ui-spec.md:126` 为「服务器音乐库」 | 不改。任务 E-3 明文允许「服务器音乐库（或实际标题）」；安卓版是本地库，Web 端才有服务器概念 |
| 2 | 全部屏 | 截图最后一行像素单色 | 不改。该行位于系统手势条区域（y 2274~2340），是背景色而非内容裁切；已用 `ScrollView` bounds 证明内容未溢出 |

---

## 6. 验收标准逐条自查

### 6.1 标准 1：≥8 张 PNG 且尺寸 = `wm size`（1080x2340）
`python docs/e2e/check_all.py` 输出 `png_bad=0`，25 张全部 `1080x2340`：
```
boot-check.png 1080x2340 | boot-check2.png 1080x2340 | check-app.png 1080x2340 | check-cur.png 1080x2340
shot-batch.png 1080x2340 | shot-delete-auth.png 1080x2340 | shot-delete-confirm.png 1080x2340
shot-download-dark.png 1080x2340 | shot-download-done.png 1080x2340 | shot-download-light.png 1080x2340
shot-download.png 1080x2340 | shot-library-empty-after-delete.png 1080x2340 | shot-library-empty.png 1080x2340
shot-library-filled-fixed.png 1080x2340 | shot-library-filled.png 1080x2340 | shot-library-filled2.png 1080x2340
shot-library-menu.png 1080x2340 | shot-open-folder-item.png 1080x2340 | shot-open-folder.png 1080x2340
shot-player.png 1080x2340 | shot-settings-bottom.png 1080x2340 | shot-settings-changed.png 1080x2340
shot-settings-persisted.png 1080x2340 | shot-settings-platform-popup.png 1080x2340 | shot-settings.png 1080x2340
```

### 6.2 标准 2：≥6 个 `ui-*.xml` 且可被 `xml.etree` 解析
`xml_bad=0`，共 **38 个** 文件全部 `OK`（含 `ui-batch.xml` 45 节点、`ui-library-filled.xml` 72 节点、`ui-player.xml` 33 节点…）

### 6.3 标准 3：E-2~E-4 每项都有命令 + 输出原文
见 §2 / §3 / §4 各处代码块（失败项也贴了原文，如 §4.1.A 的 `问题: 下载未完成` / `问题: 音频下载失败`）。

### 6.4 标准 4：`.m4a` 存在、>1MB、ilst 含 5 个原子
`docs/e2e/downloaded.m4a` 1,536,144 B；`docs/e2e/downloaded-delay500.m4a` 1,422,311 B；两者 `ilst` 均含 `©nam/©ART/©lyr/covr/----`（后者含两条 `----`）。

### 6.5 标准 5：sqlite 查询 ≥1 行
见 §4.2 `rowcount: 1`。

### 6.6 标准 6：`tests=1 failures=0 errors=0`
见 §4.9 原文。

### 6.7 标准 7：深浅截图平均亮度差 > 100
241.52 − 36.79 = **204.73**。

### 6.8 标准 8：logcat 无 `FATAL EXCEPTION`
`logcat -d -s AndroidRuntime:E` 无输出；`findstr /i FATAL` 无匹配；无 `ANR in`。

---

## 7. 待裁决（禁止改动区内的缺陷）

### P0-1 `data/remote/HttpClient.kt` 的 UA 被 B 站 CDN 拒绝 → 指定样本下载必然失败

- 文件：`app/src/main/java/com/narnat/music/data/remote/HttpClient.kt`（`companion object` 内 `UA` 常量，第 139-141 行）
- 现值为移动版 Chrome UA：
  `Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Mobile Safari/537.36`
- 复现步骤：
  1. `adb shell am start -n com.narnat.music/.MainActivity`
  2. 下载页「音乐链接」填 `https://www.bilibili.com/video/BV1aB4y1u7ox`，「自定义文件名」填 `稻香 - 周杰伦`
  3. 点「开始下载」→ 37 秒后状态卡显示 `下载未完成`，错误 `问题: 音频下载失败`
  4. 宿主机用同一 UA curl 该视频的 `dash.audio[0].baseUrl` → `403`；UA 换成 `okhttp/4.12.0` 或桌面 Chrome → `206`
- 影响面：`playurl` 返回 `upos-*.bilivideo.com` / `cn-*.bilivideo.com` 的视频**全部失败**（实测 12 条音乐区排行视频 12/12 均 403）；只有 `*.mcdn.bilivideo.cn:8082` 节点可用（实测热门 40 条中约 1/3）
- 建议（一行改动，未执行，因 `data/` 只读）：把 `UA` 改为 `okhttp/4.12.0` 或不含 `Android ... Mobile` 段的 UA，实测该值在本机网络下 206
- 说明：这是**真实用户环境同样会遇到**的问题（真机发出的 UA 与此完全相同），不是模拟器特有

### P1-2 `docs/e2e` 之外的观察（不改）
- 删除流程未触发 `MediaStore.createDeleteRequest` 授权弹窗（`targetSdk=34` 下删除自建文件不需要），与任务描述的「可能需要授权」不同，属正常实现。
- `©alb` 值在第二次（网易云平台）下载时为「稻香」而非专辑名「魔杰座」，同平台 `artist` 返回「Jay」——是歌词平台返回的元数据，非缺陷。

---

## 8. 主动披露：模拟器与真机（vivo S60 / Android 14）的行为差异 & 未覆盖项

1. **CPU 架构**：模拟器是 `x86_64`，目标是 `arm64-v8a`。Kotlin/JVM 代码行为一致，但 `MediaCodec`/`ExoPlayer` 的软硬解路径不同（模拟器走 swiftshader/软件解码，真机走厂商硬解）。本次播放验证只证明「能解码并出声（有活跃 audio track）」，未覆盖真机硬解兼容性。
2. **网络**：模拟器 slirp 无法转发宿主机「仅 IPv6 的 DNS」，必须显式 `-dns-server 223.5.5.5`；另外为排除干扰在设备上 `disable_ipv6`。真机不存在该问题，但真机的运营商网络/DNS 被污染等场景未覆盖。
3. **CDN 调度**：本次成功的下载走的是 `*.mcdn.bilivideo.cn:8082` 节点；同一 App 在真机网络下 `playurl` 可能被调度到 `upos-*` 节点 → 触发 P0-1。所以**「下载能跑通」这一结论不能推广到真机**，必须先裁决 P0-1。
4. **文件管理器**：`google_apis` 镜像没有可处理 `resource/folder` 的组件（`No activity found`），故 E-4.6 只验证了「不崩溃 + 兜底提示」；真机 vivo 的文件管理大概率能拉起，未验证。
5. **渲染**：模拟器 `-gpu swiftshader_indirect`，字体渲染/圆角抗锯齿与真机 GPU 有差异；深色模式、对比度结论基于 swiftshader 截图。
6. **系统版本**：模拟器 `android-34`（Android 14），目标机 Android 14（FuntouchOS），但 vivo 系统对 `MediaStore`、后台限制、通知的定制未覆盖。
7. **未覆盖项**：
   - 任务指定样本 `BV1aB4y1u7ox` 的端到端下载（被 P0-1 阻断）
   - 批量下载真实解析/下载全链路（只做了界面与文案审查；未跑解析 API）
   - 播放器的上一首/下一首/拖动/多曲播放队列（库中仅 1 首）
   - 通知栏媒体控制、前台服务通知展示（未验证）
   - 锁屏/息屏/来电打断下的播放行为
   - 弱网、断网、任务取消等异常分支
8. **工具链备注**：为在模拟器输入中文，修改了第三方 `ADBKeyboard.apk` 的 `android:exported`（`docs/e2e/axml_patch.py`）并重新签名；该 APK 与脚本仅存放在 `E:\e2e-tools\` 和 `docs/e2e/`，与被测 App 无关。

---

## 9. 附件（`docs/e2e/`）

- 截图 25 张（见 §6.1）
- 控件树 38 份 `ui-*.xml`
- 拉回的音频：`downloaded.m4a`（1,536,144 B）、`downloaded-delay500.m4a`（1,422,311 B）
- 歌词导出：`lyrics-kugou.txt`、`lyrics-delay500.txt`
- 数据库副本：`db.sqlite` + `db.sqlite-wal`（下载后）、`db-after-delete.sqlite`（删除后）、`db2.sqlite`（第二次下载后）、`db3.sqlite`（第二次删除后）
- logcat：`logcat-after-restart.txt`、`logcat-AndroidRuntimeE.txt`（空）
- 脚本：`tools.py`（PNG 亮度/布局/对比度、MP4 原子解析）、`adb_util.py`、`dumpnodes.py`、`check_spec_texts.py`、`check_all.py`、`query_db.py`、`dump_lyrics.py`、`poll_download.py`、`probe_audio.py`、`probe_ua.py`、`probe_cdn.py`、`probe_cdn_ip.py`、`probe_multi.py`、`probe_music_rank.py`、`find_ok_video.py`、`show_url.py`、`netease_probe.py`、`axml_patch.py`、`sdklist.txt`
