# 用户反馈三项问题的修复与验收（父代理最终确认）

> 时间：2026-11-01 17:30 ~ 22:35
> 设备：模拟器 `nartest`（Android 14 / API 34），竖屏 1080×2340 / 横屏 2340×1080
> 交付包：`dist/NarMusic-v1.0.0.apk`（3,373,089 B，release + R8，已签名 `CN=NarMusic`，
> SHA-256 `32958CAA823C64A38783A99CAA832D43242FAEBDD293E3626FE99DCF00CAD2CF`）

## 一、问题 1：指定链接却下载成随机音乐 —— 已修复并端到端验证

### 根因（实测定位）

用户粘贴的是 B 站分享文本：

```
【百万级装备听《Counting Stars》- OneRepublic 【Hi-Res】-哔哩哔哩】 https://b23.tv/Ndg49HZ
```

`https://b23.tv/Ndg49HZ` 是**短链**，URL 里没有 BV 号。旧代码 `DownloadService.resolveStage` 的
`BiliUtils.extractBvid(url)` 对短链返回 `null`，于是**退化成分支**：把整段分享文本当关键词去搜索，
取搜索结果第一条 → 下成了另一个视频（用户看到的就是这个）。

### 修复（5 处，`core/` 与 `domain/`）

| 文件 | 改动 |
|---|---|
| `core/bili/LinkPlan.kt` | **新增**。把输入解析成 `Target.Bvid / Aid / NeedRedirect / Keyword / Empty`。**只要输入里出现链接，就绝不回退关键词搜索**；解析不出就报错 |
| `core/bili/BiliUtils.kt` | `resolveUrlInput` 遇到非 ASCII 或空白即截断（中文分享文本粘连时不再把中文带进 URL）+ 去尾随标点；新增 `extractAvId` 支持 `/video/av123` |
| `data/remote/HttpClient.kt` | `resolveRedirect` 改为 `followRedirects(false)` + 手动跟随最多 5 跳。OkHttp 默认自动跟随重定向，拿不到 `Location` 头，短链解析必然失败 |
| `data/remote/bili/BiliClient.kt` | 新增 `fetchVideoInfoByAid`（av 号链接用） |
| `domain/DownloadService.kt` | `resolveStage` 用新的 `resolveTarget()`：短链先跟随重定向再重新 plan；解析不出直接抛错，**不回退搜索** |

### 独立复算（不经过 App，纯 Python 复现链路）

```
> python docs/probe/verify_shortlink.py
step1 resolveUrlInput -> https://b23.tv/Ndg49HZ
step2 extractBvid -> None
step3 redirect status 302 -> Location 前缀 https://www.bilibili.com/video/BV14v4y1Q7nG?buvid=XUE612CEF3
step4 bvid from Location -> BV14v4y1Q7nG
step5 view -> aid 563960151 cid 929814545 duration 260
      title: 百万级装备听《Counting Stars》- OneRepublic【Hi-Res】
step6 audio streams 3 host upos-sz-mirrorcos.bilivideo.com
```

### release 包端到端验证（真网络、真下载）

```
> python docs/e2e/final_verify.py
[T+5s] ['下载中']
[T+10s] ['已完成']
done page: ... | 已完成 | 已下载 | 1.7 MB | 下载速度 | 0 B/s | ...
ls: -rwxrwx--- 1 u0_a172 media_rw 1749822 ... 百万级装备听《Counting Stars》- OneRepublic【Hi-Res】.m4a
```

落库 `narmeta` 原子（`----` freeform）实测内容：

```
> python -c "...读取 m4a 里 narmeta..."
narmeta url+aid: 563960151
platform: 酷狗音乐
delay: +0
filename: 百万级装备听《Counting Stars》- OneRepublic【Hi-Res】
```

`aid=563960151` ↔ `bvid=BV14v4y1Q7nG`（B 站 API 双向校验一致）—— **下载的就是用户指定的那一首**。

`ilst` 原子齐全、文件仍是合法 M4A：

```
ilst_atoms=["©nam", "©ART", "©alb", "©lyr", "covr", "----"]
format_name=mov,mp4,m4a,3gp,3g2,mj2   duration=259.157333   size=1749822
```

