# SaltUI 依赖可用性验证报告（task-X 产出，技术 Spike）

> 数据来源：本机真实构建 + 真实 AAR 反编译（`javap`）。所有签名均为 `salt-ui-android:2.5.2` 的字节码原文，**没有一处照抄 SaltUI 3.0 源码**。

## 0. 判定

**GO-但受限**：`io.github.moriafly:salt-ui:2.5.2` 能接入、能 `assembleDebug`、能在 Robolectric 下真实渲染并交互；但 3.0 的 5 个上层组件（`BasicScreen` / `ScreenCard` / `TitleBarButton` / `PillButton` / `Sheet`）与 `LabelValue` / `Card` / `SideBar` 在 2.5.2 **不存在**，`SaltTextStyles.largeTitle`、`SaltColors.success/warning/error` 也缺失——这几个必须用 Material3 自建。另有一条硬约束：`ItemCheck`/`ItemEdit`/`ItemSlider`/`ItemButton`/`ItemSelect`/`PopupMenu`/`PopupMenuItem`/`YesNoDialog`/`InputDialog` 都带 `@UnstableSaltUiApi`，调用处必须 `@OptIn(UnstableSaltUiApi::class)`，否则编译失败。

---

## 1. 验证环境与执行命令（含两个必须知道的环境坑）

| 项 | 值 |
|---|---|
| 工程 | `E:\桌面\安卓软件\NarMusicAndroid`（单模块 `app`，`com.narnat.music`） |
| Gradle / AGP / Kotlin | 8.13 / 8.9.1 / 2.1.20（**未改动** `settings.gradle.kts`、`gradle.properties`、根 `build.gradle.kts`） |
| `GRADLE_USER_HOME` | `E:\gradle-home` |
| `ANDROID_HOME` | `E:\android-sdk` |
| 新增依赖 | 仅 1 行：`implementation("io.github.moriafly:salt-ui:2.5.2")` |
| 实际使用 JDK | `E:\jdk`（`java.version=21.0.10`，`file.encoding=UTF-8`，`sun.jnu.encoding=GBK`） |

### 坑 1：`set VAR=value && ...` 会带尾随空格

```
# 失败原文（第 1 次尝试）
Exception in thread "main" java.lang.RuntimeException: Could not create parent directory for lock file
E:\gradle-home \wrapper\dists\gradle-8.13-bin\...\gradle-8.13-bin.zip.lck
```
`set GRADLE_USER_HOME=E:\gradle-home && cd ...` 里的空格进入了变量值（`E:\gradle-home `）。正确写法是 `&&` 紧贴、不留空格：

```bat
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d <工程路径>&& gradlew.bat --no-daemon <task>"
```
`set "VAR=value"` 这种带引号写法在 `cmd /c "..."` 里被二次转义成 `set ""VAR=value""`，**会静默设不上**（第 2 次尝试因此回落到 `C:\Users\<user>\.gradle`，构建虽成功但缓存写到了 C 盘）。发现后已全部改为正确写法，**本报告第 2/4 节的所有有效结果都来自 `GRADLE_USER_HOME=E:\gradle-home`**（SaltUI AAR 的实际路径也在 E 盘，见 2.3）。

#### C 盘残留的清理（已执行，附清理前后 `dir` 原始输出）

回落那次把 SaltUI 的 4 个构件包写进了 C 盘（10 个文件，mtime 12:10:30–12:12:07）：

```text
==== BEFORE: dir /b C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly ====
salt-core
salt-core-android
salt-ui
salt-ui-android
```

确认这 4 个目录下**只有 `2.5.2` 一个版本**、且全部是本次误写入（`dir /s /b` 逐文件清单共 10 个：`salt-ui-2.5.2.{pom,module}`、`salt-ui-android-2.5.2.{pom,module}` + `ui2-release.aar`、`salt-core-2.5.2.{pom,module}`、`salt-core-android-2.5.2.{pom,module}` + `core-release.aar`；`Get-ChildItem` 时间戳 12:10:30–12:12:07），无其它版本、无其它 group，随后只删这 4 个目录：

```bat
rmdir /s /q "C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly\salt-ui"
rmdir /s /q "C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly\salt-ui-android"
rmdir /s /q "C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly\salt-core"
rmdir /s /q "C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly\salt-core-android"
rmdir    "C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly"   :: 删空壳父目录
```

```text
==== AFTER: dir /b C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly ====
*** 目录不存在，已清理 ***
文件系统: 访问 C:\Users\86153\.gradle\caches\modules-2\files-2.1\io.github.moriafly 返回: 找不到文件
==== AFTER: dir /b ...\files-2.1 | findstr /i "^io\." ====
io.coil-kt
io.grpc
io.netty
io.perfmark
==== AFTER: files-2.1 目录总数 ====
137
```
（`io.coil-kt` / `io.grpc` / `io.netty` / `io.perfmark` 是 C 盘原有缓存，**未动**。`files-2.1` 下有 **137** 个 group 目录这一数字取自**清理后**的实测（`dir /b | find /c /v ""` = 137）；清理前的数字未单独测量，理论上应为 138（多 `io.github.moriafly` 一个 group）。这里用它作「清理后基线」，与下文重跑构建后的 137 对比，用于证明构建没有回写 C 盘。）

**清理后重跑验收 1/2，并再次确认 C 盘不回写**：

```bat
:: 验收 1（原路径，含 SaltSpikeExtra.kt）
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d E:\桌面\安卓软件\NarMusicAndroid&& gradlew.bat --no-daemon :app:assembleDebug"
:: 验收 2（subst 盘符规避中文路径 argfile 问题）
subst N: "E:\桌面\安卓软件"
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:testDebugUnitTest --tests *SaltSpikeRenderTest*"
```

```text
build\assemble8.txt          → > Task :app:preDebugBuild UP-TO-DATE / BUILD SUCCESSFUL in 18s
build\test11.txt             → > Task :app:testDebugUnitTest / BUILD SUCCESSFUL in 29s
（test11 是删掉 app\build\test-results\testDebugUnitTest 强制重跑的结果，不是 UP-TO-DATE）
测试 XML 首行              → tests="3" skipped="0" failures="0" errors="0" timestamp="2026-11-01T04:43:44.643Z" time="8.838"
:: 重跑后再查 C 盘
if exist "C:\...\files-2.1\io.github.moriafly" → C_DRIVE_STILL_CLEAN_NO_moriafly
dir /b C:\...\files-2.1 | findstr moriafly    → 无输出
dir /b C:\...\files-2.1 | find /c /v ""       → 137（与清理后基线一致，构建未回写 C 盘）
dir /b E:\gradle-home\caches\modules-2\files-2.1 | findstr moriafly → io.github.moriafly（构件在 E 盘，位置正确）
```

原始输出存档：`build\cdrive_before.txt`、`build\cdrive_after.txt`、`build\assemble8.txt`、`build\test11.txt`。

### 坑 2（关键，直接影响后续所有界面任务的测试）：工程路径含中文 → `testDebugUnitTest` 必然 `ClassNotFoundException`

在任何 Robolectric/JUnit 单元测试上，原路径运行会得到：

```
SaltSpikeRenderTest > initializationError FAILED
    java.lang.ClassNotFoundException at BuiltinClassLoader.java:641
1 test completed, 1 failed
```
根因（实测证据，与 SaltUI 无关）：Gradle 把 test worker 的 classpath 写进参数文件 `E:\gradle-home\.tmp\gradle-worker-classpath*.txt`（UTF-8），而 Windows 上 JDK 的 launcher **按 `sun.jnu.encoding`=GBK 解码 @argfile**，于是含中文的工程路径变成乱码，测试类加载不到。三组对照实验：

| 实验 | 结果 |
|---|---|
| `E:\jdk\bin\java.exe -cp "app\build\tmp\kotlin-classes\debugUnitTest;..." CpProbe`（直接命令行传中文路径） | `LOADED=com.narnat.music.ui.spike.PlainJunitProbeTest` ✅ |
| `java @arg_utf8.txt CpProbe`（argfile 为 UTF-8，与 Gradle 写法相同） | `错误: 找不到或无法加载主类 CpProbe` ❌ |
| `java @arg_gbk.txt CpProbe`（argfile 为 GBK） | `LOADED=...` ✅（并打印出正确的中文 classpath） |

> 上表里的 `CpProbe` / `PlainJunitProbeTest` 只是诊断探针，验证完已删除，工程里最终只保留 `SaltSpike.kt` 与 `SaltSpikeRenderTest.kt`。

**可用解法（不改任何工程文件）**：把含中文的父目录映射成无中文盘符后再跑 Gradle。

```bat
subst N: "E:\桌面\安卓软件"
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:testDebugUnitTest --tests *SaltSpikeRenderTest*"
```
本报告 X-3 的测试就是在 `N:\NarMusicAndroid` 下跑通的。`assembleDebug` 不受此坑影响（原路径也能过）。

---

## 2. X-1 依赖接入与解析

### 2.1 改动（共 1 行，位于 `app/build.gradle.kts` 的 `dependencies` 块，Compose 依赖之后）

```kotlin
    implementation("io.github.moriafly:salt-ui:2.5.2")
```

### 2.2 实际解析版本表（`:app:dependencies --configuration debugRuntimeClasspath`，`BUILD SUCCESSFUL in 15s`）

