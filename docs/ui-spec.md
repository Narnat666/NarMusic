# NarMusic 安卓界面规格（task-S 产出）

> 本文是**施工图纸**，不含任何正式 App 代码。所有 `.kt` 片段都是伪代码/签名，供后续实现者照抄。
> 数据来源（全部本机实读，无一处凭空推测）：
> - Web 端：`E:\桌面\安卓软件\NarMusic\web\index.html`（377 行）、`web\css\style.css`（1816 行）、`web\app\modules\{download,batch,library,settings,search,player,lyrics}.js`、`web\app\{utils,api,main}.js`、`web\app\components\tabs.js`
> - SaltUI：`E:\桌面\安卓软件\SaltUI`（`gradle/libs.versions.toml` 版本 = `3.0.0-beta03`）的 `ui2` / `core` / `salt-adaptive-window` / `ui-navigation` 源码
> - Maven Central：`https://repo1.maven.org/maven2/io/github/moriafly/...` 真实 HTTP 查询（见 S-3.4）
> - 现有工程脚手架：`NarMusicAndroid\build.gradle.kts`、`app\build.gradle.kts`、`settings.gradle.kts`、`gradle.properties`、`gradle\wrapper\gradle-wrapper.properties`（→ Gradle 8.13）

---

## 0. 结论速览

| 项 | 结论 |
|---|---|
| 信息架构 | 4 Tab（下载 / 批量下载 / 库 / 设置）+ 全局顶栏搜索 + 全屏播放器弹窗 + 全局 Toast；无内嵌底部播放条 |
| 主色 | `#2D7BC1`（primary），背景 `#F8FAFD`，容器白 `#FFFFFF` |
| 圆角体系 | 胶囊 100px / 大卡 24px / 卡 20px / 输入与分组 16px / 小组件 12px / 小控件 8px |
| 标题字号 | `.section-title` 24sp SemiBold（移动端 22sp）→ **正好等于 SaltUI `SaltTextStyles.largeTitle`（24.sp + SemiBold）** |
| SaltUI 可用性 | 源码 `3.0.0-beta03` 需 Gradle 9.3.1 + AGP 9.1.1 + Kotlin 2.4.20 + compileSdk 37；发布物 `salt-ui` 存在（HTTP 200），但与本脚手架兼容的只有 **2.5.2**，且 2.5.2 缺 `BasicScreen/ScreenCard/TitleBarButton/PillButton/Sheet` |
| 推荐路径 | **B（`implementation("io.github.moriafly:salt-ui:2.5.2")`）作架构基线 + Material3 承载 MD3 视觉**；A 不建议，C 作兜底（详见 S-3.5） |

---

# S-1 NarMusic Web 界面信息架构

## S-1.1 全局骨架

Web 用 CSS Grid 两列：左 `260px` 侧栏 + 右主容器（`style.css:38-46`）。

```
body (grid 260px 1fr)                      ← style.css:41-45
├── aside.sidebar                          ← index.html:17   padding 24px 16px, gap 8px
│   ├── div.brand  [music_note] 纳儿音乐    ← index.html:18-21  22px/600
│   └── button.nav-item × 4                ← index.html:22-37  16px/24px padding, radius 100px
│        data-tab = download / batchDownload / library / settings
└── main.main-container                    ← index.html:41  bg=surface, radius 24px, margin 16px 16px 16px 0
    ├── header.topbar  高 72px             ← index.html:42-50
    │   ├── div.search-bar (400×48, 胶囊)  ← index.html:43
    │   │   ├── span#searchIcon  「search」
    │   │   └── input#searchInput  placeholder「模糊搜索歌曲、歌手...」
    │   └── div.user-profile (40×40 圆) img nar-logo.svg
    └── div.content-scroll  padding 32px   ← index.html:52
        └── 4 个 .tab-content（display:none 切换，tabs.js:15-35）
```

Tab 切换逻辑（`components/tabs.js:15-35`）：点击 `.nav-item` → 隐藏所有 `.tab-content` → 显示 `#{tabName}Tab` → 切 `.active`；`tabName==='library'` 时额外派发 `tab:library` 事件（`library.js:92` 监听后重新拉列表）。

`body.preload { visibility:hidden }`（`style.css:34-36`）+ `document.fonts.ready` 后移除（`main.js:20-27`）：字体加载完成前整页不显示，避免图标字体闪烁。

---

## S-1.2 Tab 1「下载」

容器：`#downloadTab`（`index.html:54-121`），内容区 `max-width:800px` 居中。

