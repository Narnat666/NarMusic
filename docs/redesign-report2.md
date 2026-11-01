# 任务 R2：界面缺陷精修 —— 完成报告

日期：2026-11-01　改动范围：`app/src/main/java/com/narnat/music/ui/**`、`app/src/test/java/com/narnat/music/ui/**`、`docs/e2e/r2_*`、`docs/redesign-report2.md`（`core/`、`data/`、`di/`、`domain/`、`MainActivity.kt`、`res/**` 均未改动）
模拟器：AVD `nartest`，1080×2340 / 440dpi（密度 2.75），Android 14 (sdk 34)

---

## 0. 基础数据（先摆事实）

### 0.1 构建与测试

```
cmd /c "set TMP=E:\android-user\tmp&& set TEMP=E:\android-user\tmp&& set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:assembleDebug :app:testDebugUnitTest --console=plain"
BUILD SUCCESSFUL in 59s
python docs/e2e/count_tests.py  ->  TOTAL tests = 129 failures = 0      (要求 >= 125)
```

新增 4 个测试：`NarTopBarTest`（3 个：默认收起 / 点击展开 / showSearch=false 时不显示图标）+ `LibraryScreenTest.rowsShowCoverPlaceholderInsteadOfIndexLabel`。

所有截图均来自包含全部改动的 debug 构建；最后一次改动（`NarCoverCache` 的访问加 `synchronized`，行为等价）之后重新 `install -r` 并复验库页三行封面仍正常渲染（见 D-4 的第一组像素证据）。

文案命中率（未下降）：

```
python docs/verify_copy.py
rows 192
hit 188 miss 4        # 与任务书给出的 188/192 完全一致
```

### 0.2 状态栏 / 手势条像素高度（判定基准，可复现）

```
adb shell dumpsys window displays
ROTATION_0={nonDecorInsets=[0,136][0,66], configInsets=[0,136][0,66], nonDecorFrame=[0,136][1080,2274], ...}
ROTATION_90={nonDecorInsets=[136,0][0,66], configInsets=[136,66][0,66], nonDecorFrame=[136,0][2340,1014], ...}
InsetsSource id=18560000 type=statusBars frame=[0,0][1080,136] visible=true
InsetsSource id=7bb30001 type=navigationBars frame=[0,2274][1080,2340] visible=true
```

- 竖屏：状态栏 **136px**（≈49dp@440dpi），手势条 **66px**，内容区 `[0,136]–[1080,2274]`
- 横屏（ROTATION_90）：状态栏 **66px**，手势条 **66px**，内容区底边 **1014px**（= 1080 − 66）
- 判定：竖屏 App 文本 `top >= 136`；横屏 `top >= 66`，播放器控制区 `bottom <= 1014`

### 0.3 本轮新增的自查工具（都在 `docs/e2e/`）

| 文件 | 作用 |
|---|---|
| `r2_probe.py` | ①矩形区域平均色/标准差 ②扫描线上深色像素连续段 ③两区域间是否存在分隔线（相邻行色差） |
| `r2_verify.py` | 批量断言所有 dump 的「App 文本 top >= 状态栏高度」，输出违规数 |
| `r2_e2e.py` / `r2_run.py` / `r2_shots.py` | 装包、转屏、点击、输入、截图、dump 的驱动脚本（转屏后会校验截图分辨率与横竖屏一致，防止拍错方向） |

---

## 1. D-1 ~ D-7 逐项结果

### D-1【严重】状态栏文字压在 App 内容上 ✅

**改了什么**

| 文件 | 改动 |
|---|---|
| `ui/components/NarTopBar.kt` | 外层 Row 加 `.windowInsetsPadding(WindowInsets.statusBars)`（顶栏 padding 由 12dp→8dp 收口） |
| `ui/components/NarBottomBar.kt` | `NavigationRail` 显式 `windowInsets = WindowInsets(0,0,0,0)` + `.windowInsetsPadding(WindowInsets.statusBars)` + `.padding(top = 8.dp)`（M3 的 Rail 默认已含纵向 inset，这里改成我们自己消费，避免重复计算；+8dp 解决「Rail 整体太靠上」） |
| `ui/NarMusicApp.kt` | 横屏内容列 `Column` 加 `.windowInsetsPadding(WindowInsets.statusBars)`（横屏没有顶栏，标题也必须在状态栏下方） |
| `ui/player/PlayerScreen.kt` | `PlayerHeader`（返回箭头 + 正在播放）加 `.statusBarsPadding()`；横屏歌词域 `LyricsPane` 加 `.statusBarsPadding()`（否则滚动歌词会画到状态栏下面） |
| `MainActivity.kt` | **未改**，仍保持 `enableEdgeToEdge()`；采用任务书「方案 1：所有顶栏/根容器消费 inset」，无需退回 `setDecorFitsSystemWindows(true)` |