| 模块 | 解析结果 | 说明 |
|---|---|---|
| `io.github.moriafly:salt-ui` | **2.5.2** | 仅变体重定向层（`.module`） |
| `io.github.moriafly:salt-ui-android` | **2.5.2** | 真正的 AAR（`ui2-release.aar`，526 KB 级） |
| `io.github.moriafly:salt-core` / `-android` | **2.5.2** | 传递依赖 |
| `org.jetbrains.compose.runtime:runtime` | **1.8.2** | 被 SaltUI 拉高 |
| `org.jetbrains.compose.foundation:foundation` / `-layout` | **1.8.2** | 同上 |
| `org.jetbrains.compose.ui:ui` / `ui-graphics` / `ui-text` / `ui-util` / `ui-geometry` / `ui-unit` | **1.8.2** | 同上 |
| `org.jetbrains.compose.animation:animation` / `-core` | **1.8.2** | 同上 |
| `org.jetbrains.compose.components:components-resources(-android)` | **1.8.2** | SaltUI 用 Compose Resources |
| `org.jetbrains.compose.components:components-ui-tooling-preview(-android)` | **1.8.2** | 同上 |
| `androidx.compose.ui:ui` / `ui-graphics` / `ui-tooling-preview` / `ui-tooling` / `ui-test-manifest` | **1.8.2** | **BOM 2025.04.01 原本是 1.8.0，被 SaltUI 提升到 1.8.2**（未强行压回） |
| `androidx.compose.runtime:runtime` / `-saveable` | **1.8.2** | 同上 |
| `androidx.compose.foundation:foundation` / `-layout` | **1.8.2** | 同上 |
| `androidx.compose.animation:animation` / `-core` | **1.8.2** | 同上 |
| `androidx.compose.material3:material3` | **1.3.2** | SaltUI 要 1.2.1 → 保持本工程 BOM 的更高版本 1.3.2 |
| `androidx.core:core-ktx` | **1.15.0** | SaltUI 要 1.9.0 → 升到本工程版本 |
| `androidx.activity:activity-compose` | **1.10.1** | SaltUI 要 1.9.2 → 升到本工程版本 |
| `com.google.android.material:material` | **1.12.0** | SaltUI 硬依赖（Android View 主题） |
| `org.lsposed.hiddenapibypass:hiddenapibypass` | **6.1** | 进入运行时（SaltUI 用于绕过隐藏 API 限制） |
| `org.jetbrains.kotlin:kotlin-stdlib` | **2.1.21** | SaltUI 要 2.1.21；Kotlin 编译器 2.1.20，兼容（`@Metadata mv=[2,1,0]`） |
| `dev.chrisbanes.haze:haze` | **不存在** | 2.5.2 没有 haze 依赖（3.0 才有） |

`salt-ui-android:2.5.2` 的直接依赖（`dependencies` 输出原文，逐行摘录）：

```
+--- androidx.core:core-ktx:1.9.0 -> 1.15.0 (*)
+--- com.google.android.material:material:1.12.0
+--- androidx.compose.material3:material3:1.2.1 -> 1.3.2 (*)
+--- androidx.activity:activity-compose:1.9.2 -> 1.10.1 (*)
+--- org.lsposed.hiddenapibypass:hiddenapibypass:6.1
+--- io.github.moriafly:salt-core:2.5.2
+--- org.jetbrains.kotlin:kotlin-stdlib:2.1.21 (*)
+--- org.jetbrains.compose.runtime:runtime:1.8.2
+--- org.jetbrains.compose.foundation:foundation:1.8.2
+--- org.jetbrains.compose.ui:ui:1.8.2 (*)
+--- org.jetbrains.compose.components:components-resources:1.8.2
\--- org.jetbrains.compose.components:components-ui-tooling-preview:1.8.2
```

### 2.3 `assembleDebug` 与「Android 上不缺类」验证

```
> Task :app:assembleDebug
BUILD SUCCESSFUL in 31s          ← 退出码 0（build\assemble7.txt，含 SaltSpike.kt + SaltSpikeExtra.kt）
```
> C 盘缓存清理后又重跑了一次同样命令：`build\assemble8.txt` → `BUILD SUCCESSFUL in 18s`（证明构件确实来自 `E:\gradle-home`，删掉 C 盘残留不影响构建）。两次结果都在原路径 `E:\桌面\安卓软件\NarMusicAndroid` 下取得。
APK：`app/build/outputs/apk/debug/app-debug.apk`，解包后有 `classes.dex` … `classes6.dex` 共 6 个 dex。用 `python zipfile` 直接在 dex 字节流里搜类型描述符（`build\check_apk.py`，最终 APK 上跑）：

```
dex_count 6
Lcom/moriafly/salt/ui/ItemKt;                  -> True
Lcom/moriafly/salt/ui/SaltThemeKt;             -> True
Lcom/moriafly/salt/ui/ButtonKt;                -> True
Lcom/moriafly/salt/ui/popup/PopupMenuKt;       -> True
Lcom/moriafly/salt/ui/dialog/DialogKt;         -> True
Lcom/narnat/music/ui/spike/SaltSpikeKt;        -> True
Lcom/narnat/music/ui/spike/SaltSpikeExtraKt;   -> True
```
另有统计（同一脚本）：dex 内 `Lcom/moriafly/salt/ui/*Kt;` 形态的类共 **29** 个，SaltUI 包前缀 **10** 个：`Lcom/moriafly/salt/core/`、`.../core/os/`、`.../ui/`、`.../ui/dialog/`、`.../ui/ext/`、`.../ui/icons/`、`.../ui/internal/`、`.../ui/popup/`、`.../ui/util/`、`.../ui/web/`。

> **重要澄清**：Maven 坐标是 `io.github.moriafly:salt-ui`，但**运行时 Java 包名是 `com.moriafly.salt.ui`**（不是 `io/github/moriafly/salt/ui`）。任务书 X-1.3 里写的 `io/github/moriafly/salt/ui/` 在 APK 里永远搜不到，别据此判定失败。

AAR 位置（Gradle 缓存，`E:\gradle-home`）：

```
E:\gradle-home\caches\modules-2\files-2.1\io.github.moriafly\salt-ui-android\2.5.2\d4dec88fc973c5461dcb7814c2ede0fd5df6a0b4\ui2-release.aar
E:\gradle-home\caches\modules-2\files-2.1\io.github.moriafly\salt-core-android\2.5.2\5b976797b2d91f6e3ec9f7fd39e776c9a767aec4\core-release.aar
```
AAR 内部条目：`AndroidManifest.xml`、`classes.jar`（228 个条目）、`R.txt`、`res/values/values.xml`、`assets/composeResources/saltui.ui2.generated.resources/...`。

---

## 3. X-2 SaltUI 2.5.2 的真实 API 签名（`javap` 原文）

反编译命令模板：

```bat
cd /d E:\桌面\安卓软件\NarMusicAndroid\build\saltspike\aar
E:\jdk\bin\jar.exe tf classes.jar > ..\classlist.txt
E:\jdk\bin\javap.exe -classpath classes.jar com.moriafly.salt.ui.SaltThemeKt
```

### 3.1 类清单核实（`jar tf` 证据）

`classes.jar` 共 **228** 条条目，`com/moriafly/salt/ui/` 顶层类 **50** 个。任务书点名的 12 个类型里，**只有 6 个存在**：

| 任务书点名 | 2.5.2 是否存在 | 说明 |
|---|---|---|
| `SaltThemeKt` | ✅ | `SaltTheme(...)` + `ChangeSaltThemeIsDark(...)` |
| `SaltColorsKt` | ✅ | 只有 `lightSaltColors` / `darkSaltColors` 两个工厂 |
| `SaltPaletteKt` | ❌ | `SaltPalette` 是 `object`，12 个颜色全是 `getXxx-0d7_KjU()` 实例方法，没有 `*Kt` 文件类 |
| `SaltTextStylesKt` | ✅ | `saltTextStyles(main, sub, paragraph)` |
| `SaltShapesKt` | ✅ | `default(SaltShapes$Companion, small, medium, large)` |
| `SaltDimensKt` | ✅ | `saltDimens(item, itemIcon, corner, dialogCorner, padding, subPadding)` |
| `BasicScreenKt` | ❌ **缺失** | 3.0 才在 `screen/BasicScreen.kt` |
| `ScreenCardKt` | ❌ **缺失** | 3.0 才有 |
| `ItemKt` | ✅ | 本报告最重要的一族 |
| `RoundedColumnKt` | ✅ | 只有 1 个重载，且**没有 `paddingValues` / `RoundedColumnType`** |
| `PillButtonKt` | ❌ **缺失** | 3.0 才有（`LocalControlSize` / `ControlSize` 在 2.5.2 也不存在） |
| `SheetKt` | ❌ **缺失** | 2.5.2 只有 `dialog/BottomSheetDialogKt`（View 版 BottomSheetDialog），没有 `sheet/` 包 |
| `SwitcherKt` | ✅ | **只有 `Switcher(state, modifier)` 一个重载**（无 `onChange` 版本） |
| `LabelValueKt` | ❌ **缺失** | 3.0 才有；2.5.2 用 `ItemValue(text, sub)` 代替 |
| `TitleBarButtonKt` | ❌ **缺失** | 3.0 才有 |

`javap` 对不存在/包名猜错的类给出的原始输出（对任务书点名的每个缺失类都实际跑了一遍，保留以证明"确实没有"）：

```text
==== BasicScreenKt ====
错误: 找不到类: com.moriafly.salt.ui.BasicScreenKt
==== ScreenCardKt ====
错误: 找不到类: com.moriafly.salt.ui.ScreenCardKt
==== PillButtonKt ====
错误: 找不到类: com.moriafly.salt.ui.PillButtonKt
==== SheetKt ====
错误: 找不到类: com.moriafly.salt.ui.SheetKt
==== LabelValueKt ====
错误: 找不到类: com.moriafly.salt.ui.LabelValueKt
==== TitleBarButtonKt ====
错误: 找不到类: com.moriafly.salt.ui.TitleBarButtonKt
==== SaltPaletteKt ====
错误: 找不到类: com.moriafly.salt.ui.SaltPaletteKt
==== CardKt ====
错误: 找不到类: com.moriafly.salt.ui.CardKt
==== SideBarKt ====
错误: 找不到类: com.moriafly.salt.ui.SideBarKt
==== PopupMenuKt ====
错误: 找不到类: com.moriafly.salt.ui.PopupMenuKt
==== DialogKt ====
错误: 找不到类: com.moriafly.salt.ui.DialogKt
==== SaltDimens$Companion ====
错误: 找不到类: com.moriafly.salt.ui.SaltDimens$Companion
==== SaltIcons ====
错误: 找不到类: com.moriafly.salt.ui.SaltIcons
```
后两个（`PopupMenuKt`/`DialogKt`）是**包名猜错**而非缺失：真实位置是 `com.moriafly.salt.ui.popup.PopupMenuKt`、`com.moriafly.salt.ui.dialog.DialogKt`、`com.moriafly.salt.ui.icons.SaltIcons`；`SaltDimens` **没有** companion，2.5.2 用顶层 `saltDimens(...)`。

