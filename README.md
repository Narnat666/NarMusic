# NarMusic Android（纳儿音乐 · 安卓版）

把 B 站视频里的音频下载成「带封面 + 带歌词」的 M4A，存到手机 `内部存储/Music/NarMusic`，并在本地歌库里管理、播放 —— 原 Web 服务端（`../NarMusic`）功能的手机端重写，纯本地运行、不需要服务器。

## 功能（对齐原 Web 版）

| 功能 | 说明 |
|---|---|
| B 站音频提取 | 粘贴视频链接、`b23.tv` 短链、分享文本（含中文标题）、`av` 号或纯 BV 号；短链会先跟随重定向定位到指定视频，**只要输入含链接就不会退化成关键词搜索** |
| 关键词搜索下载 | 输入里完全没有链接时，才取 B 站搜索结果第一条 |
| 多平台歌词 | 酷狗 / 网易云 / QQ / 汽水 并发抓取，按歌名+歌手评分取最佳；酷狗 KRC 可解码 |
| 封面与元数据封装 | 下载到的 fMP4 重封装为普通 MP4，写入 `©nam/©ART/©alb/©lyr/covr` + 自定义 `----`（`narmeta`、`delayMs`） |
| 歌词整体平移 | 设置页「延迟 (ms)」按毫秒平移歌词时间轴，双语歌词自动合并 |
| 批量下载 | 粘贴歌单或导入文件；支持 `url\|平台\|延迟ms\|文件名` 的精确行（`narmeta` 格式），其余行按「歌名 - 歌手」关键词搜索 |
| 音乐库 | 行首显示 M4A 内嵌封面；搜索、排序（名称/时间/大小）、批量选择删除、打开文件位置 |
| 播放器 | 全屏沉浸式（背景随封面主色晕染），Media3 / ExoPlayer 播放，读取文件内 `©lyr` 做歌词滚动；横屏左右双栏 |
| 设置 | 歌词平台、延迟、深色模式；SharedPreferences 持久化，杀进程仍生效 |
| 横竖屏适配 | 竖屏底部四页签；横屏改左侧 `NavigationRail`、内容限宽 640dp 居中、消费状态栏与导航栏 inset，旋转不丢输入；下载页横屏为左右双栏（左输入+主按钮，右状态卡） |

界面：Material 3 视觉 + SaltUI 2.5.2 架构基线（`io.github.moriafly:salt-ui`）。设计语言参考椒盐音乐：连续渐变背景、整块 24dp 圆角 surface、无边框填充输入、单一胶囊主操作、无胶囊底部导航。

## 构建

Gradle 缓存与 SDK 都必须落在 E 盘，且 `set VAR=value&&` 之间**不能有空格**：

```bat
:: 工程路径含中文（本仓库在 E:\桌面\安卓软件\NarMusic），先建盘符映射
subst N: "E:\桌面\安卓软件"

cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusic&& gradlew.bat --no-daemon :app:assembleDebug"
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusic&& gradlew.bat --no-daemon :app:assembleRelease"
```

- 工程路径用 `N:\NarMusic`。**中文路径会让单测 test worker 的 UTF-8 argfile 被按 GBK 解码**，跑单测务必用 `N:` 盘路径（`assembleDebug` 不受影响，容易漏检）。
- `local.properties` 自行创建，内容 `sdk.dir=E:\\android-sdk`；该文件不入库。
- release 开了 R8（`isMinifyEnabled` + `isShrinkResources`），未加自定义 keep 规则。

### 签名（密钥不入库）

`keystore/narmusic.jks` 与 `keystore.properties` 都在 `.gitignore` 里。**没有它们也能构建**：`signingConfigs.release` 只在 keystore 存在时创建，缺失时 `assembleRelease` 产出未签名包。

要出可安装的签名包，先生成密钥并写配置：

```bat
keytool -genkeypair -v -keystore keystore\narmusic.jks -alias narmusic ^
  -keyalg RSA -keysize 2048 -validity 10000 ^
  -dname "CN=NarMusic, OU=NarMusic, O=NarNar, L=CN, ST=CN, C=CN"
```

`keystore.properties`（不入库）：

```properties
storePassword=你的口令
keyAlias=narmusic
keyPassword=你的口令
```

也可用环境变量 `NARMUSIC_STORE_PASSWORD` / `NARMUSIC_KEY_PASSWORD` 代替该文件。

- 产物：`app/build/outputs/apk/release/app-release.apk`。

版本：`applicationId com.narnat.music`、`versionName 1.0.0`、`minSdk 26`、`targetSdk 34`、`compileSdk 35`。

## 技术文档

`docs/` 下是与实现绑定的技术结论，读代码前先看这两份能省很多时间：