### 回归单测（锁住行为，防再退化）

`app/src/test/java/com/narnat/music/LinkPlanTest.kt`，10 例全绿，覆盖：短链必须走重定向、
分享文本里含中文仍能抠出短链、完整链接直接得 BV、裸 BV 号不当作关键词、重定向后的 URL、
av 链接、纯关键词、空输入、尾随中文标点、含标点的短链。

## 二、问题 2：没有图标 —— 已做自适应图标

- `res/drawable/ic_launcher_foreground.xml`：单色几何音符（椭圆符头 + 6dp 竖符干 + 贝塞尔符尾），
  108dp 画布，图形限制在中间 72dp 安全区。
- `res/mipmap-anydpi-v26/ic_launcher.xml` + `ic_launcher_round.xml`：`<adaptive-icon>` + `<monochrome>`（Android 13 主题图标）。
- 低版本兼容：`mipmap-{hdpi,xhdpi,xxhdpi,xxxhdpi,mdpi}` 下 5 档真实 PNG。
- `AndroidManifest.xml` 的 `<application>` 补 `android:icon` 与 `android:roundIcon`（此前缺失，这正是显示默认机器人的原因）。

`aapt2 dump badging` 原文：

```
application-icon-160:'res/BW.xml'
application-icon-240:'res/BW.xml'
application-icon-320:'res/BW.xml'
application-icon-480:'res/BW.xml'
application-icon-640:'res/BW.xml'
application: label='NarMusic' icon='res/BW.xml'
```

桌面实测截图 `docs/e2e/r-icon-desktop.png`：第三行第四列「NarMusic」是**蓝底白色音符**，不再是默认安卓机器人。

## 三、问题 3：界面不够美学 —— 按椒盐设计哲学重构 + 三轮精修