### 3.2 `javap` 原文（23 个类）

```text
==== SaltThemeKt ====
Compiled from "SaltTheme.kt"
public final class com.moriafly.salt.ui.SaltThemeKt {
  public static final void SaltTheme(com.moriafly.salt.ui.SaltConfigs, com.moriafly.salt.ui.SaltDynamicColors, com.moriafly.salt.ui.SaltTextStyles, com.moriafly.salt.ui.SaltDimens, com.moriafly.salt.ui.SaltShapes, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ChangeSaltThemeIsDark(boolean, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int);
  public static final androidx.compose.runtime.ProvidableCompositionLocal access$getLocalSaltConfigs$p();
  public static final androidx.compose.runtime.ProvidableCompositionLocal access$getLocalSaltDynamicColors$p();
  public static final androidx.compose.runtime.ProvidableCompositionLocal access$getLocalSaltTextStyles$p();
  public static final androidx.compose.runtime.ProvidableCompositionLocal access$getLocalSaltDimens$p();
  public static final androidx.compose.runtime.ProvidableCompositionLocal access$getLocalSaltShapes$p();
  static {};
}
==== SaltTheme ====
Compiled from "SaltTheme.kt"
public final class com.moriafly.salt.ui.SaltTheme {
  public static final com.moriafly.salt.ui.SaltTheme INSTANCE;
  public static final int $stable;
  public final com.moriafly.salt.ui.SaltConfigs getConfigs(androidx.compose.runtime.Composer, int);
  public final com.moriafly.salt.ui.SaltColors getColors(androidx.compose.runtime.Composer, int);
  public final com.moriafly.salt.ui.SaltTextStyles getTextStyles(androidx.compose.runtime.Composer, int);
  public final com.moriafly.salt.ui.SaltDimens getDimens(androidx.compose.runtime.Composer, int);
  public final com.moriafly.salt.ui.SaltShapes getShapes(androidx.compose.runtime.Composer, int);
  static {};
}
==== SaltColorsKt ====
Compiled from "SaltColors.kt"
public final class com.moriafly.salt.ui.SaltColorsKt {
  public static final com.moriafly.salt.ui.SaltColors lightSaltColors-FD3wquc(long, long, long, long, long, long, long, long);
  public static com.moriafly.salt.ui.SaltColors lightSaltColors-FD3wquc$default(long, long, long, long, long, long, long, long, int, java.lang.Object);
  public static final com.moriafly.salt.ui.SaltColors darkSaltColors-FD3wquc(long, long, long, long, long, long, long, long);
  public static com.moriafly.salt.ui.SaltColors darkSaltColors-FD3wquc$default(long, long, long, long, long, long, long, long, int, java.lang.Object);
}
==== SaltColors ====
Compiled from "SaltColors.kt"
public final class com.moriafly.salt.ui.SaltColors {
  public static final int $stable;
  public final long getHighlight-0d7_KjU();
  public final long getText-0d7_KjU();
  public final long getSubText-0d7_KjU();
  public final long getBackground-0d7_KjU();
  public final long getSubBackground-0d7_KjU();
  public final long getPopup-0d7_KjU();
  public final long getStroke-0d7_KjU();
  public final long getOnHighlight-0d7_KjU();
  public final com.moriafly.salt.ui.SaltColors copy-FD3wquc(long, long, long, long, long, long, long, long);
  public static com.moriafly.salt.ui.SaltColors copy-FD3wquc$default(com.moriafly.salt.ui.SaltColors, long, long, long, long, long, long, long, long, int, java.lang.Object);
  public com.moriafly.salt.ui.SaltColors(long, long, long, long, long, long, long, long, kotlin.jvm.internal.DefaultConstructorMarker);
  static {};
}
==== SaltPalette ====
Compiled from "SaltPalette.kt"
public final class com.moriafly.salt.ui.SaltPalette {
  public static final com.moriafly.salt.ui.SaltPalette INSTANCE;
  public static final int $stable;
  public final long getWarningLightIcon-0d7_KjU();
  public final long getWarningDarkIcon-0d7_KjU();
  public final long getWarningLightBackground-0d7_KjU();
  public final long getWarningDarkBackground-0d7_KjU();
  public final long getErrorLightIcon-0d7_KjU();
  public final long getErrorDarkIcon-0d7_KjU();
  public final long getErrorLightBackground-0d7_KjU();
  public final long getErrorDarkBackground-0d7_KjU();
  public final long getSuccessLightIcon-0d7_KjU();
  public final long getSuccessDarkIcon-0d7_KjU();
  public final long getSuccessLightBackground-0d7_KjU();
  public final long getSuccessDarkBackground-0d7_KjU();
  static {};
}
==== SaltTextStylesKt ====
Compiled from "SaltTextStyles.kt"
public final class com.moriafly.salt.ui.SaltTextStylesKt {
  public static final com.moriafly.salt.ui.SaltTextStyles saltTextStyles(androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle);
  public static com.moriafly.salt.ui.SaltTextStyles saltTextStyles$default(androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, int, java.lang.Object);
}
==== SaltTextStyles ====
Compiled from "SaltTextStyles.kt"
public final class com.moriafly.salt.ui.SaltTextStyles {
  public static final int $stable;
  public com.moriafly.salt.ui.SaltTextStyles(androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle);
  public final androidx.compose.ui.text.TextStyle getMain();
  public final androidx.compose.ui.text.TextStyle getSub();
  public final androidx.compose.ui.text.TextStyle getParagraph();
  public final com.moriafly.salt.ui.SaltTextStyles copy(androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle);
  public static com.moriafly.salt.ui.SaltTextStyles copy$default(com.moriafly.salt.ui.SaltTextStyles, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, androidx.compose.ui.text.TextStyle, int, java.lang.Object);
  static {};
}
==== SaltShapesKt ====
Compiled from "SaltShapes.kt"
public final class com.moriafly.salt.ui.SaltShapesKt {
  public static final com.moriafly.salt.ui.SaltShapes default(com.moriafly.salt.ui.SaltShapes$Companion, androidx.compose.ui.graphics.Shape, androidx.compose.ui.graphics.Shape, androidx.compose.ui.graphics.Shape);
  public static com.moriafly.salt.ui.SaltShapes default$default(com.moriafly.salt.ui.SaltShapes$Companion, androidx.compose.ui.graphics.Shape, androidx.compose.ui.graphics.Shape, androidx.compose.ui.graphics.Shape, int, java.lang.Object);
}
==== SaltDimensKt ====
Compiled from "SaltDimens.kt"
public final class com.moriafly.salt.ui.SaltDimensKt {
  public static final com.moriafly.salt.ui.SaltDimens saltDimens-erZIsFM(float, float, float, float, float, float);
  public static com.moriafly.salt.ui.SaltDimens saltDimens-erZIsFM$default(float, float, float, float, float, float, int, java.lang.Object);
}
==== SaltConfigsKt ====
Compiled from "SaltConfigs.kt"
public final class com.moriafly.salt.ui.SaltConfigsKt {
  public static final com.moriafly.salt.ui.SaltConfigs saltConfigs(boolean, androidx.compose.foundation.Indication);
  public static com.moriafly.salt.ui.SaltConfigs saltConfigs$default(boolean, androidx.compose.foundation.Indication, int, java.lang.Object);
}
==== SaltDynamicColors ====
Compiled from "SaltColors.kt"
public final class com.moriafly.salt.ui.SaltDynamicColors {
  public static final com.moriafly.salt.ui.SaltDynamicColors$Companion Companion;
  public static final int $stable;
  public com.moriafly.salt.ui.SaltDynamicColors(com.moriafly.salt.ui.SaltColors, com.moriafly.salt.ui.SaltColors);
  public final com.moriafly.salt.ui.SaltColors getLight();
  public final com.moriafly.salt.ui.SaltColors getDark();
  public final com.moriafly.salt.ui.SaltColors component1();
  public final com.moriafly.salt.ui.SaltColors component2();
  public final com.moriafly.salt.ui.SaltDynamicColors copy(com.moriafly.salt.ui.SaltColors, com.moriafly.salt.ui.SaltColors);
  public static com.moriafly.salt.ui.SaltDynamicColors copy$default(com.moriafly.salt.ui.SaltDynamicColors, com.moriafly.salt.ui.SaltColors, com.moriafly.salt.ui.SaltColors, int, java.lang.Object);
  public java.lang.String toString();
  public int hashCode();
  public boolean equals(java.lang.Object);
  static {};
}
==== ItemKt ====
Compiled from "Item.kt"
public final class com.moriafly.salt.ui.ItemKt {
  public static final void ItemTitle(java.lang.String, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final void ItemTip(java.lang.String, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final void ItemText(java.lang.String, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final void Item-si-Zk_8(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, long, java.lang.String, long, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, com.moriafly.salt.ui.ItemArrowType, androidx.compose.runtime.Composer, int, int, int);
  public static final void ItemSwitcher-zYA1wlE(boolean, kotlin.jvm.functions.Function1<? super java.lang.Boolean, kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, java.lang.String, androidx.compose.runtime.Composer, int, int);
  public static final void ItemSelect-zYA1wlE(com.moriafly.salt.ui.popup.PopupState, java.lang.String, java.lang.String, androidx.compose.ui.Modifier, boolean, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.ColumnScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ItemPopup-zYA1wlE(com.moriafly.salt.ui.popup.PopupState, java.lang.String, java.lang.String, androidx.compose.ui.Modifier, boolean, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.ColumnScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ItemCheck(boolean, kotlin.jvm.functions.Function1<? super java.lang.Boolean, kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, java.lang.String, androidx.compose.runtime.Composer, int, int);
  public static final void ItemValue(java.lang.String, java.lang.String, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final void ItemEdit-Hzv_svQ(java.lang.String, kotlin.jvm.functions.Function1<? super java.lang.String, kotlin.Unit>, androidx.compose.ui.Modifier, java.lang.String, long, boolean, androidx.compose.foundation.text.KeyboardOptions, androidx.compose.foundation.text.KeyboardActions, androidx.compose.ui.text.input.VisualTransformation, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ItemEditPassword-NpZTi58(java.lang.String, kotlin.jvm.functions.Function1<? super java.lang.String, kotlin.Unit>, androidx.compose.ui.Modifier, java.lang.String, long, boolean, androidx.compose.foundation.text.KeyboardOptions, androidx.compose.foundation.text.KeyboardActions, androidx.compose.runtime.Composer, int, int);
  public static final void ItemSlider-z756j2U(float, kotlin.jvm.functions.Function1<? super java.lang.Float, kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, java.lang.String, boolean, kotlin.ranges.ClosedFloatingPointRange<java.lang.Float>, int, kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.foundation.interaction.MutableInteractionSource, androidx.compose.runtime.Composer, int, int, int);
  public static final void ItemButton-AAXhOkM(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, boolean, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, androidx.compose.runtime.Composer, int, int);
  public static final void ItemSpacer(androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final void ItemContainer(androidx.compose.ui.Modifier, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.BoxScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ItemDivider-aM-cp0Q(androidx.compose.ui.Modifier, long, float, androidx.compose.runtime.Composer, int, int);
  public static final void ItemInfo(java.lang.String, com.moriafly.salt.ui.ItemInfoType, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
  public static final boolean access$ItemEditPassword_NpZTi58$lambda$35(androidx.compose.runtime.MutableState);
  public static final void access$ItemEditPassword_NpZTi58$lambda$36(androidx.compose.runtime.MutableState, boolean);
}
==== RoundedColumnKt ====
Compiled from "RoundedColumn.kt"
public final class com.moriafly.salt.ui.RoundedColumnKt {
  public static final void RoundedColumn-3IgeMak(androidx.compose.ui.Modifier, long, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.ColumnScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
}
==== SwitcherKt ====
Compiled from "Switcher.kt"
public final class com.moriafly.salt.ui.SwitcherKt {
  public static final void Switcher(boolean, androidx.compose.ui.Modifier, androidx.compose.runtime.Composer, int, int);
}
==== ButtonKt ====
Compiled from "Button.kt"
public final class com.moriafly.salt.ui.ButtonKt {
  public static final void Button(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, com.moriafly.salt.ui.ButtonType, androidx.compose.runtime.Composer, int, int);
  public static final void TextButton-wBJOh4Y(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.String, androidx.compose.ui.Modifier, boolean, long, long, androidx.compose.runtime.Composer, int, int);
  public static final void BasicButton-fWhpE4E(kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.Modifier, boolean, long, androidx.compose.foundation.layout.PaddingValues, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.RowScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
}
==== ItemOuterKt ====
Compiled from "ItemOuter.kt"
public final class com.moriafly.salt.ui.ItemOuterKt {
  public static final void ItemOuterTitle(java.lang.String, androidx.compose.runtime.Composer, int);
  public static final void ItemOuterTip(java.lang.String, androidx.compose.runtime.Composer, int);
  public static final void ItemOuterLargeTitle(java.lang.String, java.lang.String, androidx.compose.runtime.Composer, int);
  public static final void ItemOuterEdit-Hzv_svQ(java.lang.String, kotlin.jvm.functions.Function1<? super java.lang.String, kotlin.Unit>, androidx.compose.ui.Modifier, java.lang.String, long, boolean, androidx.compose.foundation.text.KeyboardOptions, androidx.compose.foundation.text.KeyboardActions, androidx.compose.ui.text.input.VisualTransformation, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void ItemOuterTextButton-jA1GFJw(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.String, boolean, long, long, androidx.compose.runtime.Composer, int, int);
  public static final void ItemOuterSpacer(androidx.compose.runtime.Composer, int);
  public static final void ItemOuterHalfSpacer(androidx.compose.runtime.Composer, int);
}
==== ItemArrowType ====
Compiled from "Item.kt"
public final class com.moriafly.salt.ui.ItemArrowType extends java.lang.Enum<com.moriafly.salt.ui.ItemArrowType> {
  public static final com.moriafly.salt.ui.ItemArrowType Arrow;
  public static final com.moriafly.salt.ui.ItemArrowType None;
  public static final com.moriafly.salt.ui.ItemArrowType Link;
  public static com.moriafly.salt.ui.ItemArrowType[] values();
  public static com.moriafly.salt.ui.ItemArrowType valueOf(java.lang.String);
  public static kotlin.enums.EnumEntries<com.moriafly.salt.ui.ItemArrowType> getEntries();
  static {};
}
==== ButtonType ====
Compiled from "Button.kt"
public final class com.moriafly.salt.ui.ButtonType extends java.lang.Enum<com.moriafly.salt.ui.ButtonType> {
  public static final com.moriafly.salt.ui.ButtonType Highlight;
  public static final com.moriafly.salt.ui.ButtonType Sub;
  public static com.moriafly.salt.ui.ButtonType[] values();
  public static com.moriafly.salt.ui.ButtonType valueOf(java.lang.String);
  public static kotlin.enums.EnumEntries<com.moriafly.salt.ui.ButtonType> getEntries();
  static {};
}
==== popup.PopupMenuKt ====
Compiled from "PopupMenu.kt"
public final class com.moriafly.salt.ui.popup.PopupMenuKt {
  public static final int InTransitionDuration;
  public static final int OutTransitionDuration;
  public static final void PopupMenu-RmyCI1M(boolean, kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.Modifier, long, androidx.compose.ui.window.PopupProperties, long, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.ColumnScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void DropdownMenuContent-uDo3WH8(androidx.compose.animation.core.MutableTransitionState<java.lang.Boolean>, androidx.compose.runtime.MutableState<androidx.compose.ui.graphics.TransformOrigin>, androidx.compose.ui.Modifier, long, kotlin.jvm.functions.Function3<? super androidx.compose.foundation.layout.ColumnScope, ? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final long calculateTransformOrigin(androidx.compose.ui.unit.IntRect, androidx.compose.ui.unit.IntRect);
  public static final float getDropdownMenuVerticalPadding();
  public static final float getMenuVerticalMargin();
  static {};
}
==== dialog.DialogKt ====
Compiled from "Dialog.kt"
public final class com.moriafly.salt.ui.dialog.DialogKt {
  public static final void YesDialog(kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.window.DialogProperties, java.lang.String, java.lang.String, java.lang.String, androidx.compose.runtime.Composer, int, int);
  public static final void YesNoDialog(kotlin.jvm.functions.Function0<kotlin.Unit>, kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.window.DialogProperties, java.lang.String, java.lang.String, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, java.lang.String, java.lang.String, androidx.compose.runtime.Composer, int, int);
  public static final void InputDialog(kotlin.jvm.functions.Function0<kotlin.Unit>, kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.window.DialogProperties, java.lang.String, java.lang.String, kotlin.jvm.functions.Function1<? super java.lang.String, kotlin.Unit>, java.lang.String, java.lang.String, java.lang.String, androidx.compose.runtime.Composer, int, int);
  public static final void BasicDialog(kotlin.jvm.functions.Function0<kotlin.Unit>, androidx.compose.ui.window.DialogProperties, kotlin.jvm.functions.Function2<? super androidx.compose.runtime.Composer, ? super java.lang.Integer, kotlin.Unit>, androidx.compose.runtime.Composer, int, int);
  public static final void DialogTitle(java.lang.String, androidx.compose.runtime.Composer, int);
}
==== popup.PopupStateKt ====
Compiled from "PopupState.kt"
public final class com.moriafly.salt.ui.popup.PopupStateKt {
  public static final com.moriafly.salt.ui.popup.PopupState rememberPopupState(boolean, androidx.compose.runtime.Composer, int, int);
}
==== popup.PopupState ====
Compiled from "PopupState.kt"
public final class com.moriafly.salt.ui.popup.PopupState {
  public static final com.moriafly.salt.ui.popup.PopupState$Companion Companion;
  public static final int $stable;
  public com.moriafly.salt.ui.popup.PopupState(boolean);
  public final boolean getExpend();
  public final void expend();
  public final void dismiss();
  public static final androidx.compose.runtime.saveable.Saver access$getSaver$cp();
  static {};
}
==== popup.PopupMenuItemKt ====
Compiled from "PopupMenuItem.kt"
public final class com.moriafly.salt.ui.popup.PopupMenuItemKt {
  public static final void PopupMenuItem-PIknLig(kotlin.jvm.functions.Function0<kotlin.Unit>, java.lang.Boolean, java.lang.String, java.lang.String, androidx.compose.ui.graphics.painter.Painter, androidx.compose.foundation.layout.PaddingValues, androidx.compose.ui.graphics.Color, androidx.compose.runtime.Composer, int, int);
  static {};
}
```