底部导航：`NavigationBar` 自带 systemBars(底) inset，竖屏手势条 66px 一直正确；横屏用 Rail（底部 inset 不参与内容排布，已用 dumps row 验证内容未越界）。

**怎么验证 / 证据**

1) `python docs/e2e/r2_verify.py` → **违规节点数 = 0**（完整输出见 `docs/e2e/r2-verify.txt`），逐条 bounds 原文节选：

```
== r2-p1-download.xml  size=1080x2340  竖屏  状态栏上限=136
   OK  desc='纳儿音乐'  bounds=[55,183][116,244] top=183
   OK  text='纳儿音乐'  bounds=[138,180][302,246] top=180
   OK  desc='搜索'      bounds=[965,186][1020,241] top=186
   OK  text='音乐提取'  bounds=[66,329][374,440] top=329
   OK  text='下载'      bounds=[96,2183][156,2233] top=2183
== r2-p5-player.xml  size=1080x2340  竖屏  状态栏上限=136
   OK  desc='收起播放器' bounds=[85,188][157,260] top=188
   OK  text='正在播放'   bounds=[470,191][610,257] top=191
== r2-l1-download.xml  size=2340x1080  横屏  状态栏上限=66
   OK  text='下载'      bounds=[80,198][140,248] top=198
   OK  text='批量下载'   bounds=[50,369][170,419] top=369
   OK  text='库'        bounds=[95,540][125,590] top=540
   OK  text='设置'      bounds=[80,711][140,761] top=711
   OK  text='音乐提取'   bounds=[466,110][774,221] top=110
== r2-l4-settings.xml  size=2340x1080  横屏  状态栏上限=66
   OK  text='设置'(Rail) bounds=[80,711][140,761] top=711
   OK  text='设置'(标题) bounds=[466,110][620,221] top=110
== r2-l5-player.xml  size=2340x1080  横屏  状态栏上限=66
   OK  desc='收起播放器' bounds=[107,118][179,190] top=118
   OK  text='正在播放'   bounds=[531,121][671,187] top=121
```

2) 像素 A/B（同一台机器、同一页面，旧图 `r-p1-download.png` vs 新图 `r2-p1-download.png`）：

```
# 旧图：状态栏带内 y=80/90/100/110/120/130 都能扫到 App 自己的 logo 深色像素簇（x≈56..114）
y= 80 runs=17 → 首个 run x=[69,101]      # 系统时钟 + App logo 同时出现在状态栏带内
y= 90 runs=1  → run x=[59,111]
y=100 runs=2  → run x=[56,84] [89,114]
y=120 runs=1  → run x=[59,111]

# 新图：同一批扫描线在 App 区域（x<160）已无任何深色像素，只剩系统状态栏图标
y= 80 runs=15 → 全部落在 x>=166（时钟）与 x>=875（信号/电池）
y= 90 runs=0
y=100 runs=0
y=110 runs=0
y=120 runs=0
y=130 runs=0
```

命令：`python docs/e2e/r2_probe.py runs <png> <y> 0 1079`

3) 整屏状态栏区域统计（y∈[0,80]）：

```
r-p1-download.png   region=(0,0)-(1080,80) meanLuma=232.8 stdLuma=22.4   # 旧：混入 App logo
r2-p1-download.png  region=(0,0)-(1080,80) meanLuma=238.6 stdLuma=21.2
```
（该区域内剩余的深色簇全部落在 x≥166 的时钟与 x≥875 的信号/电池图标位置，已用 runs 逐条列出，无 App 内容）

---

### D-2【严重】横屏播放器底部控制条被裁出屏幕 ✅