从 `E:\桌面\安卓软件\椒盐音乐设计哲学\` 的 5 张参考图提炼 9 条设计语言并逐条落地，再经三轮看图迭代修掉 12 处具体缺陷。

| # | 椒盐设计语言 | 改造前 | 改造后 |
|---|---|---|---|
| 1 | 沉浸式连续背景（不是白板） | 纯白 surface 画布上贴多个卡片 | `NarGradient` 从专辑封面主色晕染出低饱和渐变，整屏连续；播放页随封面色变化 |
| 2 | 极低分割感（整块 surface + 细线） | 每个模块各自描边卡片，互相割裂 | 统一 24dp 圆角整块 `NarSurface`，组内行用 1px `outlineVariant@0.5` 分隔线（`r3-p4-library.png` / `r3-l3-library.png`） |
| 3 | 无边框输入 | `OutlinedTextField` 蓝色描边方框（用户指出的"拼接感"主因） | 填充式圆角 16dp 无边框（`NarFillField`）；`ui/` 下 `OutlinedTextField` 计数 = **0** |
| 4 | 字体层级 | 标题与正文差异不足 | 大标题 28sp SemiBold / 行主 17sp Medium / 行副 13sp 低对比 / 数字 tabular |
| 5 | 主色稀释 | 大面积蓝 | 主色只用于按钮、选中态、进度；大面积中性色阶 |
| 6 | 单视觉重心按钮 | 「开始下载」实心胶囊 + 「保存到本地」浅灰胶囊并排 | 只保留一个实心胶囊主操作，次操作改纯文字（`r3-p1-download.png`） |
| 7 | 无胶囊底部导航 | 选中项一大坨 `primaryContainer` 椭圆底 | `indicatorColor = Color.Transparent` + 图标填充 + 主色文字（`NarBottomBar.kt:60,97`） |
| 8 | 全屏沉浸播放器 | `ModalBottomSheet` 弹层 + 独立白底歌词区 | 全屏 `PlayerScreen`，根容器唯一渐变，歌词浮在渐变上（`r3-p6-player.png`）；横屏左右双栏（`f-l3-player.png`） |
| 9 | 列表行真实封面 | 行首浅蓝底 + 数字「01/02/03」 | 行首 48dp 圆角 12dp **真实专辑封面**（M4A 内嵌 `covr`）；无封面降级为音符图标（`r3-p4-library.png` / `r3-l3-library.png`） |

### 三轮看图迭代修掉的 12 处缺陷

| 轮次 | 缺陷 | 证据 | 修法 |
|---|---|---|---|
| R2-1 | 状态栏时间压在 App 顶栏/导航上（全屏通用） | `r-p1-download.png`「11:06」盖住「纳儿音乐」 | 顶栏 / NavigationRail / 播放器顶栏全部消费 `statusBars` inset；15 份 dump 验证 App 文本 top ≥ 状态栏高 |
| R2-2 | 横屏播放器底部控制条被裁出屏幕 | `r-l5-player.png` 只见进度条，时间与按钮全不可见 | 左栏改权重布局，封面自动缩小；控制区 bottom=981 ≤ 1014 |
| R2-3 | 横屏库页每行一个独立圆角卡片 | `r-l3-library.png` 行间露背景 | 单块 surface + 行间 1px 分隔线 |
| R2-4 | 库页行首是序号方块不是封面 | `r-p3-library.png`「01/02/03」 | 改真实封面（`embeddedPicture` + Coil），无封面降级音符 |
| R2-5 | 顶栏大搜索框抢戏、页面标题被压低 | `r-p1-download.png` | 顶栏改「logo + 40dp 圆形搜索按钮」，点击才展开输入 |
| R2-6 | 下载页状态卡被底部导航裁切 | `r-p1-download.png`「状态」值被切 | 内容区可滚动 + 底部留白 |
| R2-7 | 库页副标题末尾多余「·」 | `r-l3-library.png`「酷狗音乐 ·」 | `listOfNotNull(...).joinToString(" · ")` |
| R3-1 | 播放器歌词区自带白底，与渐变割裂（横竖屏都有） | `r2-l5-player.png` 左右两块、`r2-p5-player.png` 上下两段 | 删掉歌词区 Surface，根容器唯一渐变；横屏窄条 stdLuma 12.8 → **2.1** |
| R3-2 | 横屏封面过小（85dp 像贴纸） | `r2-l5-player.png` | 封面 601×601px = **218.5dp**，控制区仍在屏内 |
| R3-3 | 旋转后库页第一行封面丢失 | 横屏降级成音符占位、竖屏正常 | 封面读取改串行限流 + 退避重试 + `release()` 在 finally；横竖屏行首平均色差 (4.8,4.7,4.6) ≤ 12 |
| R3-4 | Toast 压住底部导航 | `r2-p1-download.png` 盖住「批量下载」「库」 | Toast 抬到底栏之上，bottom=2010 ≤ 导航 top−8=2046 |
| R3-5 | 库页行尾 3 个图标挤压副标题 | 副标题被截成「…酷狗音乐···」 | 行尾只留「▶ 播放」+「⋮ 更多」，删除 / 打开位置收进下拉菜单 |

### 我在最终验收中额外发现并修掉的 5 处（含一次不实取证的纠正）

| 缺陷 | 证据 | 修法 |
|---|---|---|
| 横屏下载页「开始下载」按钮 bottom=1080 恰好被裁在屏幕边缘 | 修复前 dump `bounds=(1192, 1063, 1368, 1080)` | 页面改「左栏标题+可滚动输入卡+固定底部按钮」；修复后 `bounds=(769, 888, 945, 953)`，bottom=**953** |
| 横屏批量页「导入歌单」被挤到屏幕最右并竖排成单字 | `r3-l1-download.png` 时期 Tab 显示「导/入/歌/单」四行 | Tab 组件 `Column` 缺固有宽度约束，下划线 `fillMaxWidth` 拉满整行 → 改 `width(IntrinsicSize.Max)`；新 bounds 两个 Tab 相邻在 x 455..860 |
| **横屏状态卡完全在屏外**（用户点完下载后看不到进度与文件信息） | 修复前横屏完成态 dump 只有「开始下载/保存到本地」，`已下载/下载速度/下载时间/状态/文件名/文件大小` **全部缺失** | 横屏改左右双栏：左栏输入+按钮，右栏状态卡独立可滚动 |
| 横屏右栏状态卡底部「文件大小」仍差几像素被裁 | 截图 `f-l2-download-done.png`（22:17 版）右下角被切 | 去掉右栏顶部 34dp 对齐间隙；`文件大小` bottom=**942** ≤ 1080 |
| 横屏空态「歌曲名称 - 作者」输入框被裁一半 | 截图 `f-l2-download-empty.png`（2:22 版）第二输入框只露上半 | 横屏输入卡内边距 20→12dp、间距 20→8/6dp；两个输入框完整可见 |

**纠正一处不实取证（验证员指出，属实）**：上一版本报告把 `docs/e2e/f-l2-download.png` / `.xml` 当作横屏下载页证据，
但该 PNG 与 `f-l1-library.png` **MD5 完全相同**（`fd1500001974cd0779162012770635b1`），
其 xml 只有库页文本、无「开始下载」节点 —— 即它是横屏库页的重复件，被错误引用。
根因：当时的截图脚本 `land_tab()` 只按「底部导航 `bounds[1] > 1900`」找 Tab，
横屏时导航在左侧（x<300），该条件永不成立且函数静默返回 false，于是停留在库页截图并覆盖了目标文件名。
现在该文件已作废，改用下面两个独立证据文件；`f-l1-library.png` 保持原值未动。

## 三之二、横屏下载页专项复验（补做，2026-11-01 22:30）

复验脚本 `docs/e2e/verify_landscape_download.py`，全程自校验，输出 `RESULT: ALL PASS`：

```
== 1. 锁定横屏并校验
  PASS  screencap 尺寸 = 2340x1080（期望 2340x1080）
  PASS  hierarchy rotation="1"（期望 "1"）