| # | 控件 | 类型 | 文案（原文） | 线号 | 交互 |
|---|---|---|---|---|---|
| 1 | `.section-title` | 标题 h2 | 音乐提取 | 55 | — |
| 2 | `#urlInput` | textarea rows=3 | placeholder `输入B站歌曲视频链接或包含链接的文本...` | 60 | `Ctrl+Enter` 触发开始下载（download.js:50-52） |
| 3 | `.input-label` | label | 音乐链接 | 59 | — |
| 4 | `#filenameInput` | text input | placeholder `歌曲名称 - 作者` | 65 | `Enter` 触发开始下载（download.js:53-55） |
| 5 | `.input-label` | label | 自定义文件名（歌词提取） | 64 | — |
| 6 | `#sendBtn` | `.btn-primary` | 开始下载 | 69-72 | 点击 → 校验 → `POST` 建任务；忙碌时文案变「发送中...」并禁用 |
| 7 | `#localDownloadBtn` | `.btn-secondary` | 保存到本地 | 73-76 | 初始 `display:none`；任务成功后才显示；点击 → 流式下载，按钮变「取消 下载中 n%」，再点取消 |
| 8 | `#statusCard` | `.status-card` | （整卡） | 80-119 | 初始 `display:none`，点开始下载后显示，`resetTaskStatus()` 重置 |
| 8a | `#statusLight` | 12×12 圆点 | — | 82 | class `idle`(#B0B0B0) / `downloading`(warning+脉冲) / `success` / `error` |
| 8b | `#taskStatus` | `.status-text` 16px/600 | 等待中 | 83 | 随状态变：等待中 / 下载中 / 下载完成 / 下载未完成 / `问题: {err}` |
| 9 | `.status-grid` | 4 格网格 | — | 86-103 | `auto-fit minmax(200px,1fr)`，移动端 2×2 |
| 9a | `#downloadedBytes` | `.status-value` 18px/600 | 0 B | 89 | 轮询每秒刷新（`formatBytes`） |
| 9b | `#downloadSpeed` | 同上 | 0 B/s | 93 | 最近 3 次采样均值（download.js:262-273） |
| 9c | `#downloadTime` | 同上 | - | 97 | 从建任务起累计，`formatTime` 输出「x.x秒 / x分y秒 / x小时y分」 |
| 9d | `#downloadStatus` | 同上 | - | 101 | 进行中 / 已完成 / 未完成 / - |
| 10 | `#fileInfo` | `.file-info` | 文件信息 | 106-116 | 成功后 `.show`；内部两行 label/value |
| 10a | `#fileName` | `.file-info-value` | - | 110 | 文件名（缺失显示「未命名文件」） |
| 10b | `#fileSize` | `.file-info-value` | - | 114 | `formatBytes` |

空态：下载页**没有空态组件**——`#statusCard` 默认隐藏，页面即「空」。文案表 T016 的「等待中」是卡片内部初值。

轮询：`setInterval(...,1000)`（download.js:284-298）；`task_not_found` → Toast「任务不存在或已过期」并 `stopPolling()`；结束 2 秒后停轮询（:254）。

---

## S-1.3 Tab 2「批量下载」

容器：`#batchDownloadTab`（`index.html:124-220`），`max-width:900px`。

| # | 控件 | 类型 | 文案（原文） | 线号 | 交互 |
|---|---|---|---|---|---|
| 1 | `.section-title` | h2 | 批量下载 | 125 | — |
| 2 | `#pasteModeBtn` | `.batch-mode-tab` | 粘贴歌单 | 128-131 | 切到粘贴模式（active 加 `.active`） |
| 3 | `#importModeBtn` | `.batch-mode-tab` | 导入歌单 | 132-135 | 切到导入模式 |
| 4 | `#batchPasteInput` | textarea rows=8 | placeholder 为空串 | 142 | `parsePlaylistText()` 按行解析（batch.js:210-259） |
| 5 | `.input-label` | label | 粘贴歌单内容 | 141 | — |
| 6 | `#batchFileUpload` | 虚线拖拽区 2px dashed | 点击或拖拽文件到此处 + 支持 .txt 文件 | 146-151 | 点击 → 文件选择器；dragover 加 `.drag-over`；非 .txt → Toast「请上传 .txt 文件」 |
| 7 | `#batchFileName` + `.batch-file-remove` | 胶囊条 | （动态文件名） | 152-158 | 显示已导入文件名；`close` 按钮清除 |
| 8 | `#batchParseBtn` | `.btn-primary` | 解析歌单 | 163-166 | 忙碌态「解析中...」并禁用 |
| 9 | `#batchClearBtn` | `.btn-secondary` | 清空 | 167-170 | 清输入、清结果、清 sessionStorage，Toast「已清空」 |
| 10 | `#batchParseProgress` | `.batch-parse-progress` | 正在解析歌单... | 173-184 | 旋转图标 + 6px 进度条 + `#batchProgressStats`「a / b」右对齐 |
| 11 | `.batch-results-summary` | 汇总行 | 解析结果：**N** 首 + 徽标 已找到 N + 徽标 未找到 N | 186-194 | `已找到` 绿(success-container)，`未找到` 红(error-container) |
| 12 | `#batchSelectAllCheckbox` | checkbox 18×18 | 全选 | 198-201 | 只对 `found=true` 生效 |
| 13 | `#batchSelectedCount` | 文本 13px | 已选 0 项 → 已选 N 项 | 202 | 选中数变化即刷新 |
| 14 | `#batchDownloadServerBtn` | `.btn-batch-download` | 下载到服务器 | 205-208 | 0 选中时 disabled；忙碌态「下载中...」 |
| 15 | `#batchDeleteSelectedBtn` | `.btn-batch-delete` | 删除选中 | 209-212 | 0 选中时 disabled；先 `confirm()` |
| 16 | `#batchResultsList` | 列表 16px 圆角 + 1px 边框 | — | 216-217 | 每行 grid `40px 40px 1fr auto 56px`，高 56px |
| 16a | 行内序号 | `.batch-item-index` | 01、02… | batch.js:394 | `padStart(2,'0')` |
| 16b | 行内 checkbox | — | — | batch.js:424 | 未找到行 disabled |
| 16c | 行标题/副标题 | `.batch-item-title` / `.batch-item-artist` | （动态） | batch.js:427-428 | narmeta 行副标题为 `{平台} · +{delay}ms`，否则 `{artist}` 或「未知歌手」 |
| 16d | 行状态徽标 | `.batch-item-status` | 精确匹配 / 已找到 / 下载中 / 已下载 / 失败 / 未找到 | batch.js:400-415 | 5 种配色：found / notfound / downloading(warning) / downloaded(primary-container) |
| 16e | 行删除按钮 | `.batch-item-delete` 28×28 | title「删除」 | batch.js:432 | 删除该行并重排序号 |

空态：结果区 `display:none`；列表为空时渲染「暂无搜索结果」（batch.js:388，含 `search_off` 图标 48px）。

歌单行格式（`batch.js:210-259`）：
- 精确行 `url|平台|延迟ms|文件名`（≥4 段且首段是 http）→ `narmeta=true`，跳过搜索直接「精确匹配」
- 普通行 `歌名 - 歌手` 或 `歌名-歌手` → 送批量搜索

---

## S-1.4 Tab 3「库」

容器：`#libraryTab`（`index.html:223-290`）。

| # | 控件 | 类型 | 文案（原文） | 线号 | 交互 |
|---|---|---|---|---|---|
| 1 | `.section-title` | h2 | 服务器音乐库 | 224 | — |
| 2 | `#librarySearchInput` | input | placeholder `搜索歌曲名...` | 229 | `input` 事件实时前端过滤（library.js:62-67） |
| 3 | `#librarySearchClear` | close 按钮 | — | 230-232 | 有输入才 `display:flex`；清空关键词 |
| 4 | `.filter-chip` × 7 | 胶囊筛选 | 全部 / A-Z / Z-A / 最新 / 最早 / 最大 / 最小 | 235-241 | data-filter = all / name-asc / name-desc / time-new / time-old / size-large / size-small |
| 5 | `#musicCount` | 文本 14px | `N` 首 | 246 | 显示**过滤后**条数 |
| 6 | `#batchSelectBtn` | `.btn-secondary` | 批量选择 ↔ 取消批量 | 248-251 / library.js:569,574 | 进入批量模式后加 `.active` |
| 7 | `#refreshLibraryBtn` | `.btn-secondary` | 刷新 | 252-255 | 忙碌态「加载中...」；有下载中时先 `confirm()` |
| 8 | `#batchToolbar` | `.batch-toolbar` | 全选 + 已选 N 项 + 3 个按钮 | 259-281 | 仅批量模式显示 |
| 8a | `#selectAllCheckbox` | checkbox | 全选 | 261-264 | 对**当前过滤结果**全选 |
| 8b | `#selectedCount` | 13px | 已选 0 项 | 265 | — |
| 8c | `#batchDownloadBtn` | `.btn-batch-download` | 批量下载 | 268-271 | 打包 zip 流式下载；再点取消 |
| 8d | `#batchPlaylistBtn` | `.btn-batch-playlist` | 歌单生成 | 272-275 | 生成 `NarMusic_playlist_yyyyMMdd_HHmmss.txt` |
| 8e | `#batchDeleteBtn` | `.btn-batch-delete` | 批量删除 | 276-279 | `confirm()` + 可选密码验证 |
| 9 | `.music-list-header` | 表头（桌面） | 歌曲 / 延迟 / 大小 / 下载时间 | library.js:291 | grid `48px 1fr 90px 80px 130px 72px`，高 36px，移动端隐藏 |
| 10 | `.music-item` | 行 高 52px | — | library.js:309-332 | 偶数行淡底 `rgba(0,0,0,.015)`；hover 时序号变播放按钮 |
| 10a | 序号 `01…` | `.music-index-num` 13px | — | library.js:312 | hover 隐藏 |
| 10b | 播放按钮 | `.music-index-play` 28×28 圆 | title「播放」 | library.js:313-315 | 打开播放器弹窗 |
| 10c | 标题 | `.music-title` 14px/500 | （去掉扩展名的文件名） | library.js:318 | 已下载过的条目文字转 primary 色 |
| 10d | 副标题 | `.music-subtitle` 12px | `{delay}ms · {size}` | library.js:319 | 桌面 `display:none`，移动端显示 |
| 10e | 延迟列 | `.music-delay` 13px | `{n}ms` | library.js:321 | 移动端隐藏 |
| 10f | 大小列 | `.music-size` 13px | `formatBytes` | library.js:322 | 移动端隐藏 |
| 10g | 时间列 | `.music-time` 13px | `yyyy-MM-dd HH:mm` | library.js:323 | 移动端 12px 右对齐 |
| 10h | 操作列 | `.music-actions-cell` 28×28 ×2 | title「下载」「删除」 | library.js:324-331 | 桌面 hover 才显形；移动端常显竖排；批量模式隐藏 |

三种「空」态（互斥）：
- 无数据：`音乐库为空` + `前往下载页面添加音乐`（index.html:286-287 / library.js:278，图标 `library_music` 48px）
- 有数据但过滤为空：`未找到匹配的音乐` + `尝试其他关键词或筛选条件`（library.js:287，图标 `search_off`）
- 加载中：`加载音乐库中...`（library.js:138，图标 `refresh` 旋转）
- 加载失败：`网络连接失败` + `请检查服务器是否运行`（library.js:146，图标 `wifi_off`）

密码保护对话框（library.js:459-525，**动态 DOM，非 index.html**）：标题「文件保护」、说明「请输入密码以执行删除操作」、密码框 placeholder「输入密码」+ 眼睛图标切换、错误行「密码错误」、按钮「取消」「确认」。

---

## S-1.5 Tab 4「设置」

容器：`#settingsTab`（`index.html:293-341`），`max-width:600px`。

| # | 控件 | 类型 | 文案（原文） | 线号 | 交互 |
|---|---|---|---|---|---|
| 1 | `.section-title` | h2 | 设置 | 294 | — |
| 2 | `.status-card` 1 | 卡片 | 下载设置 | 296-327 | — |
| 2a | `#settingsPlatformSelect` | `<select>` | 歌词提取平台 | 300-307 | 选项：网易云音乐 / 酷狗音乐 / QQ音乐 / 汽水音乐（默认第 1 项） |
| 2b | `#settingsDelayInput` | `<input type=number>` | 延迟 (ms)，value=0，step=100 | 310-311 | 非法整数时 `getSettingsValues()` 置 `delayError`，下载前 Toast「延迟参数必须是整数」 |
| 2c | 说明文字 | 12px | 歌词延迟调整 | 312-314 | — |
| 2d | `#saveSettingsBtn` | `.btn-secondary` | 保存设置 | 318-321 | 写 localStorage，Toast「设置已保存」 |
| 2e | `#resetSettingsBtn` | `.btn-secondary` | 恢复默认 | 322-325 | 平台回落网易云音乐、延迟 0，Toast「设置已恢复默认」 |
| 3 | `.status-card` 2 | 卡片 | 关于纳儿音乐 | 329-339 | 6 行说明文案（见文案表 T163-T168） |

空态/错误态：无。设置项存 `localStorage`：`musicPlatform`、`downloadDelay`（settings.js:24-25, 39-40）。

---

## S-1.6 播放器弹窗 `#audioSection`

结构（`index.html:348-370` + `style.css:408-528`）：

```
div#audioSection.audio-popup        320px 宽, radius 20px, 居中 fixed, z-index 1000
├── div.audio-popup-header          bg = primary-container, padding 16px 20px, cursor:move（可拖动）
│   ├── div.audio-popup-title       [music_note] 正在播放（16px/600, on-primary-container）
│   └── button#closeAudioBtn        32×32 圆, close 图标 → 关闭并 pause
└── div.audio-popup-content         padding 20px
    ├── div.audio-popup-track-info
    │   ├── div#playerTitle         18px/600, 单行 ellipsis, 默认「未在播放」
    │   └── div#playerArtist        14px, 默认「纳儿音乐」，播放时变「从音乐库播放」
    ├── audio#audioPlayer           原生 controls，宽 100%，radius 8px
    └── div.lyrics-container        max-height 200px 可滚动, bg = surface-variant,
        └── div#lyricsDisplay        radius 8px, padding 12px, margin-top 12px
            └── div.lyrics-line × N  14px, line-height 1.6, 居中, padding 4px 8px, radius 4px
                                      .active → color primary + 600 + bg primary-container
```

行为：
- 拖动：header `mousedown/touchstart` 拖整个弹窗，边界限制在视口内（player.js:141-227），位置存 `localStorage['audioPopupPosition']`
- 高亮：`timeupdate` → 找最后一个 `time <= currentTime` 的行加 `.active` 并 `scrollIntoView({block:'center'})`（lyrics.js:59-79）
- LRC 解析：`/\[(\d+):(\d+\.?\d*)\]/g`，多时间标签展开、按时间升序（lyrics.js:22-42）
- 歌词来源：`GET /api/lyrics?filename=` → `{lyrics: "lrc 文本"}`（lyrics.js:10-20）
- 无歌词：正文「暂无歌词」（lyrics.js:47）

> 注意：**没有** `#playerBar`/底部迷你播放条，桌面端也没有常驻播放器——播放器只有这一个弹窗，从库页点「播放」才出现。（`style.css:1436-1461` 里的 `.player-info/.player-title/.play-pause-btn/.progress-bar/.time-display` 等类在样式表中**全无定义**，是历史残留死代码。）

## S-1.7 顶部搜索栏行为（`search.js:9-60`）

1. 触发：`#searchInput` 回车（keydown/keyup 都拦）或点 `#searchIcon`。
2. 空关键词 → Toast「请输入搜索关键词」并聚焦。
3. 进行中图标变 `hourglass_top` 并旋转，输入框 `disabled`（单次请求防重入）。
4. 请求 `GET /api/search?keyword={kw}`（api.js:120-121）。
5. 成功（`data.link` 存在）→ **自动切到「下载」Tab**，把 `data.title + ' ' + data.link` 写进 `#urlInput`，把 `data.keyword` 写进 `#filenameInput`，Toast「已找到: {title}」。
6. 无 `link` → Toast「未找到匹配视频，请尝试其他关键词」。
7. 异常 → Toast「搜索失败: {msg}」。

即：顶栏搜索**不展示结果列表**，它是一个「搜索并回填下载页」的快捷入口。

## S-1.8 文案表

条目数：**192 条**（`T001`–`T192`，去重后唯一中文串 **189** 条，因为 T060/T074 等多行共享同一串）。`{...}` 表示运行时插值。

### A. 全局 / 导航 / 启动

| ID | 中文原文 | 出现位置 |
| T001 | 纳儿音乐 | index.html:6 title；:20 .brand；index.html:361 #playerArtist 默认值 |
| T002 | 搜索 | index.html:44 #searchIcon（Material Symbols 字形名） |
| T003 | 模糊搜索歌曲、歌手... | index.html:45 #searchInput placeholder |
| T004 | 下载 | index.html:24 导航项 1 |
| T005 | 批量下载 | index.html:28 导航项 2；:125 批量页 .section-title；:270 库页批量按钮 |
| T006 | 库 | index.html:31 导航项 3 |
| T007 | 设置 | index.html:36 导航项 4；:294 设置页 .section-title |
| T008 | 🎵 输入链接或包含链接的文本即可开始下载 | main.js:40 首屏 Toast(info)，延迟 1000ms |

### B. 下载页

| ID | 中文原文 | 出现位置 |
| T009 | 音乐提取 | index.html:55 .section-title |
| T010 | 音乐链接 | index.html:59 .input-label |
| T011 | 输入B站歌曲视频链接或包含链接的文本... | index.html:60 #urlInput placeholder |
| T012 | 自定义文件名（歌词提取） | index.html:64 .input-label |
| T013 | 歌曲名称 - 作者 | index.html:65 #filenameInput placeholder |
| T014 | 开始下载 | index.html:71 #sendBtn |
| T015 | 保存到本地 | index.html:75 #localDownloadBtn |
| T016 | 等待中 | index.html:83 #taskStatus 初值；download.js:197 idle 分支 |
| T017 | 已下载 | index.html:88 状态格 1 label |
| T018 | 下载速度 | index.html:92 状态格 2 label |
| T019 | 下载时间 | index.html:96 状态格 3 label；library.js:291 库表头 |
| T020 | 状态 | index.html:100 状态格 4 label |
| T021 | 0 B | index.html:89 #downloadedBytes 初值；download.js:205 |
| T022 | 0 B/s | index.html:93 #downloadSpeed 初值；download.js:206；utils.js:11 |
| T023 | - | index.html:97 #downloadTime；:101 #downloadStatus 初值；download.js:198 |
| T024 | 文件信息 | index.html:107 .file-info-title |
| T025 | 文件名 | index.html:109 .file-info-label |
| T026 | 文件大小 | index.html:113 .file-info-label |
| T027 | 下载中 | download.js:189 #taskStatus；batch.js:408 批量项徽标 |
| T028 | 进行中 | download.js:190 #downloadStatus |
| T029 | 下载完成 | download.js:193 #taskStatus |
| T030 | 已完成 | download.js:194 #downloadStatus |
| T031 | 下载未完成 | download.js:185 #taskStatus；:252 Toast(warning) |
| T032 | 未完成 | download.js:186 #downloadStatus |
| T033 | 问题: {error} | download.js:222 #taskStatus 前缀 |
| T034 | 请输入内容 | download.js:70 Toast(warning) |
| T035 | 未找到有效的链接 | download.js:73 Toast(warning) |
| T036 | 请输入自定义文件名 | download.js:76 Toast(warning) |
| T037 | 延迟参数必须是整数 | download.js:79 Toast(warning) |
| T038 | 正在创建下载任务... | download.js:83 Toast(info) |
| T039 | 发送中... | download.js:87 按钮忙碌态 |
| T040 | 下载任务已创建，开始下载... | download.js:97 Toast(success) |
| T041 | 下载未完成: {error} | download.js:101, 165 Toast(warning) |
| T042 | 连接错误: {msg} | download.js:105 Toast(warning) |
| T043 | 准备中... | download.js:123 按钮忙碌态；library.js:662 |
| T044 | 下载中 {n}% | download.js:142 按钮进度态；library.js:673 |
| T045 | 文件下载完成 | download.js:159, 248 Toast(success)；library.js:408 |
| T046 | 下载已取消 | download.js:161, 169；library.js:413, 424, 698 Toast(info) |
| T047 | 下载错误: {msg} | download.js:171 Toast(warning) |
| T048 | 任务不存在或已过期 | download.js:289 Toast(warning) |
| T049 | 未命名文件 | download.js:238 文件名兜底 |

### C. 批量下载页

| ID | 中文原文 | 出现位置 |
| T050 | 粘贴歌单 | index.html:130 #pasteModeBtn |
| T051 | 导入歌单 | index.html:134 #importModeBtn |
| T052 | 粘贴歌单内容 | index.html:141 .input-label |
| T053 | 点击或拖拽文件到此处 | index.html:148 .batch-upload-text |
| T054 | 支持 .txt 文件 | index.html:149 虚线区副文案 |
| T055 | 解析歌单 | index.html:165 #batchParseBtn |
| T056 | 清空 | index.html:169 #batchClearBtn |
| T057 | 正在解析歌单... | index.html:176 #batchProgressText |
| T058 | 正在批量搜索歌单... | batch.js:310 #batchProgressText |
| T059 | 解析结果：{n} 首 | index.html:190 汇总行 |
| T060 | 已找到 {n} | index.html:191 徽标；batch.js:405 单项徽标 |
| T061 | 未找到 {n} | index.html:192 徽标；batch.js:414 单项徽标 |
| T062 | 全选 | index.html:200 批量页；:263 库页 |
| T063 | 已选 {n} 项 | index.html:202, 265 初值「已选 0 项」；batch.js:487；library.js:627 |
| T064 | 下载到服务器 | index.html:207 #batchDownloadServerBtn |
| T065 | 删除选中 | index.html:211 #batchDeleteSelectedBtn |
| T066 | 解析中... | batch.js:283 按钮忙碌态 |
| T067 | 请先输入或导入歌单内容 | batch.js:271 Toast(warning) |
| T068 | 未解析到有效歌曲 | batch.js:274 Toast(warning) |
| T069 | 批量搜索失败: {msg} | batch.js:340, 351 Toast(warning) |
| T070 | 解析完成: 找到 {a} 首, 未找到 {b} 首 | batch.js:373 Toast |
| T071 | (含 {n} 首精确匹配) | batch.js:374 Toast 追加 |
| T072 | 暂无搜索结果 | batch.js:388 列表空态 |
| T073 | 精确匹配 | batch.js:403 徽标（narmeta 行） |
| T074 | 已下载 | batch.js:410 徽标 |
| T075 | 失败 | batch.js:412 徽标 |
| T076 | 未知歌手 | batch.js:419 副标题兜底 |
| T077 | 删除 | batch.js:432 按钮 title；library.js:328 按钮 title |
| T078 | 请上传 .txt 文件 | batch.js:165 Toast(warning) |
| T079 | 文件已导入: {name} | batch.js:184 Toast(success) |
| T080 | 文件读取失败 | batch.js:186 Toast(warning) |
| T081 | 已清空 | batch.js:207 Toast(info) |
| T082 | 确定要删除选中的 {n} 首歌曲吗？ | batch.js:527 confirm |
| T083 | 歌单已清空 | batch.js:517, 543 Toast(info) |
| T084 | 已删除 {n} 首歌曲 | batch.js:547 Toast(success) |
| T085 | 请先选择要下载的歌曲 | batch.js:553 Toast(warning) |
| T086 | 正在提交 {n} 首歌曲下载到服务器... | batch.js:560 Toast(info) |
| T087 | 提交失败 | batch.js:603, 608 单项错误文案 |
| T088 | 已提交 {n} 首歌曲下载 | batch.js:617 Toast(info) |
| T089 | ，{n} 首提交失败 | batch.js:617 Toast 追加 |
| T090 | 所有歌曲下载提交失败 | batch.js:620 Toast(warning) |
| T091 | 批量下载错误: {msg} | batch.js:630 Toast(warning) |
| T092 | 任务不存在 | batch.js:667 单项错误文案 |
| T093 | 状态查询失败 | batch.js:701 单项错误文案 |
| T094 | 状态查询多次失败，已停止轮询 | batch.js:706 Toast(warning) |

### D. 库页

| ID | 中文原文 | 出现位置 |
| T095 | 服务器音乐库 | index.html:224 .section-title |
| T096 | 搜索歌曲名... | index.html:229 #librarySearchInput placeholder |
| T097 | 全部 | index.html:235 筛选 chip（默认激活） |
| T098 | A-Z | index.html:236 chip name-asc |
| T099 | Z-A | index.html:237 chip name-desc |
| T100 | 最新 | index.html:238 chip time-new |
| T101 | 最早 | index.html:239 chip time-old |
| T102 | 最大 | index.html:240 chip size-large |
| T103 | 最小 | index.html:241 chip size-small |
| T104 | {n} 首 | index.html:246 #musicCount（过滤后计数） |
| T105 | 批量选择 | index.html:250；library.js:574 退出批量后的文案 |
| T106 | 取消批量 | library.js:569 进入批量后的文案 |
| T107 | 刷新 | index.html:255 #refreshLibraryBtn |
| T108 | 批量下载 | index.html:270 #batchDownloadBtn |
| T109 | 歌单生成 | index.html:274 #batchPlaylistBtn |
| T110 | 批量删除 | index.html:278 #batchDeleteBtn |
| T111 | 音乐库为空 | index.html:286；library.js:278 空态标题 |
| T112 | 前往下载页面添加音乐 | index.html:287；library.js:278 空态副文案 |
| T113 | 歌曲 | library.js:291 列表表头列 2 |
| T114 | 延迟 | library.js:291 表头列 3 |
| T115 | 大小 | library.js:291 表头列 4 |
| T116 | 未知文件 | library.js:294 文件名兜底 |
| T117 | 播放 | library.js:313 播放按钮 title |
| T118 | 下载 | library.js:325 下载按钮 title |
| T119 | 加载中... | library.js:136 #refreshLibraryBtn 忙碌态 |
| T120 | 加载音乐库中... | library.js:138 加载态标题 |
| T121 | 加载失败: {msg} | library.js:145 Toast(warning) |
| T122 | 网络连接失败 | library.js:146 错误态标题 |
| T123 | 请检查服务器是否运行 | library.js:146 错误态副文案 |
| T124 | 未找到匹配的音乐 | library.js:287 过滤空态标题 |
| T125 | 尝试其他关键词或筛选条件 | library.js:287 过滤空态副文案 |
| T126 | 有文件正在下载，刷新列表将中断下载，确定继续？ | library.js:28 confirm |
| T127 | 文件保护 | library.js:464 密码对话框标题 |
| T128 | 请输入密码以执行删除操作 | library.js:465 密码对话框说明 |
| T129 | 输入密码 | library.js:467 密码框 placeholder |
| T130 | 密码错误 | library.js:470 密码对话框错误行 |
| T131 | 取消 | library.js:472 密码对话框按钮 |
| T132 | 确认 | library.js:473 密码对话框按钮 |
| T133 | 文件ID无效 | library.js:529 Toast(warning) |
| T134 | 确定要删除「{name}」吗？此操作不可恢复。 | library.js:534 confirm |
| T135 | 已删除 | library.js:542 Toast(success) |
| T136 | 删除失败: {msg} | library.js:545 Toast(warning) |
| T137 | 需要密码验证 | library.js:551, 747 Toast(warning) |
| T138 | 删除错误: {msg} | library.js:553, 749 Toast(warning) |
| T139 | 请先选择要下载的文件 | library.js:647 Toast(warning) |
| T140 | 文件下载已开始 | library.js:696 Toast(success)，选中 1 项时 |
| T141 | {n} 个文件打包下载已开始 | library.js:696 Toast(success)，选中多项时 |
| T142 | 删除中... | library.js:736 #batchDeleteBtn 忙碌态 |
| T143 | 正在删除 {n} 个文件... | library.js:731 Toast(info) |
| T144 | 已删除 {n} 个文件 | library.js:740 Toast(success) |
| T145 | 请先选择要删除的文件 | library.js:725 Toast(warning) |
| T146 | 确定要删除选中的 {n} 个文件吗？此操作不可恢复。 | library.js:726 confirm |
| T147 | 生成中... | library.js:764 #batchPlaylistBtn 忙碌态 |
| T148 | 请先选择要生成歌单的文件 | library.js:759 Toast(warning) |
| T149 | 歌单已生成: {n} 条记录 | library.js:781 Toast(success) |
| T150 | 歌单生成失败: {msg} | library.js:783 Toast(warning) |
| T151 | 下载失败: {msg} | library.js:702 Toast(warning) |

### E. 设置页

| ID | 中文原文 | 出现位置 |
| T152 | 下载设置 | index.html:297 卡片标题 |
| T153 | 歌词提取平台 | index.html:300 .input-label |
| T154 | 网易云音乐 | index.html:302 option 1（settings.js:49 恢复默认值） |
| T155 | 酷狗音乐 | index.html:303 option 2 |
| T156 | QQ音乐 | index.html:304 option 3 |
| T157 | 汽水音乐 | index.html:305 option 4 |
| T158 | 延迟 (ms) | index.html:310 .input-label |
| T159 | 歌词延迟调整 | index.html:312-314 输入说明 |
| T160 | 保存设置 | index.html:319 #saveSettingsBtn |
| T161 | 恢复默认 | index.html:323 #resetSettingsBtn |
| T162 | 关于纳儿音乐 | index.html:330 卡片标题 |
| T163 | 基于B站歌曲链接下载音乐文件 | index.html:332 |
| T164 | -支持封面与歌词封装 | index.html:333 |
| T165 | -支持多平台歌词获取 | index.html:334 |
| T166 | -支持歌词与音频同步 | index.html:335 |
| T167 | -支持通过歌曲名搜索下载 | index.html:336 |
| T168 | -支持精准或模糊批量下载 | index.html:337 |
| T169 | 设置已保存 | settings.js:45 Toast(success) |
| T170 | 设置已恢复默认 | settings.js:54 Toast(info) |

### F. 播放器 / 歌词

| ID | 中文原文 | 出现位置 |
| T171 | 正在播放 | index.html:352 .audio-popup-title |
| T172 | 未在播放 | index.html:360 #playerTitle 初值 |
| T173 | 从音乐库播放 | player.js:67 #playerArtist 播放时 |
| T174 | 未知歌曲 | library.js:303；player.js:65 标题兜底 |
| T175 | 暂无歌词 | lyrics.js:47 歌词区空态 |
| T176 | 正在加载歌词... | player.js:70 Toast(info) |
| T177 | 歌词加载成功 | player.js:76 Toast(success) |
| T178 | 未找到歌词 | player.js:80 Toast(info) |
| T179 | 歌词加载失败 | player.js:83 Toast(warning) |
| T180 | 歌词库加载失败，请刷新页面 | player.js:86 Toast(warning) |
| T181 | 服务器请求失败: {err} | player.js:88 Toast(warning) |
| T182 | 音频文件读取失败 | player.js:90 Toast(warning) |
| T183 | 歌词数据不完整，尝试重新加载 | player.js:92 Toast(warning) |
| T184 | 点击播放按钮开始播放 | player.js:101 Toast(info)（自动播放被拦） |
| T185 | 正在加载音乐... | player.js:105 Toast(info) |
| T186 | 音频播放器初始化失败 | player.js:107 Toast(warning) |
| T187 | 文件名无效 | player.js:38；library.js:348 Toast(warning) |
| T188 | 未找到对应的音乐文件 | player.js:46 Toast(warning) |

### G. 顶栏搜索

| ID | 中文原文 | 出现位置 |
| T189 | 请输入搜索关键词 | search.js:31 Toast(warning) |
| T190 | 已找到: {title} | search.js:48 Toast(success) |
| T191 | 未找到匹配视频，请尝试其他关键词 | search.js:50 Toast(warning) |
| T192 | 搜索失败: {msg} | search.js:53 Toast(warning) |

> 表里 192 行、去重后**唯一中文串 189 条**（重复来自多位置共用同一条文案，例如「刷新」「批量下载」「已下载」「下载中」；T140/T141 是同一处 Toast 的两个分支）。统计口径与可复跑命令见「验收与复现」。

---

# S-2 配色与排版数据表

## S-2.1 颜色（`style.css:1-23` 的 `:root`）

| CSS 变量 | 十六进制 | 用途（谁在用） |
|---|---|---|
| `--md-sys-color-background` | `#F8FAFD` | body 背景、`.md-input` 底、`.status-card` 底、`.search-bar` 底、`.batch-file-upload` 底、`.batch-parse-progress` 底、`.batch-results-list` 底 |
| `--md-sys-color-surface` | `#FFFFFF` | `.main-container`、`.status-item`、`.file-info`、`.audio-popup`、移动端 body/`.sidebar` |
| `--md-sys-color-surface-variant` | `#E8EAED` | 所有 1px 分隔线、`.md-input` 边框、`.status-card` 边框、`.filter-chip` 边框、进度条槽、`.lyrics-container` 底 |
| `--md-sys-color-on-surface` | `#1A1A1A` | 正文/标题文字、`.btn-secondary` 文字、`.batch-upload-text` |
| `--md-sys-color-on-surface-variant` | `#5A5D63` | 次要文字、`.nav-item` 常态色、`.input-label`、`.status-label`、`.file-info-label`、`.music-*` 列、空态文字 |
| `--md-sys-color-primary` | `#2D7BC1` | `.btn-primary` 底、`.btn-batch-download` 底、播放圆钮、进度条填充、`.music-index-play`、高亮歌词、品牌图标 |
| `--md-sys-color-primary-container` | `#E3F0FF` | `.nav-item.active` 底、`.btn-secondary.active`、`.batch-toolbar` 底、`.batch-edit-toolbar` 底、`.batch-mode-tab.active`、`.filter-chip.active`、`.batch-file-name` 底、`.audio-popup-header` 底、`.lyrics-line.active` 底、选中行底 |
| `--md-sys-color-on-primary-container` | `#002A4D` | `.nav-item.active` 文字、`.audio-popup-title`、`.batch-select-all`、`.batch-selected-count`、`.batch-file-name` 文字、`.toast.info` 文字 |
| `--md-sys-color-secondary` | `#6B7685` | 仅定义，样式中未被直接引用（保留给后续/FAB） |
| `--md-sys-color-secondary-container` | `#E9EDF5` | `.btn-secondary` 底、`.filter-chip:hover`、`.batch-mode-tab:hover` |
| `--md-sys-color-error` | `#C62828` | `.btn-batch-delete` 底、错误态灯、`.toast.error` 文字/左边框、未找到徽标文字、`.batch-item-delete:hover` 色 |
| `--md-sys-color-error-container` | `#FFEBEE` | 未找到徽标底、`.toast.error` 底、`.batch-item-delete:hover` 底 |
| `--md-sys-color-success` | `#2E7D32` | 已找到徽标、成功态灯、`.toast.success` 文字/左边框 |
| `--md-sys-color-success-container` | `#E8F5E9` | 已找到徽标底、`.toast.success` 底 |
| `--md-sys-color-warning` | `#F57C00` | 下载中状态灯、下载中徽标、`.toast.warning` 文字/左边框 |
| `--md-sys-color-warning-container` | `#FFF3E0` | 下载中徽标底、`.toast.warning` 底 |
| `--player-bg` | `#FFFFFF` | 播放器专属背景（`:root` 第 21 行） |
| `--safe-bottom` | `env(safe-area-inset-bottom, 0px)` | 非颜色，安全区变量 |

**引用了但没定义**（隐性依赖，实现时必须补值）：`--md-sys-color-outline`（fallback `#79747e`，style.css:616、library.js:467）、`--md-sys-color-tertiary`（fallback `#7D5260`，style.css:786）、`--md-sys-color-surface-container-high`（fallback `#f0f0f0`，library.js:463）。

**硬编码色（非变量，需单独给出）**：`.btn-primary:hover` `#1A8AE6`、`.btn-batch-download:hover` `#1A8AE6`、`.btn-batch-delete:hover` `#B71C1C`、`.btn-batch-playlist:hover` `#5D3F4D`、`.btn-secondary:hover` `#C5D4EA`、`.status-light` 缺省 `#ccc`、`.status-light.idle` `#B0B0B0`、`.md-input:focus` 阴影 `rgba(0,97,164,0.1)`、`.btn-primary` 阴影 `rgba(0,97,164,0.3)`、hover 蒙层 `rgba(0,0,0,0.04)`/`0.03`/`0.015`、行分隔 `rgba(0,0,0,0.04)`。

`docs/ui/palette.json` 收录其中 26 个纯色值（16 个 `--md-sys-color-*` + `--player-bg` + 3 个 fallback + 4 个 hover 色 + 2 个状态灯灰）。

## S-2.2 圆角 / 尺寸 / 字体

### 圆角

| 值 | 用在哪 |
|---|---|
| `100px`（胶囊） | `.nav-item`、桌面 `.search-bar`、`.btn-primary`、`.btn-secondary`、`.btn-batch`、`.batch-mode-tab`、`.filter-chip`、`.batch-result-badge`、`.batch-item-status` |
| `40px` | 移动端 `.nav-item`、移动端 `.toast` |
| `24px` | 桌面 `.main-container`（唯一的大圆角容器，移动端归零） |
| `20px` | `.status-card`、`.audio-popup` |
| `16px` | `.md-input`、`.file-info`、`.batch-toolbar`、`.batch-file-upload`、`.batch-parse-progress`、`.batch-edit-toolbar`、`.batch-results-list`、密码对话框 |
| `12px` | `.status-item`、桌面 `.toast`、`.batch-file-name`、移动端 `.batch-toolbar`/`.batch-edit-toolbar`、密码对话按钮 |
| `8px` | `.library-search-bar`、`.audio-popup-player`、`.lyrics-container`、密码对话框输入框 |
| `4px` | `.lyrics-line` |
| `3px` | `.batch-progress-bar` / `.batch-progress-fill` |
| `50%` | `.status-light`、`.user-profile`、`.music-index-play`、`.music-action-btn`、`.audio-popup-close-btn`、`.batch-file-remove`、`.library-search-clear` |

### 间距 / 关键尺寸

| 元素 | 值 |
|---|---|
| `body` 网格 | `260px 1fr`（桌面），`100vh` |
| `.sidebar` | padding `24px 16px`，gap `8px` |
| `.brand` | padding `0 16px 24px`，gap `12px`，字号 22px，weight 600 |
| `.nav-item` | padding `16px 24px`，gap `16px`，高约 50px（16+16+18 行高） |
| `.topbar` | 高 `72px`，padding `0 24px`，下边框 1px |
| `.search-bar` | `400×48`，padding `0 16px`，gap `12px`，输入 16px |
| `.user-profile` | `40×40` 圆 |
| `.content-scroll` | padding `32px` |
| `.section-title` | **24px / 600**，margin-bottom `24px` |
| `.download-section` | max-width `800px` 居中 |
| `.batch-download-section` | max-width `900px` 居中 |
| `.input-group` | margin-bottom `20px` |
| `.input-label` | **14px / 500**，margin-bottom `8px` |
| `.md-input` | padding `16px 20px`，**16px**，边框 1px |
| `.btn-primary` | padding `16px 32px`，**16px / 600**，阴影 `0 2px 8px rgba(0,97,164,.3)` |
| `.btn-secondary` | padding `12px 24px`，**14px / 600** |
| `.btn-batch` | padding `8px 16px`，**13px / 600**，图标 18px |
| `.status-card` | padding `24px`，margin-top `24px`，边框 1px |
| `.status-header` | gap `12px`，margin-bottom `20px` |
| `.status-light` | `12×12` |
| `.status-text` | **16px / 600** |
| `.status-grid` | `auto-fit minmax(200px,1fr)`，gap `16px` |
| `.status-item` | padding `16px` |
| `.status-label` | **12px**，uppercase，letter-spacing `0.5px` |
| `.status-value` | **18px / 600**，tabular-nums |
| `.file-info` | padding `20px`，margin-top `16px` |
| `.file-info-title` | **14px / 600**，uppercase，letter-spacing `0.5px` |
| `.file-info-row` | padding `8px 0`，下边框 1px（最后一行无） |
| `.audio-popup` | 宽 `320px`，阴影 `0 8px 32px rgba(0,0,0,.2)` |
| `.audio-popup-header` | padding `16px 20px` |
| `.audio-popup-title` | **16px / 600** |
| `.audio-popup-close-btn` | `32×32` |
| `.audio-popup-content` | padding `20px` |
| `.audio-popup-track-title` | **18px / 600**，单行 ellipsis |
| `.audio-popup-track-artist` | **14px** |
| `.lyrics-container` | max-height `200px`，padding `12px`，margin-top `12px` |
| `.lyrics-display` | **14px**，line-height `1.6`，居中 |
| `.lyrics-line` | padding `4px 8px` |
| `.toast` (桌面) | top/right `24px`，padding `16px 24px`，左边框 `4px` |
| `.music-list-header` | grid `48px 1fr 90px 80px 130px 72px`，高 `36px`，**12px / 500** |
| `.music-item` | 同 grid，高 `52px`，padding `0 12px`，底边 `1px rgba(0,0,0,.04)` |
| `.music-index-num` | **13px** |
| `.music-index-play` | `28×28` 圆，图标 18px |
| `.music-title` | **14px / 500** |
| `.music-subtitle` | **12px**（桌面 `display:none`） |
| `.music-delay` / `.music-size` / `.music-time` | **13px** |
| `.music-action-btn` | `28×28` 圆，图标 18px |
| `.library-search-bar` | padding `6px 12px`，max-width `320px`，图标 20px |
| `.filter-chip` | padding `6px 14px`，**13px / 500** |
| `.batch-toolbar` / `.batch-edit-toolbar` | padding `12px 20px`，gap `12px` |
| `.batch-select-all` | **14px / 500**，checkbox `18×18` |
| `.batch-selected-count` | **13px** |
| `.batch-mode-tab` | padding `12px 24px`，**14px / 600** |
| `.batch-textarea` | min-height `180px`，line-height `1.6` |
| `.batch-file-upload` | padding `40px 20px`，虚线 2px，图标 48px |
| `.batch-upload-text` | **16px / 500**，margin-top `12px` |
| `.batch-file-name` | padding `12px 16px`，**14px / 500** |
| `.batch-parse-progress` | padding `20px 24px`，margin-top `24px` |
| `.batch-progress-bar` | 高 `6px` |
| `.batch-progress-stats` | **13px**，右对齐 |
| `.batch-results-summary` | **15px**，gap `12px` |
| `.batch-result-badge` | padding `4px 12px`，**12px / 600** |
| `.batch-result-item` | grid `40px 40px 1fr auto 56px`，高 `56px` |
| `.batch-item-title` | **14px / 500**；`.batch-item-artist` **12px** |
| `.batch-item-status` | padding `4px 10px`，**12px / 600** |
| 字体栈 | `'Google Sans', system-ui, -apple-system, BlinkMacSystemFont, sans-serif`（style.css:29）；图标字体 `Material Symbols Rounded`（wght 400 / FILL 1 / opsz 24，index.html:8） |
| 动效 | `--` 统一 `0.2s` 过渡；状态灯 `pulse 2s infinite`；加载图标 `spin 1s linear infinite`；行 hover `0.15s`；进度条 `width 0.3s ease` |

## S-2.3 移动端媒体查询改动清单（逐条）

### 块 1：`@media (max-width: 768px)`（style.css:531-536）

| 选择器 | 改动 |
|---|---|
| `.audio-popup` | `width: calc(100% - 40px)`；`max-width: 400px` |

### 块 2：`@media (max-width: 768px)`（style.css:1026-1425）

| # | 选择器 | 改动 |
|---|---|---|
| 1 | `body` | grid → `flex column`；`height:100vh`；`overflow:hidden`；背景改 `--md-sys-color-surface`（桌面是 background 色） |
| 2 | `.sidebar` | 变成**顶部横向导航**：`flex-direction:row`；`width:100%`；padding `8px 16px`；gap `8px`；`overflow-x:auto` + 隐藏滚动条；背景 surface；下边框 1px；`order:0`；`flex-shrink:0` |
| 3 | `.sidebar::-webkit-scrollbar` | `display:none` |
| 4 | `.brand` | `inline-flex`；gap `6px`；padding `8px 12px`；字号 **18px**；`margin-right:4px`；不换行 |
| 5 | `.brand .material-symbols-rounded` | 字号 `24px` |
| 6 | `.nav-item` | `inline-flex`；`width:auto`；padding **`10px 18px`**；gap `8px`；字号 14px；圆角 **40px**；不换行 |
| 7 | `.main-container` | `margin:0`；圆角 `0`；去阴影；`flex:1`；`min-height:0` |
| 8 | `.topbar` | 高 **60px**；padding `0 16px` |
| 9 | `.search-bar` | 宽 100%；max-width 100%；高 **44px**；padding `0 12px` |
| 10 | `.search-bar input` | 字号 **15px** |
| 11 | `.user-profile` | `36×36`；`margin-left:12px` |
| 12 | `.content-scroll` | padding **`20px 16px`** |
| 13 | `.section-title` | 字号 **22px**；margin-bottom **20px** |
| 14 | `.download-section` | max-width 100% |
| 15 | `.input-row` | 单列；gap 12px |
| 16 | `.md-input` | padding `14px 16px`；字号固定 16px（防 iOS 缩放） |
| 17 | `.btn-primary` | 宽 100%；padding `16px 20px`；字号 16px |
| 18 | `.btn-secondary` | padding `12px 18px` |
| 19 | `.status-card` | padding `20px 16px`；margin-top 20px |
| 20 | `.status-grid` | **固定 2 列** `1fr 1fr`（桌面 auto-fit）；gap 12px |
| 21 | `.status-item` | padding `14px` |
| 22 | `.file-info` | padding `16px` |
| 23 | `.audio-section` | padding 16px；margin-top 20px（**该类在样式表中不存在，死规则**） |
| 24 | `.toast` | top/right/left 都 16px；`transform: translateY(-120%)`；padding `14px 20px`；字号 15px；居中；圆角 **40px**；max-width 400px |
| 25 | `.toast.show` | `translateY(0)`（桌面是 `translateX(0)`） |
| 26 | `.music-list-header` | `display:none`（移动端不显示表头） |
| 27 | `.library-toolbar-right` | `flex-wrap:wrap` |
| 28 | `.library-toolbar-right .btn-secondary` | padding `8px 14px`；字号 13px |
| 29 | `.filter-chip` | padding `5px 10px`；字号 12px |
| 30 | `.batch-toolbar` | padding `10px 14px`；圆角 12px；`flex-direction:column`；gap 10px |
| 31 | `.batch-toolbar-left` / `.batch-toolbar-right` | `width:100%`（左 space-between / 右 flex-end） |
| 32 | `.btn-batch` | padding `8px 12px`；字号 12px；图标 16px |
| 33 | `.music-item` | grid 改 **`36px 1fr auto auto`**；高 auto、min-height **56px**；padding `10px 12px`；底边 1px surface-variant |
| 34 | `.batch-mode .music-item` | grid `28px 1fr auto` |
| 35 | `.batch-mode .music-time` | `display:none` |
| 36 | `.batch-mode .music-checkbox` | `22×22` |
| 37 | `.music-delay` / `.music-size` | `display:none`（移动端只留标题+时间） |
| 38 | `.music-time` | 字号 12px；右对齐；padding-right 4px |
| 39 | `.music-index-play` | `24×24`；图标 16px |
| 40 | `.music-name` | padding-right 8px；`max-width:none` |
| 41 | `.music-title` | 字号 **15px** |
| 42 | `.music-subtitle` | 字号 12px；**`display:block`**（移动端才显示） |
| 43 | `.music-actions-cell` | `opacity:1`（常显）；`flex-direction:column`；gap 2px |
| 44 | `.music-action-btn` | `32×32`；图标 20px |
| 45 | `.batch-input-mode` | `width:100%` |
| 46 | `.batch-mode-tab` | `flex:1`；padding `10px 16px`；字号 13px |
| 47 | `.batch-textarea` | min-height **140px** |
| 48 | `.batch-file-upload` | padding `28px 16px`；图标 36px |
| 49 | `.batch-upload-text` | 字号 14px |
| 50 | `.batch-edit-toolbar` | padding `10px 14px`；圆角 12px；竖排；gap 10px |
| 51 | `.batch-edit-toolbar-left` / `-right` | `width:100%` |
| 52 | `.batch-result-item` | grid `32px 32px 1fr auto 44px`；高 auto、min-height **52px**；padding `8px 10px` |
| 53 | `.batch-item-checkbox input` | `20×20` |
| 54 | `.batch-item-title` | 字号 14px；`.batch-item-artist` `display:block` |
| 55 | `.batch-item-status` | 字号 11px；padding `3px 8px` |
| 56 | `.batch-result-badge` | 字号 11px；padding `3px 8px` |
| 57 | `.batch-results-summary` | 字号 14px；gap 8px |
| 58 | `.batch-parse-progress` | padding `16px`；`.batch-progress-header` 字号 14px |
| 59 | `.btn-primary` | 宽 100%（与 #17 重复定义） |

### 块 3：`@media (max-width: 400px)`（style.css:1428-1462）

| 选择器 | 改动 | 备注 |
|---|---|---|
| `.brand` | 字号 16px | 生效 |
| `.nav-item` | padding `8px 14px`；字号 13px | 生效 |
| `.player-info` | max-width 120px | **样式表中无此类的定义 → 死规则** |
| `.player-title` | 字号 12px | 死规则 |
| `.player-artist` | 字号 10px | 死规则 |
| `.player-controls .icon-btn:not(.play-pause-btn)` | `32×32` | 死规则 |
| `.play-pause-btn` | `40×40` | 死规则 |
| `.progress-bar` | width 45px | 死规则 |
| `.time-display` | 字号 10px | 死规则 |
| `.player-extra` | min-width 100px | 死规则 |

> 结论：手机形态下真正生效的只有 `.brand` 与 `.nav-item` 两条；其余 8 条属于已删除的底部播放条残留。**安卓移植时不要实现这 8 条。**

## S-2.4 palette.json

`E:\桌面\安卓软件\NarMusicAndroid\docs\ui\palette.json`，**26 个键**，值全为 `#RRGGBB`。
校验命令与结果见「验收与复现」。

---

# S-3 SaltUI 能力勘察

勘察对象：`E:\桌面\安卓软件\SaltUI`（`libs.versions.toml` → `version = "3.0.0-beta03"`，`gradle-wrapper.properties` → `gradle-9.3.1-bin.zip`）。

## S-3.1 能否在「单模块纯 Android（非 KMP）」工程里直接当 Android Library 依赖？

**能，但只能用「发布物 + 老版本」。源码直挂（子工程）与本脚手架不兼容。** 证据链：

1. **`ui2` 本身是 Kotlin Multiplatform 模块，不是 Android Library 模块。**
   `ui2/build.gradle.kts:21-29` 的插件是：
   ```kotlin
   alias(libs.plugins.kotlin.multiplatform)              // org.jetbrains.kotlin.multiplatform 2.4.20
   alias(libs.plugins.kotlin.plugin.compose)
   alias(libs.plugins.android.kotlin.multiplatform.library)  // com.android.kotlin.multiplatform.library 9.1.1
   alias(libs.plugins.compose.multiplatform)             // org.jetbrains.compose 1.12.2
   alias(libs.plugins.vanniktech.maven.publish)
   alias(libs.plugins.kover)
   ```
   即它用的是 **AGP 9.1.1 的新 KMP-Android 插件**，不是 `com.android.library`。
2. **它产出的是 AAR 变体，可被 Android 工程消费**：`kotlin { android { namespace = "com.moriafly.salt.ui"; compileSdk = 37; minSdk = 23; compilerOptions { jvmTarget = JVM_21 } } }`（`ui2/build.gradle.kts:126-144`），且 `targets.withType<KotlinAndroidTarget>().configureEach { publishLibraryVariants("release") }`（:210-212）。
3. **`SaltTheme` 完全来自 `ui2` 自己的 commonMain**（`ui2/src/commonMain/kotlin/com/moriafly/salt/ui/SaltTheme.kt`），不依赖任何 Android 主题资源；`androidMain` 只有 expect/actual 与平台适配（**32 个 `.kt`**，如 `SaltShapes.android.kt`、`SaltTextStyles.android.kt`、`SaltTheme.android.kt`、`dialog/Dialog.android.kt`、`app/SaltApplication.kt`、`ext/ActivityExt.kt`、`util/SystemUtil.kt`）。
4. **`androidMain` 有真 Android 依赖**：`core-ktx`、`com.google.android.material:material`、`androidx.compose.material3`、`activity-compose`、`org.lsposed.hiddenapibypass`（`ui2/build.gradle.kts:166-172`）。其中 **`hiddenapibypass:6.1`** 会进入 App 运行时。
5. **`ui2` 的 `commonMain` 还 `api(project(":core"))` 与 `api(project(":salt-adaptive-window"))`**（:176-177），所以最少要引入 3 个模块/3 个发布物。
6. **额外传递依赖（3.0.0-beta03）**：`dev.chrisbanes.haze:2.0.1` + `haze-blur` + `haze-blur-materials`、`androidx.collection:1.5.0`、`androidx.graphics:graphics-shapes:1.0.1`、`org.jetbrains.compose.*:1.12.2`。

结论：**作为 AAR 依赖可用；作为 Gradle 子工程直接挂进 AGP 8.9.1 工程不可用**（需要 AGP 9.1.1 才认识 `com.android.kotlin.multiplatform.library` 插件，而 AGP 9.x 又要求 Gradle 9.x，本工程是 Gradle 8.13）。

## S-3.2 主题类 API 签名（逐字抄自源码）

### SaltTheme（`SaltTheme.kt:38-104`）

```kotlin
@Composable
fun SaltTheme(
    configs: SaltConfigs = SaltConfigs.default(),
    dynamicColors: SaltDynamicColors = SaltDynamicColors(
        light = SaltColors.defaultLight(),
        dark = SaltColors.defaultDark()
    ),
    textStyles: SaltTextStyles = SaltTheme.textStyles,
    dimens: SaltDimens = SaltTheme.dimens,
    shapes: SaltShapes = SaltTheme.shapes,
    content: @Composable () -> Unit
)

@UnstableSaltUiApi
@Composable
fun SaltTheme(
    configs: SaltConfigs = SaltConfigs.default(),
    dynamicColors: SaltDynamicColors = SaltDynamicColors(...),
    textStyles: SaltTextStyles = SaltTheme.textStyles,
    dimens: SaltDimens = SaltTheme.dimens,
    shapes: SaltShapes = SaltTheme.shapes,
    material: SaltMaterial = SaltTheme.material,              // ← 3.0 新增，控制 Liquid Glass
    basicScreenStyle: BasicScreenStyle = SaltTheme.basicScreenStyle,
    content: @Composable () -> Unit
)

@UnstableSaltUiApi @Composable
fun ChangeSaltThemeIsDark(isDarkTheme: Boolean, content: @Composable () -> Unit)

object SaltTheme {
    val configs: SaltConfigs            @Composable @ReadOnlyComposable get()
    val colors: SaltColors              @Composable @ReadOnlyComposable get()   // 依 configs.isDarkTheme 取 dark/light
    val textStyles: SaltTextStyles      @Composable @ReadOnlyComposable get()
    val dimens: SaltDimens              @Composable @ReadOnlyComposable get()
    val shapes: SaltShapes              @Composable @ReadOnlyComposable get()
    @UnstableSaltUiApi val material: SaltMaterial @Composable @ReadOnlyComposable get()
    @UnstableSaltUiApi val basicScreenStyle: BasicScreenStyle @Composable @ReadOnlyComposable get()
}
```

**前置条件（必须做，缺一不可）**：
1. 最外层套 `SaltTheme { ... }`——它内部会 `CompositionLocalProvider(LocalIndication / LocalContentColor / LocalSaltConfigs / LocalSaltDynamicColors / LocalSaltTextStyles / LocalSaltDimens / LocalSaltShapes / LocalSaltMaterial / LocalBasicScreenStyle / LocalHazeState + ProvideTextStyle(main))`（:87-103）。**不套就没有 `SaltTheme.colors` 等一切**。
2. 暗色切换用 `SaltTheme(configs = SaltConfigs.default(isDarkTheme = isDark))` 或 `ChangeSaltThemeIsDark(...)`。
3. `material` 默认 `MaterialType.None`（`SaltMaterial.default()`），选非 None 时会 `remember { HazeState() }`，即引入 haze 模糊链路。
4. `BasicScreen` 的 `TitleBarBackdropType` 默认按 SDK 版本选 `Progressive`（≥33）/ `Mask`（≥32）/ `None`；`Mask`/`Progressive` 走 haze 模糊，**低端机有性能代价**。
5. Android 侧若要状态栏颜色跟随，用 `com.moriafly.salt.ui.util.WindowUtil` + `com.moriafly.salt.ui.ext.edgeToEdge()`（见 `androidApp/.../MainActivity.kt:38-69`、`shared/.../theme/AppTheme.kt:33-41`）。

### SaltColors（`SaltColors.kt:74-197`）

```kotlin
@Stable
class SaltColors(
    highlight: Color, text: Color, subText: Color,
    background: Color, subBackground: Color, popup: Color,
    stroke: Color, onHighlight: Color,
    success: Color, warning: Color, error: Color
) {
    val highlight: Color; val text: Color; val subText: Color
    val background: Color; val subBackground: Color; val popup: Color
    val stroke: Color; val onHighlight: Color
    val success: Color; val warning: Color; val error: Color   // ← 3.0 新增 3 个
    fun copy(/* 同上 11 个参数，全部有默认值 */): SaltColors
    companion object {
        fun defaultLight(
            highlight: Color = Color(0xFF0470E6), text: Color = Color(0xFF1E1715),
            subText: Color = Color(0xFF8C8C8C), background: Color = 0xFFF3F3F3(Android)/0xFFF2F2F7(iOS),
            subBackground: Color = 0x80FFFFFF(Android)/0xFFFFFFFF(iOS),
            popup: Color = subBackground.compositeOver(background),
            stroke: Color = subText.copy(alpha = 0.15f),
            onHighlight: Color = Color.White,
            success = SaltPalette.SuccessLightIcon,
            warning = SaltPalette.WarningLightIcon,
            error   = SaltPalette.ErrorLightIcon
        ): SaltColors
        fun defaultDark(
            highlight: Color = Color(0xFF0088FF), text: Color = Color(0xFFEBEEF1),
            subText: Color = Color(0xBFE1E6EB), background: Color = 0xFF202020(Android)/0xFF000000(iOS),
            subBackground: Color = Color(0x08FFFFFF),
            popup = subBackground.compositeOver(background),
            stroke = subText.copy(alpha = 0.1f), onHighlight = Color.White,
            success = SaltPalette.SuccessDarkIcon, warning = SaltPalette.WarningDarkIcon,
            error = SaltPalette.ErrorDarkIcon
        ): SaltColors
    }
}

@Stable
data class SaltDynamicColors(val light: SaltColors, val dark: SaltColors) {
    companion object { fun default(light: SaltColors = ..., dark: SaltColors = ...): SaltDynamicColors }
}
```

> **关键落差**：SaltColors 只有 11 个槽位（highlight/text/subText/background/subBackground/popup/stroke/onHighlight/success/warning/error），**没有 MD3 的 primaryContainer/onPrimaryContainer/surfaceVariant 等**。Web 的 16 个 MD3 变量无法 1:1 塞进 SaltColors，必须自建扩展色板。

### SaltPalette（`SaltPalette.kt:27-40`，纯常量，无参数）

```kotlin
object SaltPalette {
    val WarningLightIcon = Color(0xFF9D5D00);  val WarningDarkIcon = Color(0xFFFCE100)
    val WarningLightBackground = Color(0xFFFFF4CE); val WarningDarkBackground = Color(0xFF433519)
    val ErrorLightIcon = Color(0xFFC42B1C);    val ErrorDarkIcon = Color(0xFFFF99A4)
    val ErrorLightBackground = Color(0xFFFDE7E9); val ErrorDarkBackground = Color(0xFF442726)
    val SuccessLightIcon = Color(0xFF0E8A00);  val SuccessDarkIcon = Color(0xFFB8F2A9)
    val SuccessLightBackground = Color(0xFFE6F8E0); val SuccessDarkBackground = Color(0xFF2B4C1F)
}
```

### SaltTextStyles（`SaltTextStyles.kt:28-77`）

```kotlin
@Immutable
class SaltTextStyles(val main: TextStyle, val sub: TextStyle, val paragraph: TextStyle, val largeTitle: TextStyle) {
    fun copy(main: ..., sub: ..., paragraph: ..., largeTitle: ...): SaltTextStyles
}

fun saltTextStyles(
    main: TextStyle = DefaultTextStyle,          // Android actual: TextStyle(fontSize = 16.sp)
    sub: TextStyle = SubTextStyle,               // fontSize = 12.sp
    paragraph: TextStyle = ParagraphTextStyle,   // fontSize = 16.sp, lineHeight = 1.5f.em
    largeTitle: TextStyle = LargeTitleTextStyle  // fontSize = 24.sp, fontWeight = SemiBold
): SaltTextStyles
```

`largeTitle` = **24sp SemiBold**，与 Web `.section-title` 完全一致，可直接复用。

### SaltShapes（`SaltShapes.kt:34-55` + `androidMain/SaltShapes.android.kt:26-33`）

```kotlin
@Stable
class SaltShapes(small: Shape, medium: Shape, large: Shape) { val small; val medium; val large }
fun SaltShapes.Companion.default(small = SaltShapesDefaults.small, medium = ..., large = ...): SaltShapes
expect object SaltShapesDefaults { val small: Shape; val medium: Shape; val large: Shape }
// Android actual: small = RoundedCornerShape(8.dp); medium = RoundedCornerShape(16.dp); large = RoundedCornerShape(24.dp)
```

### SaltDimens（`SaltDimens.kt:59-188`）

```kotlin
@Stable
class SaltDimens(item: Dp, itemIcon: Dp, corner: Dp, dialogCorner: Dp, padding: Dp, subPadding: Dp) {
    val item: Dp        // Android 56.dp / iOS 52.dp / 桌面 32.dp
    val itemIcon: Dp    // Android 24.dp / iOS 22.dp / 桌面 20.dp
    @Deprecated("Use SaltTheme.shape.medium instead.")       val corner: Dp        // 12.dp
    @Deprecated("Use SaltTheme.shape.large instead.")        val dialogCorner: Dp  // 20.dp
    val padding: Dp     // 16.dp
    val subPadding: Dp  // 12.dp
    fun copy(...): SaltDimens
    companion object {
        @UnstableSaltUiApi val RoundedColumnInListEdgePadding = 5.dp
        fun default(item: Dp = Item, itemIcon: Dp = ItemIcon, padding: Dp = 16.dp, subPadding: Dp = 12.dp): SaltDimens
        @Deprecated fun default(item, itemIcon, corner = 12.dp, dialogCorner = 20.dp, padding = 16.dp, subPadding = 12.dp): SaltDimens
    }
}
```

其他：`SaltConfigs(isDarkTheme: Boolean, indication: Indication)`（`SaltConfigs.kt:41-71`，`default(isDarkTheme = false, indication = AlphaIndication)`）、`SaltMaterial(type: MaterialType)`（`SaltMaterial.kt:30-48`，默认 `MaterialType.None`）。

## S-3.3 可复用上层组件签名（25 条，全部源码抄录）

| # | 组件 | 源码位置 | 签名 |
|---|---|---|---|
| 1 | `BasicScreen`（带回退按钮） | `screen/BasicScreen.kt:98-126` | `@UnstableSaltUiApi @Composable fun BasicScreen(onBack: () -> Unit, modifier: Modifier = Modifier, title: String? = null, subtitle: String? = null, toolButtons: (@Composable () -> Unit)? = null, contentPadding: PaddingValues = BasicScreenDefaults.ContentPadding, overlay: @Composable (BoxScope.(PaddingValues) -> Unit)? = null, style: BasicScreenStyle = SaltTheme.basicScreenStyle, content: @Composable BoxScope.(PaddingValues) -> Unit)` |
| 2 | `BasicScreen`（自定义前导） | `screen/BasicScreen.kt:144-157` | `fun BasicScreen(actionButton: (@Composable () -> Unit)?, modifier = Modifier, title: String? = null, subtitle: String? = null, toolButtons: (@Composable () -> Unit)? = null, contentPadding = BasicScreenDefaults.ContentPadding, overlay = null, style = SaltTheme.basicScreenStyle, content: @Composable BoxScope.(PaddingValues) -> Unit)` |
| 3 | `BasicScreenDefaults` | `:588-624` | `internal val TitleBarHeight = 56.dp`；`val ContentPadding: PaddingValues @Composable get() = WindowInsets.safeMainIgnoringVisibility.asPaddingValues()`；`fun BackButton(onBack: () -> Unit, modifier = Modifier, enabled = true)` |
| 4 | `BasicScreenStyle` | `:258-331` | `data class BasicScreenStyle(val titleBarBackdropType: TitleBarBackdropType, val titleBarBackdropExtraHeight: Dp = 0.dp)`；`enum TitleBarBackdropType { None, Mask, Progressive }`；`default(...)` |
| 5 | `ScreenCard` | `screen/ScreenCard.kt:65-75` | `@UnstableSaltUiApi @Composable fun ScreenCard(modifier: Modifier = Modifier, shape: Shape = CardDefaults.shape, color: Color = CardDefaults.color, border: BorderStroke? = CardDefaults.border, header: String? = null, footer: String? = null, content: @Composable ColumnScope.() -> Unit)` |
| 6 | `TitleBarButton` | `screen/TitleBarButton.kt:35-60` | `@UnstableSaltUiApi @Composable fun TitleBarButton(onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true, icon: @Composable () -> Unit)`；`TitleBarButtonDefaults.Size` = 桌面 36.dp / 移动 40.dp |
| 7 | `Card` / `CardDefaults` | `Card.kt:50-100` | `fun Card(modifier = Modifier, shape = CardDefaults.shape, color = CardDefaults.color, border: BorderStroke? = CardDefaults.border, content: @Composable ColumnScope.() -> Unit)`；默认 shape = `SaltTheme.shapes.medium`、color = `SaltTheme.colors.subBackground`、border = 1 hairline `stroke` |
| 8 | `Item` | `Item.kt:194-209` | `fun Item(onClick: () -> Unit, text: String, modifier = Modifier, enabled = true, iconPainter: Painter? = null, iconPaddingValues: PaddingValues = PaddingValues(0.dp), iconColor: Color? = SaltTheme.colors.text, textColor: Color = SaltTheme.colors.text, sub: String? = null, subColor: Color = SaltTheme.colors.subText, subContent: (@Composable () -> Unit)? = null, tag: String? = null, arrowType: ItemArrowType = ItemArrowType.Arrow)` |
| 9 | `Item` 家族（同一文件） | `Item.kt` | `ItemTitle(text, modifier)`(:99，已弃用)；`ItemTip(text, modifier)`(:120)；`ItemText`(:150，已弃用)；`ItemSwitcher(state: Boolean, onChange: (Boolean) -> Unit, text: String, modifier = Modifier, enabled = true, iconPainter = null, iconPaddingValues = PaddingValues(0.dp), iconColor = SaltTheme.colors.text, sub: String? = null)`(:294)；`ItemCheck(state, onChange, text, modifier, enabled, sub)`(:453)；`ItemValue(text: String, sub: String, modifier = Modifier)`(:516)；`ItemEdit(text: String, onChange: (String) -> Unit, modifier = Modifier, hint: String? = null, hintColor = SaltTheme.colors.subText, readOnly = false, keyboardOptions = KeyboardOptions.Default, keyboardActions = KeyboardActions.Default, visualTransformation = VisualTransformation.None, actionContent: (@Composable () -> Unit)? = null)`(:557)；`ItemEditPassword(...)`(:597)；`ItemSlider(value: Float, onValueChange: (Float) -> Unit, text: String, modifier, iconPainter, iconPaddingValues, iconColor, sub, enabled = true, valueRange: ClosedFloatingPointRange<Float> = 0f..1f, steps: Int = 0, onValueChangeFinished: (() -> Unit)? = null, interactionSource: MutableInteractionSource = remember { MutableInteractionSource() })`(:657)；`ItemButton(onClick, text, modifier, enabled = true, primary = true, iconPainter, iconPaddingValues, iconColor)`(:732)；`ItemSpacer(modifier)`(:787)；`ItemContainer(modifier, content: @Composable BoxScope.() -> Unit)`(:801)；`ItemDivider(modifier, color = SaltTheme.colors.stroke, startIndent: Dp = SaltTheme.dimens.padding)`(:819)；`ItemInfo(text: String, infoType: ItemInfoType, modifier)`(:866)；`ItemLabelValueContainer(modifier, content)`(:934)；`ItemPopup(state: PopupState, text, sub, ...)`(:415，已弃用→`ItemDropdown`) |
| 10 | `ItemOuter` 家族 | `ItemOuter.kt` | `ItemOuterTitle(text: String, modifier: Modifier = Modifier)`(:42)；`ItemOuterTip(text, modifier)`(:68)；`ItemOuterLargeTitle(text: String, sub: String, modifier)`(:89，需要 `@UnstableSaltUiApi`)；`ItemOuterEdit(...)`(:134)；`ItemOuterTextButton(...)`(:167)；`ItemOuterSpacer()`(:201)；`ItemOuterHalfSpacer()`(:213) |
| 11 | `ItemImpl`（内部实现） | `ItemImpl.kt:39-53` | `internal fun ItemEditImpl(text: String, onChange: (String) -> Unit, paddingValues: SaltPaddingValues, modifier: Modifier = Modifier, hint: String? = null, hintColor: Color = SaltTheme.colors.subText, readOnly: Boolean = false, keyboardOptions: KeyboardOptions = KeyboardOptions.Default, keyboardActions: KeyboardActions = KeyboardActions.Default, visualTransformation: VisualTransformation = VisualTransformation.None, actionContent: (@Composable () -> Unit)? = null)` — **`internal`，外部不可调用** |
| 12 | `RoundedColumn` | `RoundedColumn.kt:60-123` | `fun RoundedColumn(modifier: Modifier = Modifier, paddingValues: PaddingValues = PaddingValues(horizontal = SaltTheme.dimens.padding, vertical = SaltTheme.dimens.padding * 0.5f), color: Color = SaltTheme.colors.subBackground, content: @Composable ColumnScope.() -> Unit)`；`@UnstableSaltUiApi fun RoundedColumn(type: RoundedColumnType, modifier = Modifier, color = SaltTheme.colors.subBackground, content: @Composable ColumnScope.() -> Unit)`；`enum RoundedColumnType { Default, InList }` |
| 13 | `LabelValue` | `LabelValue.kt:38-44` | `@UnstableSaltUiApi @Composable fun LabelValue(label: String, value: String, modifier: Modifier = Modifier)`（label 固定 96dp 宽，value 可选中） |
| 14 | `PillButton` | `button/PillButton.kt:63-99` | `@UnstableSaltUiApi @Composable fun PillButton(onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true, text: (@Composable () -> Unit)? = null, size: ControlSize = LocalControlSize.current, icon: @Composable () -> Unit)`；`BasicPillButton(modifier, text, size, icon)`(:112)；`text == null` 时是正圆钮 |
| 15 | `PopupMenu` | `popup/PopupMenu.kt:64-74` | `@UnstableSaltUiApi @Composable fun PopupMenu(expanded: Boolean, onDismissRequest: () -> Unit, modifier: Modifier = Modifier, offset: DpOffset = DpOffset(16.dp, 0.dp), properties: PopupProperties = PopupProperties(focusable = true), backgroundColor: Color = SaltTheme.colors.popup, content: @Composable ColumnScope.() -> Unit)` |
| 16 | `PopupMenuItem` | `popup/PopupMenuItem.kt:54-64` | `@UnstableSaltUiApi @Composable fun PopupMenuItem(onClick: () -> Unit, selected: Boolean? = null, text: String, sub: String? = null, iconPainter: Painter? = null, iconPaddingValues: PaddingValues = PaddingValues(0.dp), iconColor: Color? = SaltTheme.colors.text)` |
| 17 | `PopupState` | `popup/PopupState.kt:30-64` | `@Composable fun rememberPopupState(initialExpend: Boolean = false): PopupState`；`@Stable class PopupState(initialExpend: Boolean) { var expend; fun expend(); fun dismiss() }` |
| 18 | `Sheet` | `sheet/Sheet.kt:78-120` | `@UnstableSaltUiApi @Composable fun Sheet(visible: Boolean, onDismissRequest: () -> Unit, modifier: Modifier = Modifier, title: String? = null, properties: SheetProperties = SheetProperties(), actions: @Composable RowScope.() -> Unit = {}, content: @Composable SheetScope.() -> Unit)`；另有带 `state: SheetState` 的重载。**注意注释里的硬约束**：`visible=false` 时调用必须留在组合里才能播完退场动画；同一 window 同时只能有一个 `visible=true` 的 Sheet；`onDismissRequest` 不会自动改 `visible`。`SheetScope : ColumnScope`（`SheetScope.kt:30`），提供 `contentPadding` |
| 19 | `SheetHost` / `SheetHostLayout` | `sheet/internal/SheetHost.kt:75, 98` | `internal expect fun SheetHost(...)`、`internal fun SheetHostLayout(...)` — **`internal`，外部不可调用**；对外只能用 `Sheet(...)` |
| 20 | `Switcher` | `Switcher.kt:66-101` | `@Composable fun Switcher(state: Boolean, modifier: Modifier = Modifier)`（纯指示器，无输入）；`@UnstableSaltUiApi @Composable fun Switcher(state: Boolean, onChange: (Boolean) -> Unit, modifier: Modifier = Modifier)`（自带点击切换） |
| 21 | `Slider` | `Slider.kt:125` | `internal fun Slider(...)` — **`internal`**，公开入口只有 `ItemSlider` |
| 22 | `LazyList` | `lazy/LazyList.kt:61` | `internal fun LazyList(...)` — **`internal`**，公开入口是 `LazyListState`/`rememberLazyListState`（`lazy/LazyListState.kt:79/100/129/154`）。**实际用 `LazyColumn` 即可，无损失** |
| 23 | `SideBar` | `sidebar/SideBar.kt:11-24` | `@UnstableSaltUiApi @Composable fun SideBar(modifier: Modifier = Modifier, content: @Composable () -> Unit)`（固定 `fillMaxHeight` + `width(220.dp)`） |
| 24 | `Dialog` 家族 | `dialog/Dialog.kt` | `YesDialog(onDismissRequest, title: String, content: String, properties: DialogProperties = DialogProperties(usePlatformDefaultWidth = false), confirmText: String = stringResourceConfirm())`(:90)；`YesNoDialog(onDismissRequest, onConfirm, title, content, properties, cancelText, confirmText, drawContent: (@Composable () -> Unit)? = null)`(:128)；`InputDialog(onDismissRequest, onConfirm, onChange: (String) -> Unit, title: String, text: String, properties, hint: String? = null, cancelText, confirmText)`(:171)；`BasicDialog(...)`(:231)；`DialogTitle(...)`(:314)；`DialogDefaults`(:257) |
| 25 | `SaltApplication` / 平台工具 | `androidMain/app/SaltApplication.kt`、`androidMain/util/SystemUtil.kt`、`androidMain/ext/{ActivityExt,ContextExt,WindowInsetsExt}.kt`、`androidMain/dialog/BottomSheetDialog.kt` | `class App : SaltApplication()`（样例 `androidApp/src/main/kotlin/.../App.kt:22`）；`com.moriafly.salt.ui.util.WindowUtil.setStatusBarForegroundColor(...)`、`com.moriafly.salt.ui.ext.edgeToEdge()`（样例 `MainActivity.kt:38-69`） |

> 统计：**25 条签名**（其中 3 条标记 `internal` 不可用：`ItemImpl`、`SheetHost`、`Slider`、`LazyList` 共 4 条）。

## S-3.4 A/B/C 三路径：可行性证据 + 代价

### 路径 A：`include(":ui2")` 子工程直挂

**可行性证据**
- `settings.gradle.kts` 需要 `include(":core")`、`include(":salt-adaptive-window")`、`include(":ui2")`（`ui2` 用 `api(project(":core"))`、`api(project(":salt-adaptive-window"))`，见 `ui2/build.gradle.kts:176-177`，缺一个都编不过）。
- 根 `build.gradle.kts` 依赖 `extra["configureAppleTargets"]` 与 `extra["signPublications"]`（`ui2/build.gradle.kts:35`、:111），必须整份复制根构建脚本里的计算逻辑。
- 插件版本硬绑：`com.android.kotlin.multiplatform.library 9.1.1`、`org.jetbrains.kotlin.multiplatform 2.4.20`、`org.jetbrains.compose 1.12.2`、`com.vanniktech.maven.publish 0.36.0`、`kover 0.9.8`（`libs.versions.toml:4-22`）。
- 插件仓库超出工程现状：`settings.gradle.kts` 需要 `maven.pkg.jetbrains.space`、`jitpack.io`、`packages.jetbrains.team` 等 4 个额外仓库。
- 编译目标：`compileSdk = 37`、`jvmTarget = JVM_21`、`minSdk = 23`（`ui2/build.gradle.kts:128-137`）。
- Gradle 必须 ≥9.3.1（`SaltUI/gradle/wrapper/gradle-wrapper.properties` → `gradle-9.3.1-bin.zip`）。

**代价**：复制 **3 个模块源码 = `ui2/src` 386 个 `.kt` + `core` 10 个 + `salt-adaptive-window` 6 个（合计 402 个 .kt）**；改 **6 个 Gradle 文件**（根 `build.gradle.kts`、根 `settings.gradle.kts`、`gradle/libs.versions.toml`、`core/build.gradle.kts`、`salt-adaptive-window/build.gradle.kts`、`ui2/build.gradle.kts`）；**并升级本工程工具链**：Gradle 8.13→9.3.1、AGP 8.9.1→9.1.1、Kotlin 2.1.20→2.4.20、compileSdk 35→**37**（需额外下载 `platforms;android-37`，本机 SDK 只装了 35）。传递依赖新增 `haze*3`、`graphics-shapes`、`collection`、`hiddenapibypass`。**结论：技术上可行，代价最高，且会把 task-A 的「可复现构建」整体推倒重来——不建议。**

### 路径 B：`implementation` 发布物

**Maven Central 真实查询结果（全部本机 curl，保留原始状态码）**

| 查询 URL | HTTP | 结果摘要 |
|---|---|---|
| `https://repo1.maven.org/maven2/io/github/moriafly/salt-ui/maven-metadata.xml` | **200** | `latest=3.0.0-beta03`、`release=3.0.0-beta03`、`lastUpdated=20261022013803`；共 **126** 个版本（从 `2.0.0-alpha01` 到 `3.0.0-beta03`，其中非 alpha/beta/rc/dev 的正式版 31 个） |
| `https://repo1.maven.org/maven2/io/github/moriafly/salt-ui/3.0.0-beta03/salt-ui-3.0.0-beta03.pom` | **200** | 依赖 `salt-core:3.0.0-beta03`、`salt-adaptive-window:3.0.0-beta03`、`kotlin-stdlib:2.4.20`、`org.jetbrains.compose.{runtime,foundation,ui,ui-tooling-preview}:1.12.2`、`components-resources:1.12.2`、`haze*:2.0.1`、`collection:1.5.0`、`graphics-shapes:1.0.1` |
| `https://repo1.maven.org/maven2/io/github/moriafly/salt-ui-android/3.0.0-beta03/salt-ui-android-3.0.0-beta03.pom` | **200** | `packaging=aar`；`kotlin-stdlib:2.4.20`；`core-ktx:1.17.0`；`material:1.13.0`；`material3-android:1.4.0`；`activity-compose:1.12.2`；`hiddenapibypass:6.1` |
| `.../salt-ui-android/3.0.0-beta03/salt-ui-android-3.0.0-beta03.module` | **200** | `createdBy.gradle = 9.3.1`；变体 `androidApiElements-published` / `androidRuntimeElements-published`（新 KMP-Android 插件命名） |
| `.../salt-ui-android/3.0.0-beta03/salt-ui-android-3.0.0-beta03.aar` | **200** | 2,349,879 字节；`classes.jar` 的 `SaltTheme` 类文件 **major version 65（Java 21）**，Kotlin `@Metadata mv=[2,4,0]` |
| `.../salt-ui-android/2.5.2/salt-ui-android-2.5.2.pom` | **200** | `packaging=aar`；`kotlin-stdlib:2.1.21`；`core-ktx:1.9.0`；`material:1.12.0`；`material3-android:1.2.1`；`activity-compose:1.9.2`；`compose runtime/foundation/ui`:1.8.2；`components-resources-android:1.8.2`；`components-ui-tooling-preview-android:1.8.2`；`hiddenapibypass:6.1` |
| `.../salt-ui-android/2.5.2/salt-ui-android-2.5.2.module` | **200** | `createdBy.gradle = 8.11.1`；变体 `releaseApiElements-published` / `releaseRuntimeElements-published`（AGP 8 老式命名） |
| `.../salt-ui-android/2.5.2/salt-ui-android-2.5.2.aar` | **200** | 526,661 字节；`SaltColors` 类文件 **major version 65**，`@Metadata mv=[2,1,0]` |
| `.../salt-core-android/2.5.2/salt-core-android-2.5.2.pom` | **200** | `kotlin-stdlib:2.1.21`、`core-ktx:1.9.0`、`hiddenapibypass:6.1` |
| `https://repo1.maven.org/maven2/io/github/moriafly/`（目录） | **200** | 存在 `salt-ui`、`salt-ui-android`、`salt-core`、`salt-adaptive-window`、`salt-ui-navigation`、`salt-audiotag` 及其平台化子件 |

**与本脚手架的版本冲突（硬证据）**：

| 项 | 本工程（`build.gradle.kts` / `app/build.gradle.kts` / `gradle/wrapper/gradle-wrapper.properties`） | salt-ui 3.0.0-beta03 | salt-ui 2.5.2 |
|---|---|---|---|
| Gradle | **8.13**（`gradle-wrapper.properties` → `distributionUrl=https\://mirrors.cloud.tencent.com/gradle/gradle-8.13-bin.zip`，与本任务书推荐值一致） | 需 9.3.1（`SaltUI/gradle/wrapper/gradle-wrapper.properties`） | 发布时用 8.11.1（`.module` → `createdBy.gradle=8.11.1`） |
| AGP | 8.9.1（`build.gradle.kts:2` → `com.android.application` version `8.9.1`） | 9.1.1（`libs.versions.toml:4`） | 8.x 可消费（`.module` 为 AGP 8 老式变体名） |
| Kotlin | 2.1.20 | 2.4.20（`@Metadata mv=[2,4,0]`） | 2.1.21（`mv=[2,1,0]`） |
| Compose | BOM 2025.04.01（ui 1.8.0 / m3 1.3.2） | `org.jetbrains.compose 1.12.2` + `material3 1.4.0` | `1.8.2` + `material3 1.2.1` |
| compileSdk | 35 | 37 | ≤35（2.x AAR） |
| jvmTarget | 17 | 21 | 21（类文件 major 65） |
| 额外依赖 | — | haze×3、graphics-shapes、collection、hiddenapibypass | hiddenapibypass、components-resources/ui-tooling-preview |

**判定**：
- `3.0.0-beta03` **不可用**：`@Metadata mv=[2,4,0]` 无法被 Kotlin 2.1.20 编译器读取（"class was compiled with an incompatible version of Kotlin"），且 2.x 的 IDE/构建链一致性问题无解。
- `2.5.2` **可用，但 API 是旧版**。用 `javap` 实测 `salt-ui-android:2.5.2` 的类清单，**缺失**：`BasicScreen*`、`ScreenCard*`、`TitleBarButton*`、`PillButton*`、`Sheet*`、`SideBar*`；`SaltColors` 只有 8 个 getter（无 `success/warning/error`）；`SaltTextStyles` 只有 3 字段（无 `largeTitle`）；`SaltTheme(SaltConfigs, SaltDynamicColors, SaltTextStyles, SaltDimens, SaltShapes, content)` 没有 `material/basicScreenStyle` 参数。**存在**：`Item*`、`ItemOuter*`、`RoundedColumn`、`LabelValue`、`Switcher`、`PopupMenu`、`PopupMenuItem`、`Card`、`Dialog*`、`SaltPalette`（12 个色）。

**代价（2.5.2）**：改 **1 个文件 1 行**（`app/build.gradle.kts` 加 `implementation("io.github.moriafly:salt-ui:2.5.2")`；建议同时把 `compileOptions`/`jvmTarget` 提到 21，见「施工者须知 R2」）。传递依赖 **12 个直接件**（含 3 个 `org.jetbrains.compose.*-android` + `components-resources-android` + `components-ui-tooling-preview-android` + `material` + `material3-android` + `core-ktx` + `activity-compose` + `hiddenapibypass` + `salt-core-android` + `kotlin-stdlib`），**AAR 总计约 0.6 MB**。

### 路径 C：放弃 SaltUI，纯 Material3 复刻观感

**可行性证据**
- Web 端本身就是 Material Design 3 配色 + MD3 组件语义（`--md-sys-color-*` 命名、胶囊按钮、chips、状态卡），**用 M3 复刻是"回到原设计"而不是"降级"**。
- 本工程已有 `androidx.compose.material3`（BOM 2025.04.01）、`ui`、`ui-graphics`、`ui-tooling-preview`，**零新增依赖、零版本风险**。
- SaltUI 的视觉语言偏 iOS（`SaltColors` 只有 highlight/subText/subBackground；`RoundedColumn` 是 iOS 分组列表；`Switcher` 是 iOS 开关），与 Web 的 MD3 观感本就不同源。

**必须复刻的视觉特征（9 条）**：
1. 胶囊按钮（radius 100px / `RoundedCornerShape(50)`）+ primary 填充 + `0 2px 8px rgba(0,97,164,.3)` 阴影
2. 卡片三档圆角：20/16/24
3. 状态 4 格网格（桌面 `auto-fit 200px`，手机固定 2×2，gap 16→12）
4. 12px 大写 + `letter-spacing .5px` 的小标签（`.status-label` / `.file-info-title`）
5. 状态灯 12×12 圆 + 下载中脉冲动画
6. 库列表 6 列 grid（桌面）/ 4 列 grid（手机），偶数行 `rgba(0,0,0,.015)` 斑马纹
7. hover 变播放按钮（手机改为常显操作列）
8. 4 种 Toast 语义色 + 左侧 4px 强调条 + 半透明背景（`rgba(...,0.95)` + `backdrop-filter: blur(10px)`）
9. 歌词行 `.active`：primary 文字 + 600 + primary-container 底 + 4px 圆角

**代价**：新增 **0 个依赖**，0 个文件改动；需要自建 **~10 个 Composable**（规格已在 S-4 给出签名）。

## S-3.5 推荐结论

**推荐路径 B（`implementation("io.github.moriafly:salt-ui:2.5.2")`）作为「基于 SaltUI 架构」的落地方式，MD3 视觉由 Material3 承担，SaltUI 3.0 独有的 5 个组件（`BasicScreen/ScreenCard/TitleBarButton/PillButton/Sheet`）用 Material3 自建。**

理由（按权重）：
1. **零工具链风险**：2.5.2 是唯一与 Kotlin 2.1.20（`mv=[2,1,0]`）和 Compose 1.8.x 兼容的发布物，一行依赖即可，不动 Gradle/AGP/Kotlin/compileSdk。
2. **满足「基于 SaltUI 架构」**：分组卡片 `RoundedColumn`、`Item*` 行、`ItemOuterTitle`、`Switcher`、`PopupMenu`、`Dialog` 都在 2.5.2 里，设置页与列表页可直接按 Salt 范式搭；主题入口仍是 `SaltTheme { }`。
3. **不会跑偏观感**：Web 的 MD3 颜色与胶囊按钮本来就不是 SaltUI 的强项，用 M3 复刻反而更贴原图；SaltUI 只提供骨架。
4. **A 路径不可行**：AGP 9.1.1 + Gradle 9.3.1 + compileSdk 37 会把 task-A 的脚手架整体推翻，且本机只装到 `platforms;android-35`。
5. **保留退路**：若评审要求"必须 1:1 使用 SaltUI 3.0 组件"，则只能走 A + 全量升级；若要求"零额外依赖"，则走 C。两者都不如 B 划算，但施工者若在 B 上踩到「1:1 还原不了 SaltUI 外观」的争议，可随时退到 C（M3 部分本来就是 C 的内容）。

**不建议的折中**：不要试图把 `3.0.0-beta03` 的 AAR 强行塞进 Kotlin 2.1.20 工程（`metadata version 2.4.0` 会直接编译失败），也不要只拷 `ui2` 源码而丢掉 `core`/`salt-adaptive-window`（`api(project(...))` 会断）。

---

# S-4 Compose 施工规格

## S-4.1 应用骨架

```
NarMusicApp
└── SaltTheme(configs = SaltConfigs.default(isDarkTheme = dark)) {        // 或纯 MaterialTheme（路径 C）
    Scaffold(
        topBar = { NarTopBar(title, searchText, onSearch, onSearchSubmit) },
        bottomBar = { NarBottomBar(currentTab, onTabSelected) }
    ) { padding ->
        NavHost / when(currentTab) {
            Download -> DownloadScreen(...)
            Batch    -> BatchScreen(...)
            Library  -> LibraryScreen(...)
            Settings -> SettingsScreen(...)
        }
        PlayerSheet(visible = player.visible, ...)                          // 覆盖式
        NarToastHost(toastState)                                            // 全局
    }
}
```

### 为什么从 Web 的左侧栏改成底部导航

1. **形态决定**：Web 桌面是 `grid 260px + 1fr`（`style.css:41-45`），手机 CSS 已自己把侧栏改成**顶部横向**（`style.css:1036-1052`，`flex-direction:row` + 横向滚动 + `border-bottom`）。说明原设计者已认定"竖屏不适合侧栏"。
2. **可达性**：手机单手拇指热区在屏幕下 1/3；底部 4 项 Material3 `NavigationBar` 高 80dp、每项宽 ≥48dp，比顶部横向滚动条（web 移动端 4 项 + 品牌共 5 个元素挤在 44–56px 高的一行里、还要横向滚动）命中率显著更高。
3. **回退成本**：`switchTabByName('download')` 这类"跨 Tab 跳转"在底部导航里天然可见（4 个标签一直显示），在横向滚动栏里目标 Tab 可能被滚出屏幕。
4. **M3 规范一致**：`NavigationBar` + `NavigationBarItem` 自带 selected indicator（=`primaryContainer` 椭圆底），正好等于 Web `.nav-item.active { background: primary-container; color: on-primary-container }` 的语义，视觉可直接对齐。
5. **品牌位**：Web 的 `.brand`（纳儿音乐 + music_note）在手机端没有位置，移到顶部大标题栏左侧，作为 `TopAppBar` 的 leading icon + 标题。

补充规格：
- `NavigationBar` 4 项：`下载`（Icons.Rounded.Download）/ `批量下载`（PlaylistAdd）/ `库`（LibraryMusic）/ `设置`（Settings），`label` 常显（对应 Web 图标 + 文字双排）。
- 选中态：`NavigationBarItemDefaults.colors(indicatorColor = colorScheme.primaryContainer, selectedTextColor = colorScheme.onPrimaryContainer, selectedIconColor = colorScheme.onPrimaryContainer)`。
- 顶部大标题栏：`LargeTopAppBar` 风格——展开时标题 22–24sp（对齐 `.section-title` 手机 22px），滚动后收成 20sp；右侧 40dp 圆形 logo（对应 `.user-profile 36×36`）。
- 搜索栏：`topBar` 下方一条高 44dp、圆角 100、底色 `background` 的 `TextField`，`singleLine`，`placeholder = "模糊搜索歌曲、歌手..."`，trailing 为 `search` 图标（点击 = `onSearchSubmit`），`keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search)`。

## S-4.2 逐屏 Composable 签名（伪代码）

```kotlin
// ───────── 骨架 ─────────
@Composable fun NarMusicApp()

@Composable
fun NarTopBar(
    title: String,
    searchText: String,
    onSearchTextChange: (String) -> Unit,
    searchEnabled: Boolean,
    onSearchSubmit: () -> Unit,
    onLogoTap: () -> Unit
)

@Composable
fun NarBottomBar(
    current: NarTab,
    onTabSelected: (NarTab) -> Unit
)
enum class NarTab { Download, Batch, Library, Settings }

@Composable
fun NarToastHost(state: ToastState)          // 4 种类型：Info / Success / Warning / Error

// ───────── 下载页 ─────────
data class DownloadUiState(
    val urlText: String = "",
    val filenameText: String = "",
    val cardVisible: Boolean = false,
    val light: StatusLight = StatusLight.Idle,      // Idle / Downloading / Success / Error
    val taskStatusText: String = "等待中",
    val downloadedText: String = "0 B",
    val speedText: String = "0 B/s",
    val timeText: String = "-",
    val statusText: String = "-",
    val fileInfoVisible: Boolean = false,
    val fileName: String = "-",
    val fileSizeText: String = "-",
    val sendBusy: Boolean = false,
    val localButtonVisible: Boolean = false,
    val localBusy: Boolean = false,
    val localProgress: String? = null               // "取消 下载中 42%"
)

@Composable
fun DownloadScreen(
    state: DownloadUiState,
    onUrlChange: (String) -> Unit,
    onFilenameChange: (String) -> Unit,
    onStart: () -> Unit,                            // 「开始下载」
    onPickLocalDownload: () -> Unit,                // 「保存到本地」
    onCancelLocalDownload: () -> Unit
)

@Composable
fun StatusCard(
    light: StatusLight,
    taskStatusText: String,
    downloaded: String,
    speed: String,
    time: String,
    status: String,
    fileInfoVisible: Boolean,
    fileName: String,
    fileSize: String
)

@Composable
fun StatusGridCell(label: String, value: String)    // 12sp uppercase label + 18sp/600 tabular value

@Composable
fun FileInfoBlock(fileName: String, fileSize: String)

// ───────── 批量下载页 ─────────
data class BatchUiState(
    val inputMode: BatchInputMode = BatchInputMode.Paste,   // Paste / Import
    val pasteText: String = "",
    val importedFileName: String? = null,
    val parsing: Boolean = false,
    val progressHeader: String = "正在解析歌单...",
    val progressFraction: Float = 0f,
    val progressStats: String = "0 / 0",
    val resultsVisible: Boolean = false,
    val totalCount: Int = 0,
    val foundCount: Int = 0,
    val notFoundCount: Int = 0,
    val items: List<BatchItem> = emptyList(),
    val selectedIndices: Set<Int> = emptySet(),
    val downloading: Boolean = false
)
data class BatchItem(
    val index: Int, val title: String, val artistInfo: String,
    val status: BatchItemStatus,                    // Found / NotFound / Downloading / Downloaded / Failed
    val exact: Boolean                              // narmeta 行 → 显示「精确匹配」
)
enum class BatchItemStatus { Found, NotFound, Downloading, Downloaded, Failed }

@Composable
fun BatchScreen(
    state: BatchUiState,
    onModeChange: (BatchInputMode) -> Unit,
    onPasteChange: (String) -> Unit,
    onPickFile: () -> Unit,                         // 系统文件选择器（替代 HTML file input + 拖拽）
    onRemoveFile: () -> Unit,
    onParse: () -> Unit,
    onClear: () -> Unit,
    onToggleSelectAll: (Boolean) -> Unit,
    onToggleSelect: (Int, Boolean) -> Unit,
    onDeleteSelected: () -> Unit,
    onDownloadToServer: () -> Unit,
    onDeleteItem: (Int) -> Unit
)

@Composable fun BatchModeTabs(mode: BatchInputMode, onChange: (BatchInputMode) -> Unit)
@Composable fun BatchFileDropZone(
    fileName: String?,
    onPick: () -> Unit,
    onRemove: () -> Unit
)   // 手机无拖拽：默认文案「点击或拖拽文件到此处」+「支持 .txt 文件」，点击起 SAF
@Composable fun BatchParseProgress(header: String, fraction: Float, stats: String)
@Composable fun BatchResultSummary(total: Int, found: Int, notFound: Int)
@Composable fun BatchEditToolbar(selectedCount: Int, onToggleSelectAll: (Boolean) -> Unit, onDownload: () -> Unit, onDelete: () -> Unit)
@Composable fun BatchItemRow(item: BatchItem, selected: Boolean, onSelect: (Boolean) -> Unit, onDelete: () -> Unit)
@Composable fun BatchEmptyState()                   // 「暂无搜索结果」

// ───────── 库页 ─────────
data class LibraryUiState(
    val loading: Boolean = false,
    val error: String? = null,
    val rawEmpty: Boolean = false,                  // 服务端 0 条
    val filteredEmpty: Boolean = false,             // 有数据但过滤后 0 条
    val keyword: String = "",
    val filter: LibraryFilter = LibraryFilter.All,
    val visibleCount: Int = 0,
    val items: List<LibraryItem> = emptyList(),
    val batchMode: Boolean = false,
    val selectedIds: Set<Long> = emptySet(),
    val downloadingIds: Set<Long> = emptySet(),
    val downloadedIds: Set<Long> = emptySet(),
    val batchDownloading: Boolean = false
)
data class LibraryItem(
    val id: Long, val title: String, val systemFilename: String,
    val delayMs: Int, val sizeText: String, val timeText: String
)
enum class LibraryFilter { All, NameAsc, NameDesc, TimeNew, TimeOld, SizeLarge, SizeSmall }

@Composable
fun LibraryScreen(
    state: LibraryUiState,
    onKeywordChange: (String) -> Unit,
    onClearKeyword: () -> Unit,
    onFilterChange: (LibraryFilter) -> Unit,
    onRefresh: () -> Unit,
    onToggleBatchMode: () -> Unit,
    onToggleSelect: (Long, Boolean) -> Unit,
    onToggleSelectAll: (Boolean) -> Unit,
    onPlay: (LibraryItem) -> Unit,
    onDownloadOne: (LibraryItem) -> Unit,
    onDeleteOne: (LibraryItem) -> Unit,
    onBatchDownload: () -> Unit,
    onBatchGeneratePlaylist: () -> Unit,
    onBatchDelete: () -> Unit
)

@Composable fun LibraryToolbar(state: LibraryUiState, callbacks: LibraryCallbacks)
@Composable fun LibrarySearchField(keyword: String, onChange: (String) -> Unit, onClear: () -> Unit)
@Composable fun LibraryFilterChips(active: LibraryFilter, onChange: (LibraryFilter) -> Unit)
@Composable fun LibraryBatchToolbar(...)
@Composable fun TrackRow(
    item: LibraryItem,
    indexNumber: String,                            // "01"
    selected: Boolean,
    batchMode: Boolean,
    downloading: Boolean,
    downloaded: Boolean,
    progressPercent: Int?,
    onPlay: () -> Unit,
    onDownload: () -> Unit,
    onDelete: () -> Unit,
    onSelect: (Boolean) -> Unit
)
@Composable fun LibraryStateView(state: LibraryUiState, onRetry: () -> Unit)   // 加载/空/错误 三态
@Composable fun ProtectionDialog(
    visible: Boolean,
    password: String,
    errorVisible: Boolean,
    onPasswordChange: (String) -> Unit,
    onConfirm: () -> Unit,
    onCancel: () -> Unit
)

// ───────── 设置页 ─────────
data class SettingsUiState(
    val platform: LyricsPlatform = LyricsPlatform.Netease,
    val delayText: String = "0",
    val delayError: Boolean = false
)
enum class LyricsPlatform(val label: String) {
    Netease("网易云音乐"), Kugou("酷狗音乐"), QQ("QQ音乐"), Soda("汽水音乐")
}

@Composable
fun SettingsScreen(
    state: SettingsUiState,
    onPlatformChange: (LyricsPlatform) -> Unit,
    onDelayChange: (String) -> Unit,
    onSave: () -> Unit,
    onReset: () -> Unit
)
@Composable fun SettingsDownloadCard(state: SettingsUiState, onPlatformChange: (LyricsPlatform) -> Unit, onDelayChange: (String) -> Unit, onSave: () -> Unit, onReset: () -> Unit)
@Composable fun AboutCard()

// ───────── 播放器 ─────────
data class PlayerUiState(
    val visible: Boolean = false,
    val title: String = "未在播放",
    val artist: String = "纳儿音乐",
    val isPlaying: Boolean = false,
    val positionMs: Long = 0,
    val durationMs: Long = 0,
    val lyrics: List<LyricLine> = emptyList(),      // 空 → 「暂无歌词」
    val activeLyricIndex: Int = -1
)
data class LyricLine(val timeMs: Long, val text: String)

@Composable
fun PlayerSheet(
    state: PlayerUiState,
    onDismiss: () -> Unit,
    onPlayPause: () -> Unit,
    onSeek: (Long) -> Unit,
    onSeekBack: () -> Unit,
    onSeekForward: () -> Unit
)   // 文档内说明：Web 是 320px 可拖动浮窗；手机改为 ModalBottomSheet（见施工者须知 C3）
@Composable fun PlayerTrackInfo(title: String, artist: String)
@Composable fun PlayerControls(isPlaying: Boolean, positionMs: Long, durationMs: Long, onPlayPause: () -> Unit, onSeek: (Long) -> Unit)
@Composable fun LyricsScroller(lyrics: List<LyricLine>, activeIndex: Int, onLineClick: (Int) -> Unit)
```

## S-4.3 每屏「加载中 / 空态 / 错误态」

文案全部照抄 S-1.8，禁止改写。

| 屏幕 | 加载中 | 空态 | 错误态 |
|---|---|---|---|
| 下载（`DownloadScreen`） | 按钮忙碌：`发送中...` / `准备中...` / `下载中 {n}%`；状态卡 `#taskStatus = 下载中`、状态灯 = warning（脉冲） | **无独立空态**：`statusCardVisible=false` 时整卡不渲染，页面只剩输入区。状态灯 `idle`(#B0B0B0) 时卡内文字 `等待中` | 状态灯 = error；`#taskStatus = 下载未完成` 或 `问题: {error}`；`#downloadStatus = 未完成`；Toast `下载未完成: {error}` |
| 批量（`BatchScreen`） | `#batchParseProgress` 可见：header `正在解析歌单...` → `正在批量搜索歌单...`；线性进度 + `{done} / {total}` | 结果区隐藏；若已渲染列表为空则显示 `暂无搜索结果`（图标 `search_off` 48px） | 单项徽标 `失败`（error 底）；Toast `批量搜索失败: {msg}` / `所有歌曲下载提交失败` / `状态查询多次失败，已停止轮询` |
| 库（`LibraryScreen`） | `加载音乐库中...`（`refresh` 图标旋转）+ 刷新按钮文案 `加载中...`、禁用 | 服务端 0 条：`音乐库为空` / `前往下载页面添加音乐`（`library_music` 图标）；过滤后 0 条：`未找到匹配的音乐` / `尝试其他关键词或筛选条件`（`search_off` 图标） | `网络连接失败` / `请检查服务器是否运行`（`wifi_off` 图标）+ Toast `加载失败: {msg}`；建议加「重试」按钮（Web 无，标注为**新增**） |
| 设置（`SettingsScreen`） | 无（同步读本地） | 无 | 延迟非法：输入框下方辅助文本 + Toast `延迟参数必须是整数` |
| 播放器（`PlayerSheet`） | `正在加载音乐...` / `正在加载歌词...` | 歌词区 `暂无歌词` | 歌词失败 4 种文案之一（`歌词加载失败` / `歌词库加载失败，请刷新页面` / `服务器请求失败: {err}` / `音频文件读取失败` / `歌词数据不完整，尝试重新加载`）；标题兜底 `未知歌曲` |

## S-4.4 主题映射表

### 浅色（直接映射 Web 变量）

| Web 变量 | 值 | Compose `ColorScheme` 字段 | `Color` 字面量 |
|---|---|---|---|
| `--md-sys-color-primary` | `#2D7BC1` | `primary` | `Color(0xFF2D7BC1)` |
| —（Web 用 `white`） | `#FFFFFF` | `onPrimary` | `Color(0xFFFFFFFF)` |
| `--md-sys-color-primary-container` | `#E3F0FF` | `primaryContainer` | `Color(0xFFE3F0FF)` |
| `--md-sys-color-on-primary-container` | `#002A4D` | `onPrimaryContainer` | `Color(0xFF002A4D)` |
| `--md-sys-color-secondary` | `#6B7685` | `secondary` | `Color(0xFF6B7685)` |
| —（推导：on-surface） | `#1A1A1A` | `onSecondary` | `Color(0xFF1A1A1A)` |
| `--md-sys-color-secondary-container` | `#E9EDF5` | `secondaryContainer` | `Color(0xFFE9EDF5)` |
| —（推导：on-surface） | `#1A1A1A` | `onSecondaryContainer` | `Color(0xFF1A1A1A)` |
| —（M3 需要，Web 无 `tertiary` 定义） | `#7D5260`（取 CSS fallback） | `tertiary` | `Color(0xFF7D5260)` |
| —（推导） | `#FFFFFF` | `onTertiary` | `Color(0xFFFFFFFF)` |
| —（M3 需要） | `#FFD8E4` | `tertiaryContainer` | `Color(0xFFFFD8E4)` |
| —（M3 需要） | `#31111D` | `onTertiaryContainer` | `Color(0xFF31111D)` |
| `--md-sys-color-error` | `#C62828` | `error` | `Color(0xFFC62828)` |
| —（推导 `white`） | `#FFFFFF` | `onError` | `Color(0xFFFFFFFF)` |
| `--md-sys-color-error-container` | `#FFEBEE` | `errorContainer` | `Color(0xFFFFEBEE)` |
| —（推导 `error`） | `#C62828` | `onErrorContainer` | `Color(0xFFC62828)` |
| `--md-sys-color-background` | `#F8FAFD` | `background` | `Color(0xFFF8FAFD)` |
| `--md-sys-color-on-surface` | `#1A1A1A` | `onBackground` | `Color(0xFF1A1A1A)` |
| `--md-sys-color-surface` | `#FFFFFF` | `surface` | `Color(0xFFFFFFFF)` |
| `--md-sys-color-on-surface` | `#1A1A1A` | `onSurface` | `Color(0xFF1A1A1A)` |
| `--md-sys-color-surface-variant` | `#E8EAED` | `surfaceVariant` | `Color(0xFFE8EAED)` |
| `--md-sys-color-on-surface-variant` | `#5A5D63` | `onSurfaceVariant` | `Color(0xFF5A5D63)` |
| —（CSS fallback `--md-sys-color-outline`） | `#79747E` | `outline` | `Color(0xFF79747E)` |
| —（推导 `surface-variant`） | `#E8EAED` | `outlineVariant` | `Color(0xFFE8EAED)` |
| —（M3 需要） | `#000000` | `scrim` | `Color(0xFF000000)` |
| —（M3 需要，取 surface 提亮） | `#FFFFFF` | `surfaceContainerLowest` | `Color(0xFFFFFFFF)` |
| — | `#F8FAFD` | `surfaceContainerLow` / `surfaceContainer` | `Color(0xFFF8FAFD)` |
| — | `#F1F4F9` | `surfaceContainerHigh` / `surfaceContainerHighest` | `Color(0xFFF1F4F9)` |
| —（推导） | `#1A1A1A` | `inverseSurface` | `Color(0xFF1A1A1A)` |
| —（推导） | `#F8FAFD` | `inverseOnSurface` | `Color(0xFFF8FAFD)` |
| —（推导） | `#8FB8DA` | `inversePrimary` | `Color(0xFF8FB8DA)` |

### SaltUI 语义色（若走路径 B，需要额外提供）

```kotlin
SaltColors(
    highlight      = Color(0xFF2D7BC1),   // = primary
    text           = Color(0xFF1A1A1A),   // = on-surface
    subText        = Color(0xFF5A5D63),   // = on-surface-variant
    background     = Color(0xFFF8FAFD),   // = background
    subBackground  = Color(0xFFFFFFFF),   // = surface（Web 卡片是白底，不是半透明白）
    popup          = Color(0xFFFFFFFF),
    stroke         = Color(0xFFE8EAED),   // = surface-variant（Web 分隔线就是这个色）
    onHighlight    = Color(0xFFFFFFFF),
    success        = Color(0xFF2E7D32),
    warning        = Color(0xFFF57C00),
    error          = Color(0xFFC62828)
)
SaltDimens.default(item = 56.dp, itemIcon = 24.dp, padding = 16.dp, subPadding = 12.dp)  // 已与 Web 的 16/12 吻合
SaltShapes.default(small = RoundedCornerShape(8.dp), medium = RoundedCornerShape(16.dp), large = RoundedCornerShape(24.dp))  // 也正好对上 Web 的 8/16/24
SaltTextStyles(main = 16.sp, sub = 12.sp, paragraph = 16.sp/1.5em, largeTitle = 24.sp/SemiBold)
```

> **可复用性发现**：SaltUI 的 `SaltShapes`(8/16/24) 与 `SaltDimens`(padding 16 / subPadding 12 / item 56) **正好等于 Web 的圆角与间距体系**；`largeTitle`(24sp SemiBold) **正好等于 `.section-title`**。这是选 B 的最强论据。

### 深色模式（Web 无深色模式，以下全部为「新增」，按 MD3 色阶规则推导）

| 字段 | 深色值 | 推导依据 |
|---|---|---|
| `primary` | `#A3C9F0`（新增） | MD3：primary 取主色相 tone 80；`#2D7BC1` 色相约 210° |
| `onPrimary` | `#0B2F4A`（新增） | tone 20，与 primary 对比度 ≥ 4.5:1 |
| `primaryContainer` | `#00456F`（新增） | tone 30 |
| `onPrimaryContainer` | `#CFE5FF`（新增） | tone 90 |
| `secondary` | `#B9C3D1`（新增） | 中性色 tone 80 |
| `secondaryContainer` | `#3C4757`（新增） | tone 30 |
| `onSecondaryContainer` | `#D5E3F7`（新增） | tone 90 |
| `background` | `#121417`（新增） | 中性 tone 6 |
| `onBackground` | `#E3E6EA`（新增） | 中性 tone 90 |
| `surface` | `#1A1C1F`（新增） | 中性 tone 10 |
| `onSurface` | `#E3E6EA`（新增） | tone 90 |
| `surfaceVariant` | `#2A2D31`（新增） | tone 20（Web 浅色的 `#E8EAED` 反向） |
| `onSurfaceVariant` | `#B4B8BE`（新增） | tone 80 |
| `outline` | `#8E9196`（新增） | tone 60 |
| `outlineVariant` | `#43474D`（新增） | tone 30 |
| `error` | `#FFB4AB`（新增） | MD3 标准红 tone 80 |
| `errorContainer` | `#93000A`（新增） | tone 30 |
| `onErrorContainer` | `#FFDAD6`（新增） | tone 90 |
| `success` | `#7ED67A`（新增） | `#2E7D32` 色相不变、tone 80 |
| `successContainer` | `#1E4620`（新增） | tone 30 |
| `warning` | `#FFB870`（新增） | `#F57C00` 色相不变、tone 80 |
| `warningContainer` | `#5A2E00`（新增） | tone 30 |
| `surfaceContainerHigh` | `#24272B`（新增） | tone 14 |
| `scrim` | `#000000` | — |

配套：状态灯 `idle #B0B0B0` 深色下改 `#6E7276`（新增）；toast 半透明底色改 `rgba(46,125,50,0.18)` 系列（新增）；`status-bar` 用 `WindowUtil.setStatusBarForegroundColor(window, White)`（SaltUI 2.5.2 无此工具，走 M3 `enableEdgeToEdge` + `WindowInsetsControllerCompat.isAppearanceLightStatusBars`）。

## S-4.5 组件复用清单

| 屏幕元素 | 走路径 B（SaltUI 2.5.2）用什么 | 纯 M3 替代（路径 C 兜底） |
|---|---|---|
| 应用外壳 / 顶栏 | `TitleBarButton`（2.5.2 **无**）→ **M3 `LargeTopAppBar` + `IconButton`** | `LargeTopAppBar` |
| 底部 4 项导航 | **M3 `NavigationBar` + `NavigationBarItem`**（SaltUI 无底部导航；`SideBar` 是 220dp 固定宽侧栏，不适用） | 同左 |
| 页面分组卡片 | `RoundedColumn` / `Card`（有） | `Card`/`Surface` + `RoundedCornerShape(16.dp)` |
| 卡片外分组标题 | `ItemOuterTitle`（有） | `Text(style = labelMedium, color = onSurfaceVariant)` |
| 卡片外说明文字 | `ItemOuterTip`（有） | `Text(style = bodySmall)` |
| 设置项行（平台选择） | `ItemPopup` / `ItemDropdown`（2.5.2 有 `ItemPopup`）→ 弹 `PopupMenu` + `PopupMenuItem` | `ExposedDropdownMenuBox` |
| 设置项行（延迟输入） | `ItemEdit` + `keyboardOptions = Number`（有） | `OutlinedTextField` |
| 开关类设置 | `ItemSwitcher` / `Switcher`（有） | `Switch` + `ListItem` |
| 复选框行 | `ItemCheck`（有） | `Checkbox` + `Row` |
| 主按钮「开始下载」 | **M3 `Button`**（SaltUI 无胶囊主按钮；`PillButton` 2.5.2 无） | `Button(shape = CircleShape)` |
| 次按钮「保存到本地」「刷新」 | **M3 `FilledTonalButton` / `OutlinedButton`** | 同左 |
| 危险按钮「批量删除」 | `ItemButton(primary = false)`（有，但是行样式非按钮样式）→ **M3 `Button(colors = error)`** | `Button(containerColor = error)` |
| 胶囊筛选 chip | **M3 `FilterChip`**（SaltUI 无 chip） | `FilterChip` |
| 文本输入（链接/文件名/歌单） | `ItemEdit`（单行）或 **M3 `OutlinedTextField`**（多行 textarea 必须用 M3） | `OutlinedTextField` |
| 下载状态 4 格 | `ItemValue(text, sub)` × 4 或 `LabelValue`（有）→ 但 Web 是 2×2 网格，**用 `Row`/`Column` + `ItemValue`** | `Card` + `Column` |
| 状态灯 | **自建 `Canvas`/`Box` 圆点 + `rememberInfiniteTransition`**（SaltUI 无） | 同左 |
| 文件信息两行 label/value | `LabelValue`（有，label 固定 96dp） | `Row { Text; Text }` |
| 批量进度条 | **M3 `LinearProgressIndicator`**（SaltUI `Slider` 是 internal，无进度条） | `LinearProgressIndicator` |
| 列表行（库/批量） | `Item` + `ItemDivider`（有），但 6 列网格需自建 `Row` | `ListItem` + 自定义 `Row` |
| 空态 / 错误态 | `ItemOuterLargeTitle(text, sub)`（近似）+ 自建图标位 | 自建 `Column` + `Icon` |
| 播放器弹窗 | `Sheet`（2.5.2 **无**）→ **M3 `ModalBottomSheet`** | `ModalBottomSheet` |
| 播放器控制条 | **M3 `Slider` + `IconButton`**（Web 用的是原生 `<audio controls>`） | 同左 |
| 歌词滚动区 | 自建 `LazyColumn` + `animateScrollToItem` | 同左 |
| 确认对话框 | `YesNoDialog`（有，风格是 iOS 弹窗）→ **M3 `AlertDialog`**（与 Web 的 confirm 语义一致） | `AlertDialog` |
| 密码输入对话框 | `InputDialog`（有）或 **M3 `AlertDialog` + `OutlinedTextField(password)`** | `AlertDialog` |
| 下拉菜单 | `PopupMenu` + `PopupMenuItem` + `rememberPopupState`（有） | `DropdownMenu` |
| Toast | **自建 `NarToastHost`**（4 类语义色，SaltUI 无 Toast） | 同左 |
| 主题入口 | `SaltTheme { }` + `SaltColors/SaltDimens/SaltShapes/SaltTextStyles` 自定义 | `MaterialTheme(colorScheme, typography, shapes)` |

---

# 验收与复现（命令 + 实测输出）

全部在 `E:\桌面\安卓软件\NarMusicAndroid` 下执行。

```bat
:: 1) palette.json 合法性与键数
python -c "import json,re;d=json.load(open('docs/ui/palette.json',encoding='utf-8'));print('JSON OK, keys=',len(d));print('bad=',[k for k,v in d.items() if not re.fullmatch(r'#[0-9A-Fa-f]{6}',v)])"
::   实测 → JSON OK, keys= 26 / bad= []

:: 2) 文案表条目数（锚定到 "^| T### |" 行，避免误命中 "| Toast |" 等行）
python -c "import re;L=open('docs/ui-spec.md',encoding='utf-8').read().splitlines();R=[l for l in L if re.match(r'^\| T\d{3} \|',l)];print('rows=',len(R));print('unique=',len({r.split('|')[2].strip() for r in R}))"
::   实测 → rows= 192 / unique= 189

:: 3) S-3.3 组件签名条目数（在该节内锚定 "^| 数字 | `名字`" 行）
python -c "import re;T=open('docs/ui-spec.md',encoding='utf-8').read();S=T.split('## S-3.3')[1].split('## S-3.4')[0];print('components=',len([l for l in S.splitlines() if re.match(r'^\| \d+ \| [\x60]',l)]))"
::   实测 → components= 25   （S-3.3 表格 1..25）
```

> 注：`findstr /c:"| T"` 这种写法会误命中 `| Toast |` 表格行与本段命令本身，所以上面统一用 Python 正则锚定统计。

Maven 查询（可复跑）：

```bat
curl -s -o NUL -w "%{http_code}\n" https://repo1.maven.org/maven2/io/github/moriafly/salt-ui/maven-metadata.xml
curl -s -o NUL -w "%{http_code}\n" https://repo1.maven.org/maven2/io/github/moriafly/salt-ui/3.0.0-beta03/salt-ui-3.0.0-beta03.pom
curl -s -o NUL -w "%{http_code}\n" https://repo1.maven.org/maven2/io/github/moriafly/salt-ui-android/2.5.2/salt-ui-android-2.5.2.aar
```

Kotlin metadata 校验（可复跑）：

```bat
:: 解出 AAR 的 classes.jar 后
E:\jdk\bin\javap -v -classpath classes.jar com.moriafly.salt.ui.SaltColors
::   2.5.2   → major version: 65 / mv=[2,1,0]   （Kotlin 2.1.20 可读）
E:\jdk\bin\javap -v -classpath classes.jar com.moriafly.salt.ui.SaltTheme
::   3.0.0-beta03 → major version: 65 / mv=[2,4,0]（Kotlin 2.1.20 不可读）
```

### 本工程 Gradle wrapper 现状（R1 自证，2026/11/01 实测）

```bat
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && dir /b
::   %D%
::   .gradle
::   app
::   build.gradle.kts
::   docs
::   gradle
::   gradle.properties
::   gradlew
::   gradlew.bat
::   local.properties
::   settings.gradle.kts
::   → gradlew / gradlew.bat / gradle 三者均存在
```

```bat
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && dir /a /od
::   2026/11/01  11:49               328 settings.gradle.kts
::   2026/11/01  11:49               368 build.gradle.kts
::   2026/11/01  11:49                25 local.properties
::   2026/11/01  11:51             8,618 gradlew
::   2026/11/01  11:51             2,896 gradlew.bat
::   2026/11/01  11:51    <DIR>          gradle
::   2026/11/01  11:52    <DIR>          %D%
::   2026/11/01  11:55               134 gradle.properties
```

```bat
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && dir gradle\wrapper /a
::   2026/11/01  11:51            45,633 gradle-wrapper.jar
::   2026/11/01  11:53               251 gradle-wrapper.properties
```

```bat
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && type gradle\wrapper\gradle-wrapper.properties
::   distributionBase=GRADLE_USER_HOME
::   distributionPath=wrapper/dists
::   distributionUrl=https\://mirrors.cloud.tencent.com/gradle/gradle-8.13-bin.zip
::   networkTimeout=60000
::   validateDistributionUrl=false
::   zipStoreBase=GRADLE_USER_HOME
::   zipStorePath=wrapper/dists
::   → Gradle 8.13，与任务书 ③ 推荐值一致
```

```bat
:: docs/build-versions.md 是否存在的自证
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && dir /b docs
::   api-probe.md
::   probe
::   task-A-env.md
::   task-B-core.md
::   task-C-api-probe.md
::   task-S-ui-spec.md
::   ui
::   ui-spec.md
::   → 无 build-versions.md；故 S-3.4 与「施工者须知 R6/R7」改用
::     build.gradle.kts / app/build.gradle.kts / gradle-wrapper.properties 的真实声明值作对比基线
```

```bat
:: %D% 垃圾目录的内容与体积自证
cd /d "E:\桌面\安卓软件\NarMusicAndroid" && dir /s /a "%D%"
::   2026/11/01  11:52    <DIR>          gradle-8.13
::   2026/11/01  11:52       136,983,045 gradle-8.13-bin.zip
::   2026/11/01  11:52                 0 gradle-8.13-bin.zip.ok
::    所列文件总数:  315 个文件    288,628,175 字节
```

---

# 施工者须知（必读）

## A. Compose 里无法 1:1 还原的细节与取舍建议

| # | Web 实现 | 问题 | 建议取舍 |
|---|---|---|---|
| A1 | `backdrop-filter: blur(10px)` + `rgba(...,0.95)` 半透明 Toast（`style.css:539-553`） | Compose 无跨平台等价物；`Modifier.blur()` 在 API<31 无效且只模糊自身内容 | **改不透明色 + 4dp 阴影**；或用 `Modifier.graphicsLayer{renderEffect}`（API 31+）判版本降级。**不要为此引入 haze** |
| A2 | `Material Symbols Rounded` 字体（FILL 1 可变字重，`style.css:95-97` 用 `font-variation-settings` 做选中态实心） | Compose 用 `ImageVector`，无法切换可变字体轴 | 用 `androidx.compose.material:material-icons-extended`（体积大，约 +3MB 未压缩，会被 R8 裁剪）或**手挑 20 个图标导成 vector drawable**。选中态用「线条版/实心版两个图标」近似 |
| A3 | `box-shadow`（`.btn-primary`、`.audio-popup`、`.toast`、`.main-container`） | Compose 只有 `Modifier.shadow`（单色、无扩散/偏移细调） | primary 按钮阴影降级为 `shadow(2.dp, CircleShape)` 或直接去掉（M3 按钮本身就无阴影）；弹窗阴影用 `tonalElevation`/`shadow(24.dp)` |
| A4 | `@keyframes pulse` 状态灯（`scale(1→1.1)` + opacity） | 可还原，但无限动画在 Compose 里需 `rememberInfiniteTransition` | 可还原；建议只做 alpha 呼吸（避免 layout 抖动），周期 2000ms，`LinearEasing` 近似 `ease-in-out` |
| A5 | 桌面 6 列 grid（`48px 1fr 90px 80px 130px 72px`）+ hover 显形 | 手机宽度 <400dp 放不下 6 列 | **必须按移动端规格做**：`36dp 1fr auto auto`（序号/标题/时间/操作），延迟与大小合并进副标题 `{delay}ms · {size}`（Web 移动端 `style.css:1306-1309` 就是这么做的）。桌面多列只在平板横屏（≥720dp）用 `WindowSizeClass` 分支还原 |
| A6 | hover（`opacity:0 → 1`）显示操作按钮 | 触屏无 hover | 操作按钮**常显**（跟 Web 移动端一致，`style.css:1311-1315`）；`onLongClick` 加菜单可选 |
| A7 | 播放器浮窗可鼠标拖动 + 记忆位置（`localStorage`） | 手机上拖动浮窗是反模式 | 改 `ModalBottomSheet`（可拖动展开/收起，语义更近）；位置记忆功能**丢弃** |
| A8 | `<audio controls>` 原生控制条 | 无等价物，且原生 `<audio>` 是浏览器 UA | 自建：播放/暂停 + 可拖动 `Slider` + 左右时间文本（`mm:ss`，`formatDuration` 规则见 `utils.js:26-31`） |
| A9 | `confirm()` / `alert()` | 系统原生弹窗 | 换 `AlertDialog`；注意文案里已经内嵌了 `\n` 语义时要分段 |
| A10 | 文件拖拽上传（`.batch-file-upload` `dragover`） | 手机无拖拽 | 保留虚线框视觉，点击起 `ActivityResultContracts.OpenDocument(mimeTypes = ["text/plain"])`；文案仍写「点击或拖拽文件到此处」会误导 → **建议改成「点击选择 .txt 文件」（标注为新增改动）**，或保留原文但只在平板横屏显示「拖拽」二字 |
| A11 | `body.preload` 字体加载后显示 | 安卓无字体加载门控 | 用 `SplashScreen` API 或直接去掉；`strings.xml` 首帧加载很快，不必复刻 |
| A12 | 单位换算 | Web 用 px，1 CSS px ≠ 1 dp（一般按 1:1 处理足够） | **按 1px = 1dp** 换算；字号同理（CSS px → sp）。注意 Web 的 `font-size: 14px` 在手机上偏小，**建议对 `.status-label`(12)、`.batch-item-status`(12)、`.music-subtitle`(12) 保持 12sp，正文不小于 12sp，正文主体 14–16sp** |
| A13 | `letter-spacing: 0.5px` + `text-transform: uppercase` 的中文标签 | 中文无大小写，uppercase 对中文无效 | 中文直接忽略 `text-transform`；`letter-spacing` 用 `letterSpacing = 0.5.sp` 保留（对中文有效果） |
| A14 | 归档文件命名 `NarMusic_playlist_yyyyMMdd_HHmmss.txt` | 安卓要写 `MediaStore`/`SAF` | 下载目录 + `MediaStore.Downloads`（API 29+）/ `WRITE_EXTERNAL_STORAGE`（26–28）。**注意 minSdk = 26**，两条分支都要写 |
| A15 | 库页排序的拼音首字母映射（`library.js:178-191` 用 `localeCompare('zh-CN')` 硬凑） | 安卓要真拼音排序 | 用 `java.text.Collator.getInstance(Locale.CHINA)` 直接排，**别照抄那段 anchor 表**（它是给浏览器兜底的 hack） |
| A16 | 表单校验时机 | Web 是「点按钮时才校验」 | 保持一致：`onStart` 里逐个 `if` 抛 Toast，不要做实时红框（除非另有需求） |

## B. 版本/工具链风险（必须在上线前确认）

| # | 风险 | 证据 | 处理建议 |
|---|---|---|---|
| R1 | **wrapper 已就位且版本正确，无风险**：`gradlew`(8,618 B)、`gradlew.bat`(2,896 B)、`gradle\wrapper\gradle-wrapper.jar`(45,633 B)、`gradle\wrapper\gradle-wrapper.properties`(251 B) 四者都在；`distributionUrl=https\://mirrors.cloud.tencent.com/gradle/gradle-8.13-bin.zip` → **Gradle 8.13，与任务书 ③ 的推荐值完全一致**。原稿曾误记为「无 wrapper」，已据实更正 | `dir /b`（列出 `gradle`、`gradlew`、`gradlew.bat`）+ `dir /a /od`（文件体积与 11:51 时间戳）+ `type gradle\wrapper\gradle-wrapper.properties`（7 行全文），原始输出见「验收与复现」段 | **无需补 wrapper**。真正的阻断项是 JVM 目标，见 R2 |
| R2 | 若采用 `salt-ui:2.5.2`，其 AAR 的 `classes.jar` 是 **Java 21 字节码（major version 65）**，而本工程 `compileOptions`/`jvmTarget` 是 **17** | `javap -v` 输出 `major version: 65`；`app/build.gradle.kts:32-39` | 把 `compileOptions` 与 `jvmTarget` 提到 **21**；否则一旦调用 SaltUI 的 `inline` 函数（如 `Modifier.thenIf`）会报 "Cannot inline bytecode built with JVM target 21 into bytecode that is being built with JVM target 17" |
| R3 | `salt-ui:2.5.2` 会带 `androidx.compose.material3:material3-android:1.2.1`（我们 BOM 给 1.3.2，Gradle 取高版本 1.3.2）与 `org.jetbrains.compose.*:1.8.2`（会把 `androidx.compose.ui` 抬到 1.8.2） | 2.5.2 的 POM 全文 | 无需处理（同 minor 升级，二进制兼容）；但需在 `:app:dependencies` 里确认最终解析到的 `material3`/`ui` 版本并记录 |
| R4 | `salt-ui` 传递依赖含 **`org.lsposed.hiddenapibypass:6.1`** | 2.5.2 / 3.0.0-beta03 POM 均列出 | 该库会绕过 hidden API 限制；**上架审核/商店合规需确认**，必要时 `exclude(group = "org.lsposed.hiddenapibypass")` 并验证不崩 |
| R5 | SaltUI 3.0 源码（本仓库 HEAD）与本脚手架**不可能共存** | `libs.versions.toml` agp 9.1.1 / kotlin 2.4.20 / compose 1.12.2；`ui2/build.gradle.kts` compileSdk 37 + JVM 21；`gradle-wrapper` 9.3.1；AAR `@Metadata mv=[2,4,0]` | 不要尝试；本规格 S-3.3 的签名仅作「能力上限参考」，实际能用的是 2.5.2 的子集（见 S-3.4 判定） |
| R6 | `docs/build-versions.md` **不存在** | 目录枚举 | 任务书 ⑤ 要求对比该文件，实际缺失；本报告改用 `build.gradle.kts` / `app/build.gradle.kts` 的**真实声明值**作为对比基线 |
| R7 | `salt-ui-android:3.0.0-beta03` 的 `.module` 用新 KMP-Android 变体命名（`androidApiElements-published`），且 `createdBy.gradle=9.3.1` | 实查 `.module` | 即使抛开 Kotlin metadata，AGP 8.9.1 + Gradle 8.13 对该变体也无把握解析 |
| R8 | **工程根目录存在环境垃圾目录 `%D%`**（315 个文件 / 288,628,175 B），内含 `gradle-8.13-bin.zip`(136,983,045 B)、`gradle-8.13-bin.zip.ok`(0 B) 与解压后的 `gradle-8.13/` 发行版；目录名是未展开的 Windows 批处理变量 `%D%`，是「wrapper 首次下载发行版时目标变量没赋值」留下的产物 | `dir /s /a "%D%"` 实测：`2026/11/01 11:52  136,983,045 gradle-8.13-bin.zip` + `0 gradle-8.13-bin.zip.ok` + `gradle-8.13/{bin,init.d,lib}`。**来源判定：非 task-S 产生**——task-S 全程只执行 `dir/type/findstr/curl/python`（curl 输出全部落 `%TEMP%`），从未执行过 `gradlew`、`gradle` 或任何在项目根建目录的命令；`%D%` 的生成时刻 11:52 紧跟 task-A 生成 `gradlew`(11:51) 之后，且其内容正是 wrapper 首次拉取发行版的中间产物，与 task-A「装 SDK + 建可复现构建」的目标完全吻合 | **task-S 不删除它**：删除会越过 task-S 的边界条款（允许写范围仅 `docs/ui-spec.md` 与 `docs/ui/**`）。**建议由 task-A 负责人或用户手动删除**：`rmdir /s /q "E:\桌面\安卓软件\NarMusicAndroid\%D%"`。它不影响构建（不在 `sourceSets`、不在 Gradle 缓存路径），但会污染工程根与 git 状态 |

## C. 语义/交互层面的取舍（不在 A 列的）

- **C1**：Web 顶栏搜索是「搜索并回填下载页」的副作用式交互（S-1.7）。手机上建议**保留同语义**（搜到就切到下载页并回填 + Toast），不要改成独立搜索结果页——那会引入 Web 端不存在的新页面。
- **C2**：Web 的 `beforeunload` 拦截（`main.js:29-34`）在安卓对应「后台下载中禁止退出」，建议用 `FragmentActivity` 的 `OnBackPressedDispatcher` + 提示，不要静默丢任务。
- **C3**：`#audioSection` 是 320px 浮窗，手机上 `ModalBottomSheet` 更合理，但**尺寸感会变**（宽 100% vs 320px）。若必须保留窄卡观感，用 `Dialog` + `Card(Modifier.width(320.dp))`。
- **C4**：`#statusCard` 的 `display:none` ↔ 「空态」是 Web 唯一的"空"表达（S-1.2）。安卓端不要给它再加一个空态插图，保持"没任务就没有卡片"。
- **C5**：库页 `已下载` 标记靠 `localStorage['narmusic_dl_done']`（`library.js:18-22`）——这是**客户端本地状态**，换设备就丢。安卓端用 Room 或 `DataStore` 存同样的 `Set<系统文件名>` 即可，但**要知道它不是服务端状态**。
- **C6**：Web 有密码保护（`/api/protection/*`，`library.js:444-526`）。安卓端「删除」需要先 `protectionStatus()` → 需要时弹密码 → 拿 token；**token 存 `DataStore` 而不是明文 SharedPreferences**。