**改了什么**（`ui/player/PlayerScreen.kt`）

横屏左栏由「固定高度堆叠 + Arrangement.Center」改为「垂直权重分配」：

```kotlin
Column(modifier = Modifier.weight(1f).fillMaxHeight()) {
    PlayerHeader(onDismiss = onDismiss)                       // 固定，含 statusBarsPadding
    BoxWithConstraints(                                       // 权重域：封面只吃剩余空间
        modifier = Modifier.weight(1f).fillMaxWidth(),
        contentAlignment = Alignment.Center
    ) {
        val side = minOf(maxWidth, maxHeight, 260.dp)          // 高度不足时自动缩小
        PlayerArtwork(coverBytes, dark, Modifier.size(side))
    }
    TitleBlock(...)                                            // 固定
    Spacer(10.dp); ProgressBlock(...)                          // 进度条 + 时间
    Spacer(4.dp);  ControlRow(...)                             // 上一首/播放/下一首
}
```
外层保留 `.navigationBarsPadding()`；`PlayerArtwork` 的封面 `AsyncImage` 补 `contentDescription = "专辑封面"`（D-2 验收要求能取到 image 节点，同时是无障碍改进）；播放/暂停按钮用 `Modifier.semantics { contentDescription = if (isPlaying) "暂停" else "播放" }` 保证任何播放状态下都有 desc。

**怎么验证 / 证据**（`r2-l5-player.xml`，横屏 2340×1080，手势条 66px → 底限 1014px）

```
''      desc='收起播放器' cls=android.view.View      bounds=[107,118][179,190]
'正在播放' text          cls=android.widget.TextView  bounds=[531,121][671,187]
''      desc='专辑封面'  cls=android.widget.ImageView bounds=[484,209][719,444]   # 封面节点
'回家吧' text            cls=android.widget.TextView  bounds=[88,444][286,540]    # 歌名
'周依然、于适' text       cls=android.widget.TextView  bounds=[88,540][316,606]    # 歌手
'00:09' text            cls=android.widget.TextView  bounds=[88,766][173,805]    # 时间（当前）
'02:55' text            cls=android.widget.TextView  bounds=[1030,766][1115,805] # 时间（总长）
''      desc='上一首'    cls=android.view.View        bounds=[335,858][418,941]    # 控制 1
''      desc='暂停'      cls=android.widget.Button      bounds=[519,816][684,981]    # 控制 2
''      desc='下一首'    cls=android.view.View        bounds=[786,858][869,941]    # 控制 3
```

`max(bottom) = 981 <= 1014` ✅；四类节点（image / 标题 / 时间 / content-desc 按钮）全部存在。

补充：修改前（任务书 `r-l5-player.png` 证据）时间「00:03 / 04:19」与三个控制键完全被裁出屏幕；现在四类节点全部在 1014px 以内。

---

### D-3【中】横屏库页列表碎片化 ✅（竖屏行高一并调成 72dp）

**改了什么**（`ui/screens/library/LibraryScreen.kt`）

- 删除横屏专用的 `LazyVerticalGrid` + `GridRowCard`（每行独立圆角卡片），横竖屏统一为「**单个 24dp 圆角 surface 包住全部行**」：
  ```kotlin
  else -> NarSurface(                                  // NarBlockShape = 24dp 圆角，整块
      modifier = Modifier.weight(1f),
      color = colorScheme.surface.copy(alpha = 0.86f),
      contentPadding = PaddingValues(vertical = 4.dp)
  ) {
      LazyColumn(modifier = Modifier.weight(1f)) {
          itemsIndexed(state.items, key = { _, item -> item.id }) { index, item ->
              if (index > 0) {
                  NarHairline(color = colorScheme.outlineVariant.copy(alpha = 0.5f))  // 行间 1px 极浅分隔线
              }
              LibraryRow(...)
          }
      }
  }
  ```
- `LibraryRow`：`.heightIn(min = 72.dp)` + 上下 12dp 内边距 + 48dp 封面 → 行高 72dp（旧值 10dp 内边距、行高偏大留白多）。
- 顺带 `wide` 变量与 `GridItemSpan` 等无用 import 一并清掉（因本次改动而失效）。

**怎么验证 / 证据**

