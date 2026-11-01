# 任务 R 完成报告：链接解析回归 / 应用图标 / 椒盐式界面重构 / 横竖屏适配

> 环境：模拟器 `nartest`（Android 14 / API 34，`wm size` = `Physical size: 1080x2340`）
> 构建：`cmd /c "set TMP=E:\android-user\tmp&& set TEMP=E:\android-user\tmp&& set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon <task>"`
> 所有截图来自 `adb exec-out screencap -p`，未使用 Compose Preview。

---

## ① 根因说明（R-1 回归发现的第一个关键事实）

**「指定链接却下载了随机音乐」的根因与任务书描述一致**，并且我实测复现了旧行为：

1. 用**未包含本次修复的旧产物** `dist/NarMusic-v1.0.0.apk`（构建时间 15:54，早于 `core/bili/LinkPlan.kt` 与 `domain/DownloadService.kt` 的 17:55 修改）安装后，粘贴
   `【百万级装备听《Counting Stars》- OneRepublic 【Hi-Res】-哔哩哔哩】 https://b23.tv/Ndg49HZ`
   落库的 `narmeta` 是 `https://www.bilibili.com/video/BV1UCm6YQEeM`（另一个视频「《燃尽の小曲》《努力の小曲》Counting Stars-OneRepublic」），
   与短链真实目标 `BV14v4y1Q7nG` 完全不符 —— 这就是用户看到的 P0。
2. 短链 `https://b23.tv/Ndg49HZ` 的真实重定向目标（宿主机实测）：
   ```
   code=302 url=https://www.bilibili.com/video/BV14v4y1Q7nG?buvid=...&unique_k=Ndg49HZ&up_id=440121192
   ```
3. 用**包含修复的源码**重新构建后，同一输入落到 `av563960151`，B 站 API 校验 `aid=563960151 → bvid=BV14v4y1Q7nG`，即下载到了指定的那一首。

结论：修复有效，本次未改动 `core/**`、`domain/**` 任何一行。

---

## ② R-1 端到端回归（三条路径，全部通过）

安装命令：
```bat
cmd /c "set TMP=E:\android-user\tmp&& set TEMP=E:\android-user\tmp&& E:\android-sdk\platform-tools\adb.exe uninstall com.narnat.music && E:\android-sdk\platform-tools\adb.exe install -r app\build\outputs\apk\debug\app-debug.apk"
```

| 路径 | 输入 | 结果 | 证据 |
|---|---|---|---|
| 1. 短链 | `【百万级装备听《Counting Stars》- OneRepublic 【Hi-Res】-哔哩哔哩】 https://b23.tv/Ndg49HZ`（原样粘贴，含中文） | **成功**，落到 BV14v4y1Q7nG | 见下方 `ilst` dump + aid/bvid 互证 |
| 2. 完整链接 + 文件名 | `https://www.bilibili.com/video/BV1aB4y1u7ox` + `稻香 - 周杰伦` | **成功** | `narmeta` 前缀 `https://www.bilibili.com/video/av588323544`，API 校验 `aid=588323544 → bvid=BV1aB4y1u7ox` |
| 3. 纯关键词 | `周杰伦 晴天` | **成功**（搜索路径未被改坏） | 落盘文件 `【4K修复】周杰伦 - 晴天MV 2160P修复版.m4a`，1706927 字节 |

### 路径 1：`narmeta` 原子 dump 原文

`python mp4dump.py r1short2-0.m4a`（拉回设备的 `百万级装备听《Counting Stars》- OneRepublic【Hi-Res】.m4a`）:
```
        ---- (size=138)
          mean (size=22)
            text: b'\x00\x00\x00\x00com.narnat'
          name (size=19)
            text: b'\x00\x00\x00\x00narmeta'
          data (size=138)
            data: b'https://www.bilibili.com/video/av563960151|\xe9\x85\xb7\xe7\x8b\x97\xe9\x9f\xb3\xe4\xb9\x90|+0|\xe7\x99\xbe\xe4\xb8\x87\xe7\xba\xa7\xe8\xa3\x85\xe5\xa4\x87\xe5\x90\xac\xe3\x80\x8aCounting Stars\xe3\x80\x8b- OneRepublic\xe3\x80\x90Hi-Res\xe3\x80\x91'
```
（解出的中文：`酷狗音乐` / `百万级装备听《Counting Stars》- OneRepublic【Hi-Res】`）