> `PopupMenuItem` 在 2.5.2 **已弃用**（见 3.3 的编译期 warning 原文），官方建议改用 `Item`。

> 说明：`-si-Zk_8` / `-zYA1wlE` / `-3IgeMak` 这类后缀是 Kotlin inline value class（`Color`、`Dp`）在 JVM 上的名称修饰，Kotlin 源码里名字仍是 `Item` / `ItemSwitcher` / `RoundedColumn`、参数类型是 `Color` / `Dp`。`Item` 的 `boolean` 是 `enabled`，`long` 是 `textColor: Color`，`String` 是 `sub`，最后一个 `ItemArrowType` 是 `arrowType`。

### 3.3 编译期实测的额外事实（`javap` 看不出来，后续界面任务必须知道）

把「白名单里声明可用」的组件真写进代码编译（`app/src/main/java/com/narnat/music/ui/spike/SaltSpikeExtra.kt`）后，`assembleDebug` **第一次直接失败**，暴露出 3 个 `javap` 无法反映的事实：

**事实 1：`@UnstableSaltUiApi` 在本工程是编译错误，不是警告。** 首次编译输出原文（节选）：

```text
> Task :app:compileDebugKotlin FAILED
e: .../SaltSpikeExtra.kt:36:9 This Salt UI API is experimental and may change or be removed in the future.
e: .../SaltSpikeExtra.kt:37:9 This Salt UI API is experimental and may change or be removed in the future.
e: .../SaltSpikeExtra.kt:38:9 This Salt UI API is experimental and may change or be removed in the future.
e: .../SaltSpikeExtra.kt:43:5 This Salt UI API is experimental and may change or be removed in the future.
e: .../SaltSpikeExtra.kt:58:9 This Salt UI API is experimental and may change or be removed in the future.
BUILD FAILED in 26s
```
命中的组件：`ItemCheck` / `ItemSelect` / `ItemEdit` / `ItemSlider` / `ItemButton` / `PopupMenu` / `PopupMenuItem` / `YesNoDialog` / `InputDialog` / `ItemOuterTitle` / `rememberPopupState`。
解法只有两个：文件/函数加 `@OptIn(UnstableSaltUiApi::class)`（本工程采用），或在 `build.gradle.kts` 加 `freeCompilerArgs += "-opt-in=com.moriafly.salt.ui.UnstableSaltUiApi"`。
（`Item` / `ItemSwitcher` / `ItemValue` / `RoundedColumn` / `Button` / `SaltTheme` / `saltConfigs` / `Text` / `Surface` **不需要** opt-in，所以 `SaltSpike.kt` 之前能直接编过。）