竖屏 `r2-p3-library.png`（3 行，行首 y=893 / 1093 / 1293 → 行距 200px = 72.7dp）：

```
python docs/e2e/r2_probe.py divider r2-p3-library.png 1052 1066 300 900
  y=1057 luma=254.00
  y=1058 luma=242.93
  y=1059 luma=242.93
  y=1060 luma=254.00
  maxDelta=11.07 between y=1057 and y=1058 -> PASS(存在分隔线, 相邻行色差>3)
python docs/e2e/r2_probe.py divider r2-p3-library.png 1252 1266 300 900
  maxDelta=11.00 between y=1257 and y=1258 -> PASS(存在分隔线, 相邻行色差>3)
python docs/e2e/r2_probe.py region r2-p3-library.png 300 1020 900 1052
  region=(300,1020)-(900,1052) avgRGB=(254.0,254.0,254.0) meanLuma=254.0 stdLuma=0.0 darkRatio=0.0000
  ↑ 两行之间是 surface 纯白（254,254,254），不是背景色 → 没有「背景色间隙」
```

横屏 `r2-l3-library.png`（行首 y=718 / 918 → 行距 200px）：

```
python docs/e2e/r2_probe.py divider r2-l3-library.png 876 892 700 1200
  y=882 luma=253.11
  y=883 luma=242.93
  y=884 luma=242.93
  y=885 luma=253.48
  maxDelta=10.55 between y=884 and y=885 -> PASS(存在分隔线, 相邻行色差>3)
python docs/e2e/r2_probe.py region r2-l3-library.png 700 860 1200 878
  region=(700,860)-(1200,878) avgRGB=(253.0,253.3,254.0) meanLuma=253.3 stdLuma=0.3 darkRatio=0.0000
```
（分隔线 0.6dp@2.75 = 1.65px，实测占 2 个设备像素行，色差 10~11）

---

### D-4【中】库页行首是「序号方块」而不是专辑封面 ✅

**改了什么**

| 文件 | 改动 |
|---|---|
| `ui/components/NarAlbumCover.kt`（新增） | 用 `MediaMetadataRetriever.embeddedPicture` 读内嵌封面，`produceState` + `remember` 缓存（按 `mediaUri` 作 key，`LruCache(48)` 命中 + `Set` 记录未命中，滚动不重复解码）；命中用 Coil `AsyncImage(ContentScale.Crop)`，未命中/读取失败降级为 `NarCoverThumb`（主色 16% 浅块）+ `NarIcons.LibraryMusic` 音符图标 |
| `ui/screens/library/LibraryScreen.kt` | 行首 `NarCoverThumb(label = id)` → `NarAlbumCover(source = item.coverSource, size = 48.dp)`；**序号已彻底去掉**；`LibraryItem` 新增 `coverSource: String = ""`（UI 层数据类，`data/`、`domain/` 未动） |
| `ui/screens/library/LibraryController.kt` | `coverSource = entity.mediaUri.ifEmpty { entity.filePath }`（只读 entity 字段，未改数据层） |

**怎么验证 / 证据**

1) 有封面（`r2-p3-library.png`，行首 48dp 方块 x∈[100,230]，三行逐行统计）：
```
python docs/e2e/r2_probe.py region r2-p3-library.png 100 890  230 1020
  avgRGB=(168.3,167.9,165.8) meanLuma=167.8 stdLuma=76.0 darkRatio=0.3680   # 回家吧
python docs/e2e/r2_probe.py region r2-p3-library.png 100 1090 230 1220
  avgRGB=(91.0,87.0,109.0) meanLuma=90.7 stdLuma=69.4 darkRatio=0.7184      # Dive Back In Time
python docs/e2e/r2_probe.py region r2-p3-library.png 100 1290 230 1420
  avgRGB=(151.5,87.3,96.4) meanLuma=107.5 stdLuma=53.3 darkRatio=0.6566     # Counting Stars
  ↑ 高方差 + 高饱和 = 真实照片缩略图，而不是纯色块 / 序号
```
2) 无 `covr` 时降级（`r2-p9-library-nocover.png`：把该文件的 `covr` box 改写为 `free` 后重启 App）：
```
python docs/e2e/r2_probe.py region r2-p9-library-nocover.png 100 890 230 1020
  avgRGB=(216.0,230.5,243.0) meanLuma=227.6 stdLuma=16.8 darkRatio=0.0000   # 柔和主色浅块（音乐图标）
python docs/e2e/r2_probe.py region r2-p9-library-nocover.png 100 1090 230 1220
  avgRGB=(91.0,87.0,109.0) meanLuma=90.7 stdLuma=69.4                        # 另一行仍是真实封面
```
（同一张图里两种状态并存 → 证明是逐条异步读取 + 逐条降级，而不是全局开关）