aid ↔ bvid 互证（`python bvcheck2.py`）:
```
aid=563960151      -> code=0 aid=563960151 bvid=BV14v4y1Q7nG title=百万级装备听《Counting Stars》- OneRepublic【Hi-Res】
bvid=BV14v4y1Q7nG  -> code=0 aid=563960151 bvid=BV14v4y1Q7nG title=百万级装备听《Counting Stars》- OneRepublic【Hi-Res】
aid=588323544      -> code=0 aid=588323544 bvid=BV1aB4y1u7ox title=【光遇琴谱】回家吧，回到最初的美好《稻香》[周杰伦][吉他3指]
bvid=BV1aB4y1u7ox  -> code=0 aid=588323544 bvid=BV1aB4y1u7ox title=【光遇琴谱】回家吧，回到最初的美好《稻香》[周杰伦][吉他3指]
```

### 路径 2：`narmeta` 原子 dump 原文
```
          data: b'https://www.bilibili.com/video/av588323544|\xe9\x85\xb7\xe7\x8b\x97\xe9\x9f\xb3\xe4\xb9\x90|+0|\xe7\xa8\xbb\xe9\xa6\x99 - \xe5\x91\xa8\xe6\x9d\xb0\xe4\xbc\xa6'
```
（`酷狗音乐` / `稻香 - 周杰伦`）

### 路径 3：UI 状态卡文本（`uiautomator dump` 原文）
```
音乐提取 | 音乐链接 | 周杰伦 晴天 | 自定义文件名（歌词提取） | 歌曲名称 - 作者 | 开始下载 | 下载中 | 纳儿音乐 | ...
音乐提取 | 音乐链接 | 周杰伦 晴天 | ... | 保存到本地 | 已完成 | 文件下载完成 | 纳儿音乐 | ...
设备落盘：-rwxrwx--- 1 u0_a172 media_rw 1706927 2026-11-01 10:16 【4K修复】周杰伦 - 晴天MV 2160P修复版.m4a
```

### 旧 APK 对照（P0 复现证据）
```
设备落盘：《燃尽の小曲》《努力の小曲》Counting Stars-OneRepublic.m4a  1464387 字节
narmeta:  https://www.bilibili.com/video/BV1UCm6YQEeM|酷狗音乐|+0|《燃尽の小曲》《努力の小曲》Counting Stars-OneRepublic
API:      bvid=BV1UCm6YQEeM -> aid= 113458154574517 title=《燃尽の小曲》《努力の小曲》Counting Stars-OneRepublic
```

截图：`docs/e2e/r-01-short-input.png`、`r-03-short-done.png`、`r-04-full-input.png`、`r-05-full-done.png`、`r-06-kw-input.png`、`r-07-kw-done.png`

---

## ③ R-2 应用图标

### 图形设计（单色几何音符）
- 画布 108×108dp，图形全部落在中间 72dp 安全区内（实测包络：x 33.2–74.5、y 28–79.9；距画布中心最远 29.7dp < 36dp 安全半径）。
- 三个纯几何基本形，无渐变、无文字、单一白色 `#FFFFFF`：
  1. **符头**：`M33.19,74.93 A11.5,8.5 -20 1,0 54.81,67.07 A11.5,8.5 -20 1,0 33.19,74.93 Z`（rx 11.5 / ry 8.5，逆时针倾斜 20° 的椭圆）
  2. **符干**：`M51.5,28 L57.5,28 L57.5,71 L51.5,71 Z`（6dp 宽竖条）
  3. **符尾**：`M57.5,28 C70,32 74.5,44 70.2,55 C69.6,42.5 63.8,36 57.5,33.6 Z`（双三次贝塞尔围成的弯钩）