**事实 2：`ItemSelect` 的参数是 `value` + `content`，不是 `sub`。** 同一份报错里的原文：

```text
e: .../SaltSpikeExtra.kt:37:48 No parameter with name 'sub' found.
e: .../SaltSpikeExtra.kt:37:48 No value passed for parameter 'value'.
e: .../SaltSpikeExtra.kt:37:48 No value passed for parameter 'content'.
```
即 2.5.2 的真实签名是 `ItemSelect(state: PopupState, text: String, value: String, modifier, enabled, iconPainter, iconPaddingValues, iconColor, content: @Composable ColumnScope.() -> Unit)`，**`content` 没有默认值**，必须传入（下拉项就写在这个 content 里）。3.0 的 `ItemPopup(state, text, sub, ...)` 写法在 2.5.2 上编不过。

**事实 3：`PopupMenuItem` 已弃用。** 修好前两条后的编译 warning 原文：

```text
w: .../SaltSpikeExtra.kt:40:13 'fun PopupMenuItem(onClick: () -> Unit, selected: Boolean? = ...,
   text: String, sub: String? = ..., iconPainter: Painter? = ..., iconPaddingValues: PaddingValues = ...,
   iconColor: Color? = ...): Unit' is deprecated. Use Item instead.
```
（这条 warning 也顺带给出了 `PopupMenuItem` 的**真实 Kotlin 参数名与默认值**：`onClick, selected: Boolean? = null, text, sub: String? = null, iconPainter = null, iconPaddingValues, iconColor`。）

修正后 `assembleDebug` 结果（`build/assemble7.txt`，原路径 `E:\桌面\安卓软件\NarMusicAndroid` 运行，且已包含全部 12 个 B 级组件 + `ItemDivider`）：

```text
> Task :app:compileDebugKotlin
w: .../SaltSpikeExtra.kt:41:13 'fun PopupMenuItem(...)' is deprecated. Use Item instead.
> Task :app:assembleDebug
BUILD SUCCESSFUL in 31s
```
该文件同时是**验收标准 1 的直接证据**（`:app:assembleDebug` 退出码 0，未用 `subst` 盘符，`ItemDivider()` 也在其中编过）。

---

## 4. X-3 最小样例 + Robolectric 真实渲染

### 4.1 `app/src/main/java/com/narnat/music/ui/spike/SaltSpike.kt`（完整文件，已编译通过）

```kotlin
package com.narnat.music.ui.spike

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.moriafly.salt.ui.Button as SaltButton
import com.moriafly.salt.ui.ButtonType
import com.moriafly.salt.ui.Item
import com.moriafly.salt.ui.ItemArrowType
import com.moriafly.salt.ui.ItemSwitcher
import com.moriafly.salt.ui.ItemValue
import com.moriafly.salt.ui.RoundedColumn
import com.moriafly.salt.ui.SaltTheme
import com.moriafly.salt.ui.saltConfigs

@Composable
fun SaltSpikeScreen(
    title: String = "纳儿音乐",
    subtitle: String = "SaltUI 2.5.2 渲染验证",
    darkTheme: Boolean = false,
    onBack: () -> Unit = {},
    onPrimaryAction: () -> Unit = {}
) {
    SaltTheme(configs = saltConfigs(isDarkTheme = darkTheme)) {
        var switched by remember { mutableStateOf(false) }
        var listClicks by remember { mutableStateOf(0) }
        val colors = SaltTheme.colors
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(colors.background)
                .padding(16.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "返回",
                    fontSize = 14.sp,
                    color = colors.highlight,
                    modifier = Modifier
                        .clickable(onClick = onBack)
                        .semantics { contentDescription = "返回" }
                        .padding(end = 12.dp)
                )
                Column(modifier = Modifier.fillMaxWidth()) {
                    Text(
                        text = title,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = colors.text
                    )
                    Text(text = subtitle, fontSize = 12.sp, color = colors.subText)
                }
            }

            RoundedColumn(
                modifier = Modifier.fillMaxWidth(),
                color = colors.subBackground
            ) {
                Item(
                    onClick = {},
                    text = "歌词提取平台",
                    sub = "网易云音乐",
                    arrowType = ItemArrowType.Arrow
                )
                ItemSwitcher(
                    state = switched,
                    onChange = { switched = it },
                    text = "歌词同步",
                    sub = if (switched) "已开启" else "已关闭"
                )
                ItemValue(text = "下载延迟", sub = "0 ms")
            }

            SaltButton(
                onClick = onPrimaryAction,
                text = "开始下载",
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 12.dp),
                type = ButtonType.Highlight
            )

            LazyColumn(modifier = Modifier.fillMaxWidth()) {
                items(SaltSpikeTracks) { track ->
                    Item(
                        onClick = { listClicks += 1 },
                        text = track,
                        sub = "第 $listClicks 次点击",
                        arrowType = ItemArrowType.None
                    )
                }
            }
        }
    }
}

private val SaltSpikeTracks = listOf("晴天 - 周杰伦", "夜曲 - 周杰伦", "稻香 - 周杰伦")
```

与任务书 X-3 的差异（**必须记录**）：`BasicScreen` / `ScreenCard` / `PillButton` / `LabelValue` 在 2.5.2 不存在，因此

| 任务书要求 | 2.5.2 实际写法 |
|---|---|
| `SaltTheme { }` 包裹 | ✅ 可用，并额外用 `saltConfigs(isDarkTheme = ...)` 控制明暗 |
| `BasicScreen`（标题栏 + 返回按钮） | 自建 `Row` + `clickable` 的「返回」+ 标题/副标题（Material3 `Text` 取 Salt 色板） |
| `ScreenCard` 里放 `Item` | `RoundedColumn(color = SaltTheme.colors.subBackground)` + `Item` / `ItemSwitcher` / `ItemValue` |
| `LabelValue` 或 `Switch` | `ItemValue(text, sub)`（`LabelValue` 不存在）+ `ItemSwitcher`（`Switcher` 无 `onChange` 重载） |
| `PillButton` | `Button(onClick, text, type = ButtonType.Highlight)`（2.5.2 的胶囊按钮） |
| `LazyList` / 普通 `Column` 列表 | `LazyColumn` + `Item(...)`（`LazyList` 在 2.5.2 没有公开入口） |

### 4.2 `app/src/test/java/com/narnat/music/ui/spike/SaltSpikeRenderTest.kt`（完整文件）

```kotlin
package com.narnat.music.ui.spike

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class SaltSpikeRenderTest {

    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun titleIsRendered() {
        composeRule.setContent { SaltSpikeScreen() }
        composeRule.onNodeWithText("纳儿音乐").assertExists()
        composeRule.onNodeWithText("歌词提取平台").assertExists()
    }

    @Test
    fun primaryButtonClickInvokesCallback() {
        var clicked = false
        composeRule.setContent {
            SaltSpikeScreen(onPrimaryAction = { clicked = true })
        }
        composeRule.onNodeWithText("开始下载").performClick()
        assertTrue(clicked)
    }

    @Test
    fun switcherClickTogglesState() {
        composeRule.setContent { SaltSpikeScreen() }
        composeRule.onNodeWithText("已关闭").assertExists()
        composeRule.onNodeWithText("歌词同步").performClick()
        composeRule.onNodeWithText("已开启").assertExists()
    }
}
```

### 4.3 测试结果（真实通过）

```bat
subst N: "E:\桌面\安卓软件"
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:testDebugUnitTest --tests *SaltSpikeRenderTest*"
```

```
> Task :app:testDebugUnitTest
BUILD SUCCESSFUL in 29s              ← 退出码 0（build\test11.txt，强制重跑）
```

统计原文（`app/build/test-results/testDebugUnitTest/TEST-com.narnat.music.ui.spike.SaltSpikeRenderTest.xml` 首行）：

```xml
<testsuite name="com.narnat.music.ui.spike.SaltSpikeRenderTest" tests="3" skipped="0" failures="0" errors="0"
           timestamp="2026-11-01T04:43:44.643Z" hostname="NARNAT" time="8.838">
```

三个用例名：`titleIsRendered` / `primaryButtonClickInvokesCallback` / `switcherClickTogglesState`（全部 pass）。

> 首次运行时 Robolectric 需要下载 `org.robolectric:android-all-instrumented`（`@Config(sdk=[34])`），耗时约 3 分 45 秒；之后增量约 30~35 秒。
> 上面这次是**删掉 `app\build\test-results\testDebugUnitTest` 强制重跑**的结果（`build/test11.txt` = `BUILD SUCCESSFUL in 29s`，`> Task :app:testDebugUnitTest` 实际执行为 29s 而非 UP-TO-DATE），发生时间在 C 盘缓存清理之后，用于证明删掉 C 盘残留后测试仍可用 `E:\gradle-home` 的构件真实跑通。