3) 序号不再出现：`LibraryScreenTest.rowsShowCoverPlaceholderInsteadOfIndexLabel` 断言 `"01"/"02"` 节点不存在（单测通过）。

---

### D-5【中】下载页搜索框抢戏，标题被压低 ✅

**改了什么**（`ui/components/NarTopBar.kt`）

- 顶栏默认只渲染「logo + 纳儿音乐 + 右侧 40dp 圆形搜索按钮」（`NarCircleIconButton`，直径 40dp，`contentDescription = "搜索"`）。
- 点击图标后 `searchExpanded = true` 才内联展开 `NarFillField`（placeholder 仍是「模糊搜索歌曲、歌手...」），展开后尾部给「收起搜索」×（清空态）/「清除」×（有输入），保证能收回去（**这是本项唯一超出字面要求的补充，见 §3 披露**，因为 `material-icons-core` 没有 `Rounded.Close`，用 `Rounded.Clear` 图形 + 不同 desc）。

**怎么验证 / 证据**

默认态（`r2-p1-download.xml`，顶栏区域 y<310 的全部节点）：
```
''          desc='纳儿音乐'  cls=android.view.View      bounds=[55,183][116,244]
'纳儿音乐'   text          cls=android.widget.TextView  bounds=[138,180][302,246]
''          desc='搜索'      cls=android.view.View      bounds=[965,186][1020,241]   # 40dp 圆钮，无输入框
# 顶栏没有 EditText 节点
```
点击后（`r2-p7-search-expanded.xml`）：
```
''  desc='纳儿音乐'  cls=android.view.View      bounds=[55,200][116,261]
'纳儿音乐' text      cls=android.widget.TextView  bounds=[138,197][302,263]
''  desc='搜索'      cls=android.widget.ImageView bounds=[379,205][429,255]        # 变成输入框内的前置图标
''  (空)            cls=android.widget.EditText  bounds=[462,164][912,296]        # 搜索输入框出现
''  desc='收起搜索'  cls=android.widget.ImageView bounds=[912,164][1044,296]
```
placeholder 渲染的像素证据（同一扫描线 y=230，x∈[440,910]）：
```
r2-p7-search-expanded.png --thresh=200 → runs=25 darkPixels=119   （x=467..811 有字形）
r2-p1-download.png        --thresh=200 → runs=0  darkPixels=0     （默认态该处是空白）
```
文案与命中率：`模糊搜索歌曲、歌手...` 仍在源码中，`verify_copy.py` 命中也仍是 **188/192**（T008/T189/T190/T191 为历史 MISS，与本轮无关）。

---

### D-6【小】下载页状态卡在竖屏被底部导航裁切 ✅（**本轮未改代码，仅补齐验收证据**）

**核查结论**：`DownloadScreen` 上一轮已经是 `Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(NarPagePadding).padding(bottom = 24.dp))`，整页可滚、内容底部 24dp。缺陷现象来自「截图只拍了未滚动时的首屏」，不是不可达。**本轮没有为 D-6 改动任何代码**（见 §3 披露）。

**怎么验证 / 证据**（`r2-p6-download-bottom.png`：连划三次到底后截图 + dump）

```
'adb shell input swipe 540 1800 540 700 300' × 3   →   python docs/e2e/r2_run.py shot r2-p6-download-bottom.png

''       text='下载时间' cls=android.widget.TextView bounds=[110,1353][250,1419]
'15.8秒'  text         cls=android.widget.TextView bounds=[110,1430][274,1510]   # 值完整可见
''       text='状态'    cls=android.widget.TextView bounds=[573,1353][643,1419]
'已完成'  text         cls=android.widget.TextView bounds=[573,1430][738,1510]   # 值完整可见
''       text='文件大小' cls=android.widget.TextView bounds=[110,1845][250,1911]
'1.5 MB' text          cls=android.widget.TextView bounds=[841,1845][970,1911]
''       text='下载'    cls=android.widget.TextView bounds=[96,2183][156,2233]    # 底部导航
```
底部导航容器 top = **2054**（`r2-p1-download.xml` 中的 NavigationBar 节点 `[0,2054][1080,2274]`），所有状态卡节点 `bottom <= 1911 < 2054` ✅