== 2. 清空音乐库，进入「下载」标签（空态）
  PASS  重启后仍为横屏 = 2340x1080
  PASS  进入「下载」标签
  f-l2-download-empty: png=(2340, 1080) rotation=1
  PASS  空态 dump 无状态卡节点（确认非过期快照）
  PASS  空态 dump 含 text="开始下载"
  开始下载 bounds=(769, 888, 945, 953) bottom=953
  PASS  开始下载 bottom=953 <= 1080
  PASS  空态在下载页（含「音乐提取」）
  PASS  空态不是库页（无「音乐库」）
== 3. 填链接并开始下载（完成态）
  PASS  找到输入框 2 个
  PASS  下载完成
  f-l2-download-done: png=(2340, 1080) rotation=1
  PASS  完成态 dump 含状态卡与完成标记
  PASS  完成态 dump 含 text="开始下载"
  开始下载 bounds=(769, 756, 945, 821) bottom=821
  PASS  开始下载 bottom=821 <= 1080
  PASS  已下载 bounds=(1357, 259, 1462, 325)
  PASS  下载速度 bounds=(1736, 259, 1876, 325)
  PASS  下载时间 bounds=(1357, 450, 1497, 516)
  PASS  状态 bounds=(1736, 450, 1806, 516)
  PASS  文件名 bounds=(1357, 782, 1462, 848)
  PASS  文件大小 bounds=(1357, 876, 1497, 942)
== 4. 竖屏对照
  PASS  竖屏 screencap = 1080x2340
  PASS  竖屏进入「下载」标签
  f-l2-portrait-ref: png=(1080, 2340) rotation=0
  竖屏 开始下载 bounds=(452, 1928, 628, 1993)
  PASS  竖屏 dump 含「开始下载」