---

## 5. X-4 判定、白名单 / 黑名单、可直接照抄的片段

### 5.1 结论

**可以用 SaltUI 2.5.2 作架构基线**（主题、颜色、字号、圆角、Item 行、RoundedColumn 分组卡、Button、PopupMenu、Dialog 都能用且已实测渲染），**但 3.0 的 5 个"骨架组件"必须自建**。

白名单的可信度分两级，报告里严格区分：
- **A 级（本工程真实编译 + Robolectric 真实渲染/点击通过）**：`SaltTheme` + `saltConfigs`、`SaltTheme.colors`、`RoundedColumn`、`Item`、`ItemSwitcher`、`ItemValue`、`Button(ButtonType.Highlight|Sub)`、`LazyColumn` + `Item`。
- **B 级（真实编译通过，未做 Robolectric 断言）**：`ItemCheck`、`ItemSelect`(+`ItemOuterTitle`/`PopupState`)、`ItemEdit`、`ItemSlider`、`ItemButton`、`ItemDivider`、`PopupMenu`、`PopupMenuItem`、`YesNoDialog`、`InputDialog` —— 证据是 `SaltSpikeExtra.kt` 参与 `assembleDebug` 成功（第 3.3 节，`build/assemble7.txt`）。

### 5.2 可用组件白名单（2.5.2 实测存在）

`SaltTheme { }` · `ChangeSaltThemeIsDark` · `SaltTheme.configs/colors/textStyles/dimens/shapes` · `saltConfigs(...)` · `lightSaltColors/darkSaltColors` · `SaltDynamicColors.default()` · `SaltPalette`（12 色）· `saltTextStyles(...)`（main/sub/paragraph 三档）· `saltDimens(...)` · `SaltShapes.default(...)` · `RoundedColumn` · `Item` · `ItemSwitcher` · `ItemValue` · `ItemCheck` · `ItemEdit` / `ItemEditPassword` · `ItemSlider` · `ItemButton` · `ItemSelect`（配 `PopupState` / `rememberPopupState`）· `ItemInfo` · `ItemSpacer` / `ItemContainer` / `ItemDivider` · `ItemTitle` / `ItemTip` / `ItemText`（已弃用但存在）· `ItemOuterTitle` / `ItemOuterTip` / `ItemOuterLargeTitle` / `ItemOuterEdit` / `ItemOuterTextButton` / `ItemOuterSpacer` · `Button` / `TextButton` / `BasicButton`（`ButtonType.Highlight|Sub`）· `Switcher(state, modifier)`（纯指示器）· `Text` / `Surface` / `Icon` · `popup.PopupMenu` / `popup.PopupMenuItem`（**已弃用**，官方建议换 `Item`）· `dialog.YesDialog` / `YesNoDialog` / `InputDialog` / `BasicDialog` / `DialogTitle` / `BottomSheetDialog` · `icons.SaltIcons`

> **使用门槛（3.3 事实 1）**：`ItemCheck`/`ItemEdit`/`ItemSlider`/`ItemButton`/`ItemSelect`/`ItemOuterTitle`/`PopupMenu`/`PopupMenuItem`/`YesNoDialog`/`InputDialog`/`rememberPopupState` 全部带 `@UnstableSaltUiApi`，调用处必须 `@OptIn(UnstableSaltUiApi::class)`，否则编译失败。

### 5.3 必须改用 Material3 自建的黑名单（2.5.2 不存在或不可用）

| 3.0 组件 | 2.5.2 状态 | 替代方案 |
|---|---|---|
| `BasicScreen`（标题栏 + 返回 + 内容 padding） | 缺失 | `Scaffold` + `TopAppBar`，或自建 `Row`（示例已给） |
| `ScreenCard`（带 header/footer 的卡） | 缺失 | `RoundedColumn` 或 M3 `Card` |
| `TitleBarButton` | 缺失 | M3 `IconButton` |
| `PillButton` / `BasicPillButton` / `LocalControlSize` / `ControlSize` | 缺失 | **`Button(type = ButtonType.Highlight)`**（本工程已实测）或 M3 胶囊按钮 |
| `Sheet` / `SheetState` / `SheetProperties` / `SheetScope` | 缺失（2.5.2 只有 View 版 `BottomSheetDialog`） | M3 `ModalBottomSheet` |
| `LabelValue` | 缺失 | **`ItemValue(text, sub)`**（示例已用） |
| `Card` / `CardDefaults` | 缺失 | M3 `Card` 或 `RoundedColumn` |
| `SideBar` | 缺失 | M3 `NavigationRail` |
| `LazyList` / `rememberLazyListState`(Salt 版) | 缺失（3.0 也是 internal） | `androidx.compose.foundation.lazy.LazyColumn` |
| `Switcher(state, onChange, modifier)` | 只有 `Switcher(state, modifier)` | 用 `ItemSwitcher`（行级开关，已实测点击翻转） |
| `SaltTextStyles.largeTitle`（24sp SemiBold） | 缺失（只有 main/sub/paragraph） | 自行 `TextStyle(fontSize = 24.sp, fontWeight = FontWeight.SemiBold)` |
| `SaltColors.success/warning/error` | 缺失（只有 8 个槽位） | 用 `SaltPalette.SuccessLightIcon` 等 12 色常量 |
| `SaltMaterial` / `LocalSaltMaterial` / 液态玻璃 | 缺失（2.5.2 无 haze 依赖） | 不使用 |
| `SaltApplication` / `edgeToEdge()` / `WindowUtil` | 部分存在（`util.WindowUtil`、`ext` 包在，但无 `app/SaltApplication`） | 按需自建 |
| `ItemDropdown`（3.0 新增） | 缺失 | 用 `ItemSelect(state, text, value, content)` |
| `ItemPopup(state, text, sub, …)` 的 3.0 写法 | 2.5.2 有同名函数但签名是 `(state, text, value, content)`，且与 `ItemSelect` 完全相同 | 统一按 `ItemSelect` 写；`sub` 名称不存在 |
| `PopupMenuItem` | 存在但**已 `@Deprecated`**（"Use Item instead."） | 下拉项直接用 `Item(onClick, text, sub, arrowType = None)` |
| 任何 `@UnstableSaltUiApi` 组件 | 不加 opt-in 就**编译失败**（不是 warning） | 调用处 `@OptIn(UnstableSaltUiApi::class)` |

### 5.4 可直接照抄的组件用法（每类一个片段，取自本工程已编译文件）

```kotlin
// 1) 主题外壳 + 明暗（saltConfigs 是 2.5.2 的构造方式，不是 SaltConfigs.default()）
import com.moriafly.salt.ui.SaltTheme
import com.moriafly.salt.ui.saltConfigs

@Composable
fun Screen(dark: Boolean, content: @Composable () -> Unit) {
    SaltTheme(configs = saltConfigs(isDarkTheme = dark)) { content() }
}

// 2) 取色板（8 个槽位：highlight / text / subText / background / subBackground / popup / stroke / onHighlight）
val colors = SaltTheme.colors
Column(Modifier.fillMaxSize().background(colors.background).padding(16.dp)) {
    Text(text = "纳儿音乐", color = colors.text, fontSize = 20.sp, fontWeight = FontWeight.SemiBold)
    Text(text = "副标题", color = colors.subText, fontSize = 12.sp)
}
```

```kotlin
// 3) 分组卡片 + 设置行（对应 Web 的 .status-card + 设置项）
import com.moriafly.salt.ui.*

RoundedColumn(modifier = Modifier.fillMaxWidth(), color = SaltTheme.colors.subBackground) {
    Item(onClick = {}, text = "歌词提取平台", sub = "网易云音乐", arrowType = ItemArrowType.Arrow)
    ItemSwitcher(state = on, onChange = { on = it }, text = "歌词同步", sub = if (on) "已开启" else "已关闭")
    ItemValue(text = "下载延迟", sub = "0 ms")
}
// 需要 ItemCheck / ItemButton / ItemSlider / ItemDivider 时见片段 6（那些必须 @OptIn）
```

```kotlin
// 4) 胶囊按钮（PillButton 的 2.5.2 等价物）
import com.moriafly.salt.ui.Button as SaltButton
import com.moriafly.salt.ui.ButtonType

SaltButton(
    onClick = { /* 开始下载 */ },
    text = "开始下载",
    modifier = Modifier.fillMaxWidth().padding(vertical = 12.dp),
    type = ButtonType.Highlight
)
SaltButton(onClick = { }, text = "保存到本地", type = ButtonType.Sub)
```

```kotlin
// 5) 列表（SaltUI 的 Item 配 LazyColumn）
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import com.moriafly.salt.ui.Item
import com.moriafly.salt.ui.ItemArrowType

LazyColumn(modifier = Modifier.fillMaxWidth()) {
    items(tracks) { t ->
        Item(onClick = { play(t) }, text = t, sub = "128ms · 3.2 MB", arrowType = ItemArrowType.None)
    }
}
```

```kotlin
// 6) 下拉菜单 + 复选/输入/滑杆（库页排序、批量工具栏）
//    注意：这些 API 都是 @UnstableSaltUiApi，必须 OptIn；ItemSelect 的第 3 参数是 value，content 必填
@OptIn(UnstableSaltUiApi::class)
@Composable
fun ToolbarAndMenu() {
    val popup = rememberPopupState()
    var checked by remember { mutableStateOf(false) }
    var editText by remember { mutableStateOf("") }
    var sliderValue by remember { mutableStateOf(0.5f) }

    RoundedColumn(modifier = Modifier.fillMaxWidth(), color = SaltTheme.colors.subBackground) {
        ItemOuterTitle("批量操作")
        ItemCheck(state = checked, onChange = { checked = it }, text = "全选")
        ItemSelect(state = popup, text = "排序", value = "全部") {
            PopupMenuItem(onClick = { popup.dismiss() }, text = "A-Z", selected = true)
            PopupMenuItem(onClick = { popup.dismiss() }, text = "最新")
        }
        ItemEdit(text = editText, onChange = { editText = it }, hint = "输入密码")
        ItemSlider(value = sliderValue, onValueChange = { sliderValue = it }, text = "音量")
        ItemButton(onClick = { }, text = "批量删除", primary = false)
        ItemDivider()
    }

    PopupMenu(expanded = popup.expend, onDismissRequest = { popup.dismiss() }) {
        PopupMenuItem(onClick = { popup.dismiss() }, text = "A-Z", selected = true)
        PopupMenuItem(onClick = { popup.dismiss() }, text = "最新")
    }
}
```