---

### D-7【小】库页副标题末尾多余分隔点 ✅

**改了什么**（`ui/screens/library/LibraryController.kt`）

```kotlin
private fun subtitleOf(entity: MusicFileEntity): String {
    val sizeText = if (entity.fileSize > 0L) formatBytes(entity.fileSize) else "文件缺失"
    return listOf(entity.artist, entity.platform, sizeText)
        .filter { it.isNotBlank() }
        .joinToString(" · ")
}
```
（判据 8 要求的 `listOfNotNull` 或 `filter` + `joinToString(" · ")`：采用后者）

另外在行内 `Text` 上补了 `overflow = TextOverflow.Ellipsis`：实际观察到的「末尾一个 ·」有一半是 `maxLines = 1` 硬裁剪导致的（文本被切在「· 」后面），只改拼接逻辑仍然会露出半个分隔符。

**怎么验证 / 证据**

```
findstr /n "listOf( entity.artist" app\src\main\java\com\narnat\music\ui\screens\library\LibraryController.kt
218:        return listOf(entity.artist, entity.platform, sizeText)
219:            .filter { it.isNotBlank() }
220:            .joinToString(" · ")
```
真机 dump 文本（`r2-p3-library.xml`）：
```
'周依然、于适 · 酷狗音乐 · 1.5 MB'
'白鲨JAWS · 酷狗音乐 · 1.5 MB'
'OneRepublic · 酷狗音乐 · 1.7 MB'
```
无任何以「· 」结尾的副标题。

---

## 2. 截图清单（15 张，全部为 `r2-` 前缀新图）

竖屏 1080×2340（9 张）：

| 文件 | 内容 | 关键证据 |
|---|---|---|
| `r2-p1-download.png` | 下载页默认态（顶栏=logo+40dp 搜索钮，页面标题「音乐提取」为主体） | D-1 / D-5 |
| `r2-p2-batch.png` | 批量下载页 | 全屏 inset |
| `r2-p3-library.png` | 音乐库（3 行、整块 surface、行首真实封面、72dp 行高） | D-3 / D-4 / D-7 |
| `r2-p4-settings.png` | 设置页 | 全屏 inset |
| `r2-p5-player.png` | 竖屏播放器（封面 816px + 歌词 + 进度 + 三键） | D-1（收起播放器 top=188） |
| `r2-p6-download-bottom.png` | 下载页滚到底（状态卡完整） | D-6 |
| `r2-p7-search-expanded.png` | 顶栏搜索展开态 | D-5 |
| `r2-p8-dark-library.png` | 深色模式音乐库（背景 luma=60.2） | 深色模式 |
| `r2-p9-library-nocover.png` | 一首无 `covr` 的降级封面 + 一首正常封面 | D-4 降级 |

横屏 2340×1080（6 张）：

| 文件 | 内容 | 关键证据 |
|---|---|---|
| `r2-l1-download.png` | 横屏下载页（Rail 首项 top=198） | D-1 |
| `r2-l2-batch.png` | 横屏批量下载页 | 全屏 inset |
| `r2-l3-library.png` | 横屏音乐库（单块 surface + 1px 分隔线） | D-3 |
| `r2-l4-settings.png` | 横屏设置页 | D-1 |
| `r2-l5-player.png` | 横屏播放器（封面+歌名+时间+三键全在屏内） | D-2 |
| `r2-l6-dark-player.png` | 深色模式横屏播放器（背景 luma=25.2） | 深色模式 + 播放器 |

生成方式（脚本都在 `docs/e2e/`，可直接复跑）：