- 背景层 `#2D7BC1`（NarMusic 主色）纯色。
- `monochrome` 复用 foreground，支持 Android 13 主题图标。

### 产物
| 文件 | 说明 |
|---|---|
| `app/src/main/res/drawable/ic_launcher_foreground.xml` | 108dp vector，白色音符 |
| `app/src/main/res/drawable/ic_launcher_background.xml` | 108dp vector，`#2D7BC1` 纯色 |
| `app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml` | `<adaptive-icon>` + `<monochrome>` |
| `app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml` | 同上 |
| `app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png` | 48/72/96/144/192 px，圆角方形底 |
| `app/src/main/res/mipmap-{mdpi..xxxhdpi}/ic_launcher_round.png` | 同上尺寸，圆形底 |
| `app/src/main/AndroidManifest.xml` | 新增 `android:icon` / `android:roundIcon` |

PNG 由 `docs/e2e/gen_icon.py` 用 Pillow 以 8× 超采样渲染与 vector 等价的几何，再 LANCZOS 缩到各档。

### 验收：`aapt2 dump badging`（dist 里的 release APK）
```bat
E:\android-sdk\build-tools\35.0.0\aapt2.exe dump badging dist\NarMusic-v1.0.0.apk
```
```
application-icon-160:'res/BW.xml'
application-icon-240:'res/BW.xml'
application-icon-320:'res/BW.xml'
application-icon-480:'res/BW.xml'
application-icon-640:'res/BW.xml'
application-icon-65534:'res/BW.xml'
application: label='NarMusic' icon='res/BW.xml'
launchable-activity: name='com.narnat.music.MainActivity'  label='' icon=''
```

各 dpi 档位的 PNG 确实在包里（`python apk_icons.py` → `aapt2 dump resources`）：
```
resource 0x7f0f0000 mipmap/ic_launcher
    (mdpi) (file) res/9w.png type=PNG
    (hdpi) (file) res/yn.png type=PNG
    (xhdpi) (file) res/FS.png type=PNG
    (xxhdpi) (file) res/RJ.png type=PNG
    (xxxhdpi) (file) res/o-.png type=PNG
    (anydpi) (file) res/BW.xml type=XML
resource 0x7f0f0001 mipmap/ic_launcher_round
    (mdpi) (file) res/zR.png type=PNG
    (hdpi) (file) res/8c.png type=PNG
    (xhdpi) (file) res/wb.png type=PNG
    (xxhdpi) (file) res/w_.png type=PNG
    (xxxhdpi) (file) res/Gc.png type=PNG
    (anydpi) (file) res/0K.xml type=XML
```

### 桌面截图
- `docs/e2e/r-icon-desktop.png`（应用抽屉，1080×2340）
- `docs/e2e/r-icon-crop.png`（同图 3× 裁切放大，600×600）
  像素统计：`#2d7bc1` 占 32.4%（蓝色圆角底），`#fbfdfe` 占 6.8%（白色音符），与默认安卓机器人图标完全不同。

---

## ④ R-3 椒盐 8 条设计语言自检表

> 旧截图取自 `docs/e2e/f-*.png`（上一轮交付的实机截图）与本轮首改前的 `r-before-01-download.png`；
> 新截图取自本轮 `r-p*.png`（竖屏）/ `r-l*.png`（横屏）。