```kotlin
// 7) 对话框（批量删除确认 / 密码输入）
@OptIn(UnstableSaltUiApi::class)
@Composable
fun Dialogs() {
    var confirmVisible by remember { mutableStateOf(false) }
    var inputVisible by remember { mutableStateOf(false) }
    var password by remember { mutableStateOf("") }

    if (confirmVisible) {
        YesNoDialog(
            onDismissRequest = { confirmVisible = false },
            onConfirm = { confirmVisible = false },
            title = "删除确认",
            content = "确定要删除选中的文件吗？"
        )
    }
    if (inputVisible) {
        InputDialog(
            onDismissRequest = { inputVisible = false },
            onConfirm = { inputVisible = false },
            title = "文件保护",
            text = password,
            onChange = { password = it }
        )
    }
}
```

片段 6/7 取自工程里的 `app/src/main/java/com/narnat/music/ui/spike/SaltSpikeExtra.kt`，为便于阅读把原文件的单个 `SaltSpikeExtraComponents()` 拆成两个函数，**与文件逐字差异只有 3 行**：两个函数名（`ToolbarAndMenu()` / `Dialogs()`）、以及片段 6 里 `ItemButton(onClick = { }, …)` 的 `onClick`（原文件是 `{ confirmVisible = true }`，拆分后不在该函数作用域）。其余每一行都与编译通过的文件相同（`build/assemble7.txt` = `BUILD SUCCESSFUL in 31s`；片段 7 在文件内是同一函数里的 `if (confirmVisible) { … }` 与 `if (inputVisible) { … }` 两段）。

> 片段 1–5 属于 A 级证据（已渲染 + 已点击验证），片段 6–7 属于 B 级证据（真实编译通过，未做 Robolectric 断言）。

---

## 6. 2.5.2 与 SaltUI 3.0（`E:\桌面\安卓软件\SaltUI` 源码）的 API 差异清单

| # | 差异 | 2.5.2 实测 | 3.0 源码 | 影响 |
|---|---|---|---|---|
| 1 | `SaltTheme(...)` 参数 | `(configs, dynamicColors, textStyles, dimens, shapes, content)` 6 参 | 多出 `material: SaltMaterial` 与 `basicScreenStyle: BasicScreenStyle` 8 参 | 液态玻璃与标题栏背景样式不可用 |
| 2 | `SaltTheme` 对象成员 | `configs/colors/textStyles/dimens/shapes` 5 个 | 多 `material` / `basicScreenStyle` 2 个 | 不能读 `SaltTheme.material` |
| 3 | `SaltTextStyles` 字段 | 3 个：`main` / `sub` / `paragraph` | 4 个，多 `largeTitle`（24sp SemiBold） | **Web `.section-title` 的 24sp 无法直接复用**，要自建 TextStyle |
| 4 | `SaltColors` 槽位 | 8 个（highlight/text/subText/background/subBackground/popup/stroke/onHighlight） | 11 个，多 `success` / `warning` / `error` | Toast/徽标的语义色要自己接 `SaltPalette` |
| 5 | `SaltColors` 工厂 | `lightSaltColors(...)` / `darkSaltColors(...)` 顶层函数 + 8 参 `copy` | 走 `SaltColors.defaultLight()/defaultDark()` companion | 2.5.2 构造自定义色板要用顶层函数 |
| 6 | `SaltConfigs` | 顶层 `saltConfigs(isDarkTheme, indication)`，**无 companion** | `SaltConfigs.default(isDarkTheme, indication)` | 主题入口写法不同 |
| 7 | `SaltDimens` | 顶层 `saltDimens(item, itemIcon, corner, dialogCorner, padding, subPadding)`，**无 companion** | 有 `companion.default(...)` 且 `corner/dialogCorner` 标 `@Deprecated` | 2.5.2 的 `corner=12dp` / `dialogCorner=20dp` 仍可用（`getCorner-D9Ej5fM$annotations` 已在字节码里） |
| 8 | `BasicScreen` / `BasicScreenDefaults` / `BasicScreenStyle` / `TitleBarBackdropType` | **完全缺失** | `screen/BasicScreen.kt`（98–624 行） | 标题栏必须自建 |
| 9 | `ScreenCard` | **缺失** | `screen/ScreenCard.kt` | 带 header/footer 的卡片必须自建 |
| 10 | `TitleBarButton` / `TitleBarButtonDefaults` | **缺失** | `screen/TitleBarButton.kt` | 用 M3 IconButton |
| 11 | `PillButton` / `BasicPillButton` / `ControlSize` / `LocalControlSize` | **缺失** | `button/PillButton.kt` + `ControlSize.kt` | 胶囊主按钮用 `Button(type=Highlight)` |
| 12 | `Sheet` / `SheetState` / `SheetProperties` / `SheetScope` / `SheetHost` | **缺失**（仅 `dialog/BottomSheetDialogKt` View 版） | `sheet/` 包 4 个文件 | 播放器弹层用 M3 `ModalBottomSheet` |
| 13 | `LabelValue` | **缺失** | `LabelValue.kt`（label 固定 96dp） | 用 `ItemValue` |
| 14 | `Card` / `CardDefaults` | **缺失** | `Card.kt` | 用 `RoundedColumn` 或 M3 Card |
| 15 | `SideBar` | **缺失** | `sidebar/SideBar.kt`（220dp 固定宽） | 用 M3 NavigationRail |
| 16 | `Switcher` 重载 | 只有 `Switcher(state, modifier)` | 另有 `@UnstableSaltUiApi Switcher(state, onChange, modifier)` | 独立可点开关只能靠 `ItemSwitcher` |
| 17 | `Button` 语义 | `Button(onClick, text, modifier, enabled, type: ButtonType{Highlight,Sub})` | `Button(onClick, text, modifier, enabled, appearance: ButtonAppearance, intent: ButtonIntent, size: ControlSize, leadingIcon, maxLines)` | 参数体系完全不同，2.5.2 不能照抄 3.0 的按钮代码 |
| 18 | `Switcher` 的实现基底 | `BasicSwitcher` 为 internal（无公开类） | 同样 internal，但多 `onChange` 公开重载 | 同 16 |
| 19 | `Item` 参数 | 12 个（onClick/text/modifier/enabled/iconPainter/iconPaddingValues/iconColor/textColor/sub/subColor/subContent/arrowType） | S-3 记录还有 `tag: String?` | 2.5.2 的 `Item` **没有** `tag` |
| 20 | `RoundedColumn` | 1 个重载：`(modifier, color, content)`，**无 `paddingValues`、无 `RoundedColumnType`** | 另有 `paddingValues` 默认值与 `RoundedColumnType{Default,InList}` | 列表内边距要自己加 |
| 21 | `ItemImpl.kt` 内部实现 | 公开类 `ItemImplKt`（含 `ItemEditImpl` 等，`internal`） | 同 internal | 两者都不可外部调用 |
| 22 | 下拉菜单 | `popup.PopupMenu` + `ItemSelect` + `ItemPopup`（`PopupState.expend` 拼写） | 另有 `ItemDropdown.kt`，`ItemSelect` 仍在（`Item.kt:382`） | 2.5.2 无 `ItemDropdown` |
| 23 | 模糊/毛玻璃链路 | **无** `dev.chrisbanes.haze`（`dependencies` 里 `<absent>`） | `haze`/`haze-blur`/`haze-blur-materials:2.0.1` | 2.5.2 不会有 haze 相关崩溃，也没有毛玻璃标题栏 |
| 24 | 额外平台依赖 | 仅 `hiddenapibypass:6.1` + `material:1.12.0`；**无** `graphics-shapes` / `collection:1.5.0` | 多 `graphics-shapes:1.0.1`、`collection:1.5.0`、`components-resources` | 依赖面更小 |
| 25 | Compose 版本 | `org.jetbrains.compose.*:1.8.2` + `material3:1.2.1`（被本工程提到 1.3.2） | `org.jetbrains.compose.*:1.12.2` + `material3:1.4.0` | 2.5.2 才能和 Kotlin 2.1.20 兼容 |
| 26 | 类文件版本 | `SaltColors` 等 `major version 65`（Java 21） | 同样 65 | **本工程 `jvmTarget=17` 也能读 65 吗？** 见下方披露第 3 条 |
| 27 | `ItemSelect` 参数 | `(state, text, value, modifier, enabled, iconPainter, iconPaddingValues, iconColor, content: ColumnScope.() -> Unit)`，`content` 无默认值 | 3.0 同名/同区域对应 `ItemPopup(state, text, sub, …)`，另有 `ItemDropdown` | 按 3.0 写 `sub = ...` 直接编译报 `No parameter with name 'sub' found`（3.3 事实 2） |
| 28 | opt-in 门槛 | `@UnstableSaltUiApi` 组件**不 OptIn 即编译失败**（error 级，非 warning） | 3.0 同样有该注解，但 3.0 工程在 `build.gradle.kts` 里统一 opt-in | 2.5.2 接入后每个用到的文件都要 `@OptIn`，或全工程加 `-opt-in` 编译参数（3.3 事实 1） |
| 29 | `PopupMenuItem` 生命周期 | 存在但 `@Deprecated("Use Item instead.")` | 3.0 仍在 `popup/PopupMenuItem.kt` | 新代码应直接用 `Item` 当下拉项（3.3 事实 3） |
| 30 | 弃用 API 集合 | 实测确认弃用的只有 `PopupMenuItem`（编译期 warning 原文在 3.3）；`ItemTitle` / `ItemText` / `ItemPopup` 的弃用状态来自 `docs/ui-spec.md` S-3.3 的记录，本次未逐个编译验证 | 3.0 已把多数替换为 `Item` / `ItemDropdown` | 新代码优先用 `Item` / `ItemValue` / `ItemSelect`，不用这几个旧入口 |