| 文件 | 内容 |
|---|---|
| `api-probe.md` | 外部接口实测（B 站 view/playurl/WBI、酷狗/网易云/QQ/汽水歌词），含当前字段名与已失效端点 |
| `ui-spec.md` | 界面规格：配色映射、尺寸、全部中文文案表、三态定义 |
| `build-versions.md` | 依赖版本矩阵与仓库校验依据 |
| `saltui-spike.md` | SaltUI 2.5.2 真实 API（`javap` 实录）与可用/不可用组件清单 |
| `fix-report-20261101.md` | 缺陷定位与修复记录（短链解析、图标、界面重构、横屏适配） |
| `redesign-report*.md` | 三轮界面迭代的量化验收数据 |
| `e2e-report.md` | 模拟器端到端验收记录 |
| `安装说明.md` | 面向最终用户的安装与权限说明 |

> 这些文档中的截图/dump 证据（`docs/e2e/`、`docs/probe/`）未入库，正文里的路径引用仅作来源标注。

## 目录结构

```
app/src/main/java/com/narnat/music/
├── MainActivity.kt / NarMusicApp.kt   入口与依赖装配
├── core/                             纯 Kotlin（无 Android 依赖，可单测）
│   ├── bili/BiliUtils.kt             链接识别、BV 号提取、关键词清洗、WBI 签名
│   ├── lyrics/                       LRC/KRC 解码、双语合并、时间轴平移
│   └── mp4/                          MP4 box 读写、标签写入（Mp4TagWriter）
├── data/
│   ├── audio/M4aMuxer.kt             fMP4 → 普通 MP4 重封装
│   ├── db/                           Room：MusicDatabase / MusicFileDao / MusicFileEntity
│   ├── library/MusicLibraryRepository.kt  MediaStore 落盘 + 删除 + 权限兜底
│   ├── prefs/SettingsStore.kt        SharedPreferences 设置持久化
│   └── remote/                       OkHttp 封装、B 站 API、四家歌词 Provider
├── di/                               手写依赖注入容器
├── domain/                           DownloadService（解析→下载→重封装→歌词→标签→入库）、PlaylistParser
└── ui/                               Compose 界面：components / player / screens / theme
                                       （ui/spike 是 SaltUI 2.5.2 组件的技术验证样例，随 UiState 渲染测试保留）
app/src/test/                         单元测试（131 例，Robolectric + JUnit4）
app/src/androidTest/                  M4aMuxerTest（真机/模拟器重封装验证，夹具为 assets 内 fMP4 样本）
docs/                                 技术文档与验收记录
docs/tools/check_tests.py             单测结果汇总脚本
```

## 测试

```bat
rem 单元测试（131 例）
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusic&& gradlew.bat --no-daemon :app:testDebugUnitTest"
python docs\tools\check_tests.py

rem 仪器测试（需模拟器/真机在线；被测包是 debug 包，装过 release 包要先卸载）
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusic&& gradlew.bat --no-daemon :app:connectedDebugAndroidTest"
```

## 已知限制

1. **汽水音乐歌词**只能通过「粘贴含 `track_id=` 的汽水分享链接」命中；其关键词搜索接口（`luna/pc/search/track`）已失效，普通关键词会返回空结果。
2. **B 站 CDN 的 UA 差异**：`upos-*.bilivideo.com` 节点会拒绝部分 UA（403）。下载阶段现在按 `okhttp` → 桌面 Chrome 的顺序逐个 UA/URL 重试，实测可下；若某天全节点都拒绝，需要再补 UA 或 Cookie。
3. **模拟器与真机的差异**（详见 `docs/e2e-report.md` §8）：模拟器是 x86_64 + swiftshader 软解，真机（vivo S60 / Android 14）走厂商硬解与 FuntouchOS 的后台/通知策略；`google_apis` 镜像没有文件管理器组件，「打开音乐文件夹」只能 Toast 兜底，真机一般能拉起。
4. 下载/入库依赖 `targetSdk 34` 的 MediaStore 语义：App 只能删除自己创建的文件，删除他人的文件会拉起系统授权弹窗。同一首歌重复下载时旧文件走同一条 MediaStore 删除路径（`MusicLibraryRepository.removeStoredFile`）移除，`Music/NarMusic` 里不会累积带时间戳的残留文件。
5. 未实现原 Web 版的服务器相关能力（cpolar 内网穿透、邮件通知、多端共享库）—— 安卓版本地运行，无服务器概念。
6. **未在真机验证**：所有验收在 Android 14 x86_64 模拟器上完成（软解播放、swiftshader 渲染），vivo S60 的 arm64 硬解、FuntouchOS 后台/通知策略、锁屏与来电打断未覆盖。