| # | 椒盐设计语言 | 改造前（旧截图） | 改造后（新截图） | 具体改了什么 |
|---|---|---|---|---|
| 1 | 沉浸式背景：整屏一层连续柔和渐变，不是白板贴卡片 | `f-03-download-filled.png`：纯 `#F8FAFD` 平板底 | `r-p1-download.png`：顶部 `#ecf3f9` → 底部 `#f4f5f7` 的连续竖向渐变；播放时由封面主色晕染 `r-p6-player.png` | 新增 `ui/theme/NarGradient.kt`：`paletteFromCover()`（缩到 32×32 求平均色 + 最大饱和度色，亮度归一化到 0.46–0.72），`immersiveBrush()` 用 4 段竖向渐变把主色稀释到 9%/3.5%/1.2%；`NarMusicApp` 根节点改为 `NarImmersiveBackground`，`Scaffold` 换成透明 `Surface`+`Box`，不再有整块不透明底色 |
| 2 | 极低分割感：大留白 + 24dp 整块 surface，不逐模块描边 | `f-03`：`Surface(border = BorderStroke(1.dp, outlineVariant))` 每个模块各套一个描边卡 | `r-p4-settings.png`：`下载设置 / 外观 / 存储 / 关于` 四块 24dp 大圆角 surface，块内 0.6dp 极细分隔 | 新增 `NarSurface`（`RoundedCornerShape(24.dp)`、`surface.copy(alpha=0.86f)`、无 border）、`NarGroup`、`NarHairline`（0.6dp，`onSurfaceVariant` alpha 0.10）；四个页面与播放器全部改用它，删掉所有 `BorderStroke` |
| 3 | 无边框输入：填充式容器，无描边 | `f-03`：两个 `OutlinedTextField`（蓝灰描边方框，最"拼接"的一块） | `r-p1-download.png`：圆角 16dp 填充块（`onSurface` alpha 0.075），无描边；搜索输入同样处理 | 新增 `NarFillField`（`BasicTextField` + 圆角 16dp fill 容器 + 占位文字叠层，支持 leading/trailing/多行/键盘回调）。`grep OutlinedTextField app/src/main/java/com/narnat/music/ui/` → **0 处** |
| 4 | 字体层级：标题 28sp SemiBold / 行主 17sp Medium / 行副 13sp alpha0.7 / 数字 tabular | `f-03`：标题 22sp，行文本 15sp/12sp，无 tabular | `r-p1-download.png`「音乐提取」28sp；`r-p3-library.png` 行标题 17sp、副标题 13sp；状态数字 `1.7 MB / 0 B/s / 14.7秒` 用 `fontFeatureSettings="tnum"` | 新增 `NarSectionTitle`(28sp SemiBold)、`NarRow`(72dp 行高、17sp+13sp)、`NarCaptionText`(13sp alpha .7)、`NarValueText`+`NarTabularNumber`（tnum） |
| 5 | 色彩：主色只用于强调，大面积中性色 | `f-03`：`primaryContainer` 大面积浅蓝块（`secondaryContainer` 分段控件、`primaryContainer` 徽标、28dp 圆形序号底） | `r-p1/r-p3/r-p4`：大面积全是中性白/灰，只有主按钮、选中态、进度条是 `#2D7BC1` | 徽标改为 `foreground.copy(alpha=0.10f)` 底 + 同色文字（`BatchBadge`）；行首序号底 `primaryContainer` → `primary.copy(alpha=0.16f)` 方缩略图；`FilterChip` → 纯文字筛选；`secondaryContainer` 分段控件 → 下划线式 tab |
| 6 | 按钮：主操作胶囊 + 一个视觉重心；次操作纯文字 | `f-03-download-filled.png`：两个并排胶囊（实心 `开始下载` + 浅灰 `保存到本地`） | `r-p1-download.png`：单个全宽胶囊 `开始下载`，`保存到本地` 变成居中的纯文字按钮 | 新增 `NarPillButton`（`CircleShape`、52dp 高、`contentPadding` 28/14dp）与 `NarTextAction`；下载页/批量页/设置页的"双胶囊"全部拆除（批量页 `解析歌单` 胶囊 + `清空` 文字，设置页 `保存设置` 胶囊 + `恢复默认` 文字） |
| 7 | 底部导航：无分割线、图标+小字、选中态用主色（不是大白胶囊） | `f-02-bottombar-icons.png` / `r-before-01-download.png`：选中项有 `primaryContainer` 大胶囊（截图底部那块 22×300px 的浅蓝圆角块） | `r-p1-download.png` 底部：4 个图标 + 11sp 小字，选中"下载"是 `#2d7bc1` 图标与文字，**无任何指示底** | `NarBottomBar`：`containerColor = Color.Transparent`、`tonalElevation = 0.dp`、`indicatorColor = Color.Transparent`、图标 24dp + 文字 11sp。`grep "NavigationBarItemDefaults.colors(" -A1` → `indicatorColor = Color.Transparent`（2 处，`NarBottomBar.kt:57` 与 `:90`） |
| 8 | 播放器：全屏沉浸，标题/歌词/细进度条/一排图标；不再用 bottom sheet | `f-07-player.png`：`ModalBottomSheet` 半屏卡片，封面 140dp，歌词塞在 220dp 高的灰块里 | `r-p6-player.png`（竖屏）/ `r-l5-player.png`（横屏双栏）：整屏由封面主色晕染，封面按 `min(宽-96dp, 高×0.38)` 自适应，歌名 24sp/歌手 14sp 左对齐，歌词居中滚动且当前行 17sp Bold 高亮，底部细进度条 + 左右时间 + 上一首/56dp 实心圆播放/下一首 | 新文件 `ui/player/PlayerScreen.kt`（删除 `PlayerSheet.kt`）：`ModalBottomSheet` → 全屏 `Surface` + `BoxWithConstraints`；新增 `rememberCoverBytes()` 复用封面字节做取色；`NarMusicApp` 用 `if (playerState.visible)` 覆盖在最上层并配 `BackHandler` |