---

## 7. 主动披露：与 `docs/ui-spec.md` S-3 结论不一致 / S-3 未覆盖之处

1. **S-3.5 的"路径 B 可行"得到验证**，但 S-3 的表里写"2.5.2 缺 `BasicScreen/ScreenCard/TitleBarButton/PillButton/Sheet`"——实测还**多缺** `LabelValue`、`Card`、`SideBar`、`ControlSize`、`SaltMaterial`，并且 `SaltTextStyles` 只有 3 档（无 `largeTitle`）。即 S-0 结论表里"S-3.5 推荐 B"仍然成立，但"可用组件"的盘点偏乐观，S-3.3 里 25 条签名**只有约一半在 2.5.2 存在**。
2. **`Switcher` 在 2.5.2 没有带 `onChange` 的重载**——S-3.2/S-3.3 未提，这会直接影响"开关"类交互（必须用 `ItemSwitcher`，或自己在 `Switcher` 外包 `clickable`）。
3. **`jvmTarget` 与 AAR 的类文件版本**：AAR 内 `SaltColors` 是 `major version 65`（Java 21 编译），而本工程 `compileOptions/jvmTarget = 17`。实测在 `jvmTarget=17` + AGP 8.9.1 下 **能编译能打包能运行**（D8 接受 65 版本输入并按 minSdk 26 反糖），所以 S-3.2 里"建议把 jvmTarget 提到 21"**不是必需**，本次未改动编译目标。
4. **`material3` 实际停在 1.3.2**（SaltUI 声明 1.2.1），S-3.4 表里"`2.5.2` → `1.8.2` + `material3:1.2.1`"只描述了 SaltUI 的**声明**，实际解析到的是 `1.3.2`（BOM 更高版本胜出）；`androidx.compose.ui` 则被 SaltUI 从 BOM 的 1.8.0 拉到 **1.8.2**。
5. **包名澄清**：Maven `io.github.moriafly` ≠ Java 包 `com.moriafly.salt.ui`。`docs/ui-spec.md` 的 S-3.3 用的是 `com.moriafly.salt.ui`（正确），但任务书 X-1.3/X-2 写的是 `io/github/moriafly/salt/ui/`（在 APK/AAR 里不存在，别据此判定失败）。
6. **S-3 未提及的运行环境问题**：本工程路径含中文时，任何 `testDebugUnitTest`（不只是 Compose 测试）都会 `ClassNotFoundException`，原因是 Gradle worker 的 `@argfile` classpath 用 UTF-8 写、JDK launcher 按 GBK 读。后续所有"界面任务必须带 Robolectric 测试"的任务都会踩这个坑，**建议固定用 `subst` 盘符跑单元测试**（本报告第 1 节给了命令）。
7. **S-3 完全没提 `@UnstableSaltUiApi` 的 opt-in 门槛**。2.5.2 里 `ItemCheck`/`ItemEdit`/`ItemSlider`/`ItemButton`/`ItemSelect`/`PopupMenu`/`PopupMenuItem`/`YesNoDialog`/`InputDialog`/`rememberPopupState` 全带该注解，**不 OptIn 就编译不过**（本报告 3.3 附报错原文）。这意味着后续界面任务的每个文件顶部都要 `@OptIn(UnstableSaltUiApi::class)` —— 若嫌麻烦，可在 `app/build.gradle.kts` 的 `kotlinOptions` 里统一加 `freeCompilerArgs += "-opt-in=com.moriafly.salt.ui.UnstableSaltUiApi"`（本次**未**这样改，因为题目只允许在 dependencies 块追加一行）。
8. **S-3 说"`LabelValue`/`Card` 等在 2.5.2 存在"是错的**（S-3.3 第 7、13 条把它们列成可用组件）：实测 `LabelValueKt`、`CardKt` 都不存在（`javap` 报错原文在 3.1）。
9. **S-3.3 第 22 条写"`LazyList` 是 internal、用 `LazyColumn` 即可"** —— 实测 2.5.2 的 `classes.jar` 里**连 `LazyListKt` 类都没有**（`jar tf` 228 条条目中 `lazy` 命中 0 次，`Label`/`Card`/`SideBar`/`ControlSize`/`SaltApplication` 也均为 0 次），所以不是"internal 不可用"而是"整个 `lazy/` 包都不存在"。结论（用 `LazyColumn`）不变。
10. **边界自查（含一条无法归因的外部改动）**：本会话的写操作目标只有 5 个文件 —— `app/build.gradle.kts`（追加 1 行）、`app/src/main/java/com/narnat/music/ui/spike/SaltSpike.kt`、`app/src/main/java/com/narnat/music/ui/spike/SaltSpikeExtra.kt`、`app/src/test/java/com/narnat/music/ui/spike/SaltSpikeRenderTest.kt`、`docs/saltui-spike.md`（另有 `build\` 下的诊断脚本/日志，均在 build 目录内）；`MainActivity.kt` / `NarMusicApp.kt` / `AndroidManifest.xml` / `settings.gradle.kts` / `gradle.properties` / `core,data,domain,di` / `docs/{ui-spec,api-probe,build-versions}.md` / `NarMusic/**` / `SaltUI/**` 均未写入。**无法归因项**：`docs/task-B-core.md` 的 `CreationTime = LastWriteTime = 2026/11/1 12:33:03`（`Get-Item` 输出，内容为「任务 B：核心机制与数据层实现」任务书，共 252 行 / 26,163 字节），该文件是在本会话时间窗内**新建**的，且属于「禁止改动 docs/task-*.md」范围；本会话对该文件没有任何读写调用记录（本会话唯一新建的 docs 文件是 `docs/saltui-spike.md`），但**无法提供文件系统级的写入者证据**，故结论标注为「**无法归因**」（可能是并行会话/其它工具写入），不作"非本会话所写"的断言。
11. **C 盘缓存的违规与补救**：第一次环境变量写法出错（`set "VAR=value"` 静默失效）时，Gradle 回落到 `C:\Users\86153\.gradle`，把 `io.github.moriafly` 的 4 个构件包（10 个文件，mtime 12:10:30–12:12:07）写进了 C 盘，违反了「禁止把缓存写到 C 盘」。该问题已**修复并清理**：确认这 4 个目录内只有 `2.5.2` 一个版本后整目录删除（含空壳父目录 `io.github.moriafly`），C 盘原有的 `io.coil-kt`/`io.grpc`/`io.netty`/`io.perfmark` 与目录总数 137 均未变化；清理后重跑 `assembleDebug`（`BUILD SUCCESSFUL in 18s`）与 `testDebugUnitTest`（强制重跑，`tests=3 failures=0`，`BUILD SUCCESSFUL in 29s`），并复查 C 盘不再出现 `moriafly` 目录（构件只在 `E:\gradle-home\caches\...`）。清理前后 `dir` 原文见第 1 节「C 盘残留的清理」。

---

## 8. 复现清单

```bat
:: 0) 中文路径变通（单元测试必需）
subst N: "E:\桌面\安卓软件"

:: 1) 依赖解析
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:dependencies --configuration debugRuntimeClasspath"

:: 2) 打包（退出码 0，原路径即可，build\assemble8.txt = BUILD SUCCESSFUL in 18s，清理 C 盘缓存后重跑）
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d E:\桌面\安卓软件\NarMusicAndroid&& gradlew.bat --no-daemon :app:assembleDebug"

:: 3) Robolectric 渲染测试（3 passed / 0 failed，build\test11.txt = BUILD SUCCESSFUL in 29s）
cmd /c "set GRADLE_USER_HOME=E:\gradle-home&& set ANDROID_HOME=E:\android-sdk&& cd /d N:\NarMusicAndroid&& gradlew.bat --no-daemon :app:testDebugUnitTest --tests *SaltSpikeRenderTest*"

:: 4) 重新 dump API 签名（含缺失类的报错）
cd /d E:\桌面\安卓软件\NarMusicAndroid\build\saltspike\aar
E:\jdk\bin\javap.exe -classpath classes.jar com.moriafly.salt.ui.ItemKt
E:\jdk\bin\javap.exe -classpath classes.jar com.moriafly.salt.ui.BasicScreenKt

:: 5) 确认 C 盘无 moriafly 缓存（应无输出）；构件只在 E 盘
dir /b "C:\Users\86153\.gradle\caches\modules-2\files-2.1" | findstr /i moriafly
dir /b "E:\gradle-home\caches\modules-2\files-2.1" | findstr /i moriafly

:: 清理盘符映射
subst N: /d
```

本次产出的中间证据文件（都在 `build/`，可随时清理）：`build\deps2.txt`、`build\assemble3.txt`、`build\assemble4.txt`、`build\assemble5.txt`（失败原文）、`build\assemble6.txt`、`build\assemble7.txt`、`build\assemble8.txt`（清理 C 盘缓存后的最终打包证据）、`build\test7.txt`、`build\test8.txt`、`build\test9.txt`、`build\test10.txt`（UP-TO-DATE，未作证据）、`build\test11.txt`（强制重跑的最终测试证据）、`build\cdrive_before.txt` / `build\cdrive_after.txt`（C 盘清理前后 `dir` 原文）、`build\saltspike\classlist.txt`、`build\saltspike\javap_all.txt`、`build\saltspike\javap2.txt`、`build\saltspike\javap3.txt`、`build\saltspike\javap_missing.txt`、`build\saltspike\javap_popup.txt`；自查脚本 `build\check_report.py`、`build\check_report2.py`、`build\check_apk.py`、`build\check_apk2.py`、`build\check_doc.py`（分别用于比对报告里的 `javap` 原文、缺失类报错原文、APK dex 内的 SaltUI 类与包前缀统计、报告完整性）。