```bat
:: 四个 tab 页的批量截图 + dump（脚本内部会断言截图分辨率与横竖屏一致）
python docs/e2e/r2_shots.py r2-p        :: 竖屏 p1..p4
python docs/e2e/r2_shots.py r2-l        :: 横屏 l1..l4

:: 单张截图 / dump
python docs/e2e/r2_run.py shot r2-p5-player.png
python docs/e2e/r2_run.py dump r2-p5-player.xml

:: 转屏
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1     :: 横屏；0 = 竖屏
```

每张图都配同名 `.xml`（`uiautomator dump` 原文）作为证据文件。

---

## 3. 主动披露（需要产品决策 / 与任务书不完全一致的点）

1. **D-6 没有代码改动**。核查后确认 `DownloadScreen` 已经满足「整页可滚 + 底部 24dp」，缺陷现象是「首屏截图」造成的观感。我补了滚到底的截图与 bounds 证据，但没有为了「有 diff」而改代码。若你希望首屏就不被裁（例如折叠输入卡），那是新的产品决策，不在本轮范围。
2. **D-5 额外加了「收起搜索」× 按钮**。任务书只说「点击图标展开」，但展开后如果没有收起的入口，顶栏就永久被输入框占据。收起按钮用 `Icons.Rounded.Clear`（`material-icons-core` 无 `Rounded.Close`），desc 为「收起搜索」，与有输入时的「清除」区分。
3. **D-4 给 `LibraryItem` 增加了 `coverSource` 字段**（UI 层数据类，位于 `ui/screens/library/`）。`data/`、`domain/`、`di/`、`core/` 一行未改，封面读取完全在 UI 层用 `mediaUri`/`filePath` + `MediaMetadataRetriever` 完成。缓存是进程内内存缓存（`LruCache(48)` + 未命中集合），不做磁盘缓存。
4. **横屏库页从 2 列网格改成单列整块列表**（删除 `GridRowCard`）。这是 D-3「一整块白色 surface 包含所有行」的直接要求，横屏每行可用宽度因此变宽；如果你希望横屏继续多列，需要另立需求。
5. **播放器补了无障碍语义**：封面 `contentDescription = "专辑封面"`，播放/暂停按钮 `contentDescription = "暂停"/"播放"`。这是 D-2 验收「能找到封面 image 节点 / 控制按钮 content-desc」的必要条件，同时也是无障碍改进；`RedesignScreensTest` 的播放器用例不受影响（129 tests 全绿）。
6. **横屏封面只有约 85dp**。在 1080px 高的横屏里，左栏要同时容纳「顶栏 + 封面 + 歌名 + 进度条 + 时间 + 三个控制键」，按任务书给的结构（权重只给封面）算下来封面就是剩余高度（≈235px）。满足「全部控件可见」的验收，但视觉上封面偏小。如果你希望封面更大，我建议把「歌名/歌手」移到封面右侧（横向排列）——这属于版面重排，等你确认后再做。
7. **Rail 的 inset 实现方式**：M3 `NavigationRail` 默认已消费纵向 inset，我把默认值设为 0 后自己消费 `statusBars`，并额外加 8dp 顶部呼吸空间（对应你「Rail 内容整体太靠上」的观感）。实测 Rail 首项标签 y 从 176 → 198px（状态栏 66px 之上再留 132px 视觉余量）。
8. **D-7 的另一半原因**是 `maxLines = 1` 硬裁剪（文本被切在「· 」处，看起来像多余分隔符）。我按任务书改了拼接逻辑，另外把行内文本改成 `Ellipsis`，否则「末尾 ·」还会以裁剪的形式出现。
9. **模拟器数据被重建过**：机器上原本装的是 release 签名的包，debug 包无法覆盖安装（`INSTALL_FAILED_UPDATE_INCOMPATIBLE`），卸载后库表清空。本轮通过 App 真实下载了 3 首（`https://b23.tv/Ndg49HZ`、`BV1Nxxr6oES2`、`BV1aB4y1u7ox`）重建数据，因此库页截图是真实链路产物（含封面/歌词）。
10. **`docs/e2e/` 新增的脚本与证据文件**：`r2_probe.py`、`r2_verify.py`、`r2_e2e.py`、`r2_run.py`、`r2_shots.py`、`r2-verify.txt`，以及 15 个 `r2-*.xml` dump。`docs/task-*.md` 与其它 `docs/*.md` 未改。