### 列表行（第 9 条，补充实现）
`r-p3-library.png`：行首 48dp 圆角 12dp 方形缩略图（主色 alpha 0.16 底 + 序号），主文本 17sp Medium、副文本 13sp alpha .7、行尾 播放/删除/更多 三个图标，行间无边框只有整块 surface 内的留白。旧截图 `f-06-library-filled.png` 是 28dp 的圆形序号徽标 + 无缩略图。

### 横屏库页双列
`r-l3-library.png`：`LazyVerticalGrid(GridCells.Adaptive(minSize = 360.dp))`，1080×2340 旋转后 2340/360dp 可排 2 列（实测内容区 2120px ≈ 771dp → 2 列）。

---

## ⑤ R-4 横竖屏适配

**判定方式**（不引依赖，自己判定）：
```kotlin
BoxWithConstraints {
    val wide = maxWidth >= 600.dp ||
        configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
}
```

| 位置 | 竖屏 | 横屏（`user_rotation=1`） |
|---|---|---|
| 导航 | 底部 `NavigationBar`（透明底，无胶囊指示） | 左侧 `NavigationRail`，宽 **80dp**（`r-l1/l2/l3/l4.png` 左侧竖排 4 项） |
| 顶栏 | 品牌小字 + 搜索胶囊（库页隐藏搜索，避免与库页内搜索重复） | 隐藏（`rail` 已承担导航；避免横向挤占） |
| 内容宽度 | 全宽（20dp 边距） | 居中且 `widthIn(max = 640.dp)` |
| 播放器 | 单列（封面 → 标题 → 歌词 → 进度 → 控制） | 左右双栏（左：封面+标题+进度+控制；右：歌词），`r-l5-player.png` |
| 库页 | 单列 `LazyColumn` | `LazyVerticalGrid`，`minSize = 360.dp` |

**`configChanges` 选择**：`AndroidManifest.xml` 的 `<activity>` 增加
```xml
android:configChanges="orientation|screenSize|screenLayout|keyboardHidden|uiMode"
```
即选择"旋转时不重建 Activity"这一条（任务书允许二选一）：好处是快照里的 `tab`、各 Controller 的 `StateFlow`、`ExoPlayer` 实例都不丢；代价是 Compose 侧必须自己响应尺寸变化 —— 由 `BoxWithConstraints` 的 `maxWidth` 与 `LocalConfiguration.orientation` 共同驱动，实测旋转后页面/播放状态都保留（`r-l5-player.png` 是从竖屏播放中直接旋转过去的，进度仍在 00:06）。

---

## ⑥ R-5 截图清单（每张都有生成命令与 `wm size`）

`wm size` 恒为 `Physical size: 1080x2340`；横屏通过 `settings put system accelerometer_rotation 0` + `settings put system user_rotation 1` 切换，截图实际尺寸 2340×1080。