== RESULT: ALL PASS
```

### 证据文件（互不重复，MD5 各异）

| 文件 | 尺寸 | MD5 | 内容 |
|---|---|---|---|
| `docs/e2e/f-l2-download-empty.png` | 2340×1080 | `1eaa81278364d894af3eb31624753d7f` | 横屏下载页·空态 |
| `docs/e2e/f-l2-download-empty.xml` | — | — | 仅含 `开始下载`(bottom 953) / `音乐提取`，**无**状态卡节点 |
| `docs/e2e/f-l2-download-done.png` | 2340×1080 | `e382905da4fade52d59a1aead16775d9` | 横屏下载页·完成态（双栏，右栏状态卡完整） |
| `docs/e2e/f-l2-download-done.xml` | — | — | 含 `开始下载`(bottom 821) 与六个状态字段（bottom 259~942） |
| `docs/e2e/f-l1-library.png`（对照，未改） | 2340×1080 | `fd1500001974cd0779162012770635b1` | 横屏库页 |
| `docs/e2e/f-l2-portrait-ref.png` | 1080×2340 | — | 竖屏下载页对照 |

两份 dump 的节点集合确实不同（空态仅 2 个关键节点、完成态 8 个），排除了「截图是重复件」的可能；
脚本还额外断言「空态 dump 不含 `已下载`」，专门用来拦截 uiautomator 的过期快照。

## 四、横竖屏适配

- 判定：`BoxWithConstraints` 判 `maxWidth ≥ 600dp` 为大屏/横屏，不引额外依赖。
- 竖屏：底部 4 项 `NavigationBar`（无胶囊选中底）。
- 横屏：左侧 80dp `NavigationRail` + 内容区限宽 640dp 居中，且内容列消费 `statusBars` + `navigationBars` inset
  （见 `f-l2-download-empty.png` / `f-l4-batch.png` / `f-l5-settings.png` / `f-l1-library.png`）；
  播放器左右双栏（`f-l3-player.png`）。
- 下载页横屏为**左右双栏**：左栏「音乐提取」标题 + 可滚动输入卡 + 固定底部主按钮，右栏状态卡独立滚动
  （`f-l2-download-empty.png` / `f-l2-download-done.png`）。这样 1080px 高度下输入区、主按钮、状态卡三段都在屏内。
- `AndroidManifest` 的 `configChanges="orientation|screenSize|screenLayout|keyboardHidden|uiMode"`：旋转不重建 Activity、输入内容与状态不丢。
  注意：`am force-stop` 后重启会让系统旋转锁定复位，复验脚本因此每次重启后都重新 `cmd window user-rotation lock` 并校验分辨率为 2340×1080。

## 五、验收证据清单

| 项 | 值 / 文件 |
|---|---|
| 单元测试 | `TOTAL tests 131 failures 0 errors 0 skipped 0`（`python docs/probe/check_tests.py`） |
| 文案命中 | `hit 188 / rows 192`（`python docs/verify_copy.py`，与重构前持平；4 条未命中是 Web 端首屏 Toast 与搜索框空态提示，安卓版无对应入口） |
| 无边框输入 | `ui/` 下 `OutlinedTextField` 计数 = **0** |
| 无胶囊导航 | `NarBottomBar.kt:60,97` `indicatorColor = Color.Transparent` |
| 崩溃 | `logcat` 过滤 `FATAL EXCEPTION` / `E AndroidRuntime` / `ANR in` → `bad: []` |
| 端到端 | 短链 → 1749822 B M4A → `ilst` 六类原子齐 → DB 1 行 → 播放出声（`audio_flinger` 有活跃 track）→ 删除后目录空、DB 0 行 |
| 竖屏截图 | `f-p1-download-done` `f-p2-library` `f-p4-download` `f-p5-batch` `f-p6-settings` `f-p7-player` `f-l2-portrait-ref` `r3-p6-player` `r3-p7-dark-player`（均 1080×2340） |
| 横屏截图 | `f-l2-download-empty` `f-l2-download-done`（下载页空态/完成态，2340×1080，MD5 各异）`f-l1-library` `f-l3-player` `f-l4-batch` `f-l5-settings` `r3-l6-dark-settings` `r3-l7-dark-library`（均 2340×1080） |
| 桌面图标 | `r-icon-desktop.png`（1080×2340） |

## 六、未覆盖 / 已知限制

1. **真机未验证**：无 vivo S60 实机，arm64 硬解播放、FuntouchOS 后台策略、锁屏与来电打断均未覆盖。
2. **模拟器软渲染**：截图来自 `-gpu swiftshader_indirect`，字体抗锯齿与真机 GPU 有差异。
3. **汽水音乐**仍只支持「含 `track_id` 的分享链接」，关键词搜索路径上游已失效（`docs/api-probe.md` §7 实测）。
4. **重复下载同名歌曲**会替换旧文件（R2 已修），但 MediaStore 里被系统回收的时机由系统决定。
5. **预览/批量下载全链路**：批量页已验证解析（含 `url|平台|延迟|文件名` 精确行），但未跑多首真实批量下载。