### 竖屏（6 张，1080×2340）
| 文件 | 内容 | 生成命令 |
|---|---|---|
| `r-p1-download.png` | 下载页（浅色，含完成态状态卡） | `python run_all.py portrait` |
| `r-p2-batch.png` | 批量页（粘贴歌单空态） | 同上（`nav 批量下载` 后 `shot_clean`） |
| `r-p3-library.png` | 库页（3 首真实数据） | 同上（`nav 库`） |
| `r-p4-settings.png` | 设置页 | 同上（`nav 设置`） |
| `r-p6-player.png` | 全屏播放器（竖屏单列） | 同上（`nav 库` + `tap_desc 播放`） |
| `r-p5-dark-settings.png` | 深色模式（设置页） | `python run_all.py dark-portrait` |

### 横屏（6 张，2340×1080）
| 文件 | 内容 | 生成命令 |
|---|---|---|
| `r-l1-download.png` | 下载页（左侧 NavigationRail） | `python run_all.py light-land` |
| `r-l2-batch.png` | 批量页 | 同上 |
| `r-l3-library.png` | 库页（双列网格） | 同上 |
| `r-l4-settings.png` | 设置页（限宽 640dp 居中，上滑后"保存设置"完整可见） | 同上 |
| `r-l5-player.png` | 播放器双栏 | 同上 |
| `r-l6-dark-library.png` | 深色模式横屏库页 | `python run_all.py dark-land`（`rland.py land` 后） |

辅助脚本：`docs/e2e/rutil.py`（adb 封装 + 健壮 `uiautomator dump`）、`rshot.py`、`rland.py`、`run_all.py`、`cropview.py`/`crop2.py`/`cropxy.py`/`px.py`（把 png 转成结构化文本视图供自查）。

---

## ⑦ 两轮迭代记录

> 说明：本环境里的 `Read` 工具对二进制 png 直接拒绝（`检测到二进制文件（含NUL字节）`），
> 因此"打开 png 自己看"是通过 `docs/e2e/cropview.py` / `crop2.py` / `cropxy.py` / `px.py`
> 把 png 转成**分区 ASCII 差分图 + 区域均值色 + 逐行像素扫描**后阅读实现的，
> 结论都基于这些可复现的文本视图（脚本与输出都留在 `docs/e2e/`）。

### 第一轮（首版 → 发现 5 个问题）
首版截图（文件名与最终版相同，已被后续覆盖重截；当时的量化记录保留在本文）：`r-p1-download.png` 的 `band00 = #bfd6ea`、`r-p3-library.png` 的 `band00 = #d0e0ed`。

发现：
1. **顶部渐变过蓝过重**：`band00 #bfd6ea`，整屏像"蓝色蒙版"，与椒盐 `#f3f3f3` 的克制不符。
   根因：`immersiveBrush` 顶部用了 `wash(tint, White, 0.26f)`（26% 主色）。
2. **输入框几乎不可见**：`NarFillField` 填充 `onSurface` alpha 0.045 在浅色渐变上分辨不出，等于又回到"看不出是输入框"。
3. **库页搜索框与顶栏搜索重复**：竖屏同一屏出现两个搜索框（`模糊搜索歌曲、歌手...` + `搜索歌曲名...`）。
4. **底部导航压到系统手势条**：`crop2` 扫描到 label 底部 `y=2299` 与系统 home 手势区（`y>2274`）重叠；`px.py` 显示 `y=2160/2200` 处是 `#444444`、`y≥2240` 是 `#000000`（输入法/系统栏叠层）。
5. **行首缩略图对比度偏低**：`primary alpha 0.12` 在 `#fdfefe` 上几乎看不出色块。

### 第二轮（改动 → 复检）
改动：
1. `immersiveBrush` 浅色：`0.26 → 0.13`，并在第三轮进一步 `0.13 → 0.09`（顶部最终 `#ecf3f9`，与底部 `#f4f5f7` 差 8/255）。
2. `NarFillField` 填充 alpha：`0.045 → 0.075`（深色 `0.08 → 0.11`）。
3. `NarTopBar` 增加 `showSearch`：库页不显示顶栏搜索，只保留页面内"搜索歌曲名..."，两个文案都还在但不再同屏重复。
4. `NarBottomBar` 去掉 `windowInsets = WindowInsets(0,0,0,0)`，恢复 `NavigationBar` 默认 inset；`PlayerScreen` 的两栏/单栏容器加 `navigationBarsPadding()`。
5. `NarCoverThumb` 底色 alpha `0.12 → 0.16`。

复检（`crop2.py r-p1-download.png 0.86 1.0`）：
- 底部 `y+0..324` 全部是 `#f4f5f7`/`#fdfdfe`，手势条只剩中间 `#888889` 那一条；label 不再落在手势区。
- 选中态"下载"图标为 `#2d7bc1`，无胶囊底。
- 缩略图 `r-p3-library.png` 行首 48dp 方块可辨（`cropview` part2 的 `........` 块）。

### 第三轮（触摸交互 bug，与视觉无关但必须记录）
复检时发现"点底部导航没反应"，逐项排查后定位到两个**非 UI 代码**原因（均已修复，属 `ui/` 内改动）：
1. `NarTheme` 的 content 里曾并列三个 root 兄弟（`NarImmersiveBackground` + `AnimatedVisibility(PlayerScreen)` + `BackHandler`），Compose 根布局把它们叠在 (0,0)，触摸命中有歧义；
   → 改为**单一 root `Box`**：`NarImmersiveBackground` / `NarToastHost` / `PlayerScreen` 依次作为它的子节点，去掉 `AnimatedVisibility`。
2. 自动化侧：`ADBKeyboard` 在"无 UI"状态下 `mInputShown=true` 仍会吞掉底部区域点击，截图前需先 `keyevent 4` 收起（脚本 `rshot.hide_ime()` 已按 `dumpsys input_method` 判断后再发）。
修复后 `nav()` 连续切换 4 个 tab 全部成功（`run_all.py` 一次跑完 12 张截图）。

---

## ⑧ 验收标准逐条核对

| # | 验收项 | 结果 |
|---|---|---|
| 1 | `:app:testDebugUnitTest` 全绿且 `tests >= 111` | **125 个测试，0 失败**（原 91 = 81 + LinkPlanTest 10，新增 34：`NarPaletteTest` 9 + `NarSurfacesTest` 10 + `RedesignScreensTest` 15） |
| 2 | R-1 三条路径 | 全部通过，`ilst` 原子与 aid/bvid 互证见 §② |
| 3 | `aapt2 dump badging` 图标非空 + 桌面截图 | 见 §③（`application-icon-160..65534` 均非空，桌面裁切图 `r-icon-crop.png`） |
| 4 | 椒盐 8 条自检表 | 见 §④（8 条 + 列表行补充，均有改造前/后截图与具体改动） |
| 5 | `OutlinedTextField` 出现次数 = 0 | `Grep "OutlinedTextField" app/src/main/java/com/narnat/music/ui` → **无匹配** |
| 6 | `indicatorColor` 必须 `Color.Transparent`/极低 alpha | `NarBottomBar.kt:57` 与 `:90` 均为 `indicatorColor = Color.Transparent` |
| 7 | 横竖屏各 ≥6 张，文件名 `r-*`，含 `wm size`，横屏可见 NavigationRail 且无裁切 | 见 §⑥（竖 6 + 横 6；横屏 `r-l1..l4` 左侧 80dp rail 清晰可见；`r-l4-settings.png` 内容限宽 640dp 居中、无裁切） |
| 8 | `docs/redesign-report.md` 含根因/修复清单/图标/自检表/横竖屏/两轮迭代/未覆盖项 | 本文件 |

---

## ⑨ 改动清单

**新增**
```
app/src/main/java/com/narnat/music/ui/theme/NarGradient.kt
app/src/main/java/com/narnat/music/ui/theme/NarSurfaces.kt
app/src/main/java/com/narnat/music/ui/player/PlayerScreen.kt
app/src/main/res/drawable/ic_launcher_foreground.xml
app/src/main/res/drawable/ic_launcher_background.xml
app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml
app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml
app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png
app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher_round.png
app/src/test/java/com/narnat/music/ui/NarPaletteTest.kt
app/src/test/java/com/narnat/music/ui/NarSurfacesTest.kt
app/src/test/java/com/narnat/music/ui/RedesignScreensTest.kt
```
**修改**
```
app/src/main/AndroidManifest.xml                    （icon/roundIcon + configChanges）
app/src/main/java/com/narnat/music/ui/NarMusicApp.kt      （单 root、rail/底栏分支、沉浸背景、全屏播放器）
app/src/main/java/com/narnat/music/ui/components/NarBottomBar.kt   （无胶囊 + 新增 NavigationRail）
app/src/main/java/com/narnat/music/ui/components/NarTopBar.kt      （轻量顶栏 + 填充搜索 + showSearch）
app/src/main/java/com/narnat/music/ui/screens/download/DownloadScreen.kt
app/src/main/java/com/narnat/music/ui/screens/batch/BatchScreen.kt
app/src/main/java/com/narnat/music/ui/screens/batch/BatchParts.kt
app/src/main/java/com/narnat/music/ui/screens/library/LibraryScreen.kt
app/src/main/java/com/narnat/music/ui/screens/settings/SettingsScreen.kt
dist/NarMusic-v1.0.0.apk                            （用新源码重新构建的 release）
```
**删除**
```
app/src/main/java/com/narnat/music/ui/player/PlayerSheet.kt   （被全屏 PlayerScreen 取代）
```
**未触碰**：`core/**`、`data/**`、`di/**`、`domain/**`、`docs/task-*.md`、`docs/*.md`（除本报告）、`NarMusic/**`、`SaltUI/**`；`app/build.gradle.kts` 未改（无新依赖）。

---

## ⑩ 未覆盖项 / 待裁决

1. **设置页的 SaltUI 仅剩 `NarTheme` 包装**：`SettingsScreen` 已不再使用 `Item*/RoundedColumn`，但 `ui/theme/NarTheme.kt` 仍包着 `SaltTheme`（`SaltSpike*.kt` 还在用）。按"禁止顺手重构"的边界，本次没有删除 Salt 依赖；是否彻底移除 salt-ui 需要产品决策。
2. **横屏顶栏被隐藏**：横屏只有 `NavigationRail`，没有品牌/全局搜索入口。搜索只能在库页内进行。这是为"内容区不被横向挤压"做的取舍，若需要横屏也带顶栏可以再调。
3. **状态栏 inset 未显式处理**：`Theme.NarMusic` 非 edge-to-edge，Android 14 上内容自动避开状态栏（截图中 logo 位于 `y=75`，状态栏高约 66px）。若后续改为 edge-to-edge，需要补 `statusBarsPadding()`。
4. **`wm size` 不是横屏后的返回值**：`adb shell wm size` 始终返回 `Physical size: 1080x2340`，横屏只能通过截图尺寸（2340×1080）与 `settings get system user_rotation`（=1）证明。任务书要求的"用 `wm size` 确认 2340x1080"在当前模拟器上无法直接满足，已用截图尺寸 + rotation 值替代。
5. **深色模式下的封面取色**：`paletteFromCover` 只在播放时生效（用当前播放音频的内嵌封面）。下载完成后不预览封面，因此下载页背景永远是主题蓝的淡晕染，不是"随最近一首歌变化"。
6. **批量页横屏截图是空态**：未在横屏下跑一次完整歌单解析（解析依赖外网 B 站搜索，耗时且易受风控）。设计层面（无边框输入、下划线 tab、结果摘要、行高 72dp）已在竖屏 `r-p2-batch.png` 与组件测试中覆盖。
7. **`Read` 工具无法直接读 png**：任务书要求"用 Read 打开 png 自己看"，本环境的 Read 对二进制文件一律拒绝，已改用结构化文本视图（分区 ASCII + 区域色 + 逐行像素扫描）自查，脚本全部保留在 `docs/e2e/`，可复现。
