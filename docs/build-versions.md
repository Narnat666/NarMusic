# NarMusic Android 构建版本矩阵

工程：`E:\桌面\安卓软件\NarMusicAndroid`（单模块 `:app`，namespace/applicationId = `com.narnat.music`）
记录时间：本机实际执行 `gradlew.bat --no-daemon assembleDebug` 成功（BUILD SUCCESSFUL in 4m 31s）后整理。

## 1. 版本矩阵

| 组件 | 实际使用版本 | 在哪个元数据 / 仓库确认存在 |
| --- | --- | --- |
| Gradle（wrapper 发行版） | 8.13 | `https://services.gradle.org/distributions/gradle-8.13-bin.zip` 返回 307 重定向，实际从 `https://mirrors.cloud.tencent.com/gradle/gradle-8.13-bin.zip` 下载（200，136,983,045 字节）；`gradle-8.13/bin/gradle --version` 输出 `Gradle 8.13 / Kotlin: 2.0.21 / Launcher JVM: 21.0.10` |
| AGP（com.android.application） | 8.9.1 | `https://dl.google.com/dl/android/maven2/com/android/tools/build/gradle/8.9.1/gradle-8.9.1.pom` → 200 |
| Kotlin（org.jetbrains.kotlin.android） | 2.1.20 | `https://repo1.maven.org/maven2/org/jetbrains/kotlin/kotlin-gradle-plugin/2.1.20/kotlin-gradle-plugin-2.1.20.pom` → 200 |
| Kotlin Compose 编译器插件 | 2.1.20 | `https://repo1.maven.org/maven2/org/jetbrains/kotlin/compose-compiler-gradle-plugin/2.1.20/compose-compiler-gradle-plugin-2.1.20.pom` → 200 |
| Kotlin serialization 插件 | 2.1.20 | `https://repo1.maven.org/maven2/org/jetbrains/kotlin/kotlin-serialization/2.1.20/kotlin-serialization-2.1.20.pom` → 200 |
| KSP（com.google.devtools.ksp） | 2.1.20-1.0.32 | `https://repo1.maven.org/maven2/com/google/devtools/ksp/symbol-processing-gradle-plugin/maven-metadata.xml` 版本列表含 `<version>2.1.20-1.0.32</version>`；插件标记 `.../com.google.devtools.ksp.gradle.plugin/2.1.20-1.0.32/...pom` → 200 |
| Compose BOM | 2025.04.01 | `https://dl.google.com/dl/android/maven2/androidx/compose/compose-bom/2025.04.01/compose-bom-2025.04.01.pom` → 200；解析后 compose-ui = 1.8.0、material3 = 1.3.2 |
| Room | 2.7.0（room-runtime / room-ktx / room-compiler，KSP 注解处理） | `https://dl.google.com/dl/android/maven2/androidx/room/room-runtime/2.7.0/room-runtime-2.7.0.pom` → 200；`.../room-compiler/2.7.0/room-compiler-2.7.0.pom` → 200 |
| Media3 ExoPlayer | 1.6.1（media3-exoplayer、media3-ui） | `https://dl.google.com/dl/android/maven2/androidx/media3/media3-exoplayer/1.6.1/media3-exoplayer-1.6.1.pom` → 200 |
| OkHttp | 4.12.0 | `https://repo1.maven.org/maven2/com/squareup/okhttp3/okhttp/4.12.0/okhttp-4.12.0.pom` → 200 |
| kotlinx-serialization-json | 1.8.1 | `https://repo1.maven.org/maven2/org/jetbrains/kotlinx/kotlinx-serialization-json/1.8.1/kotlinx-serialization-json-1.8.1.pom` → 200 |
| kotlinx-coroutines-android | 1.10.1 | `https://repo1.maven.org/maven2/org/jetbrains/kotlinx/kotlinx-coroutines-android/1.10.1/kotlinx-coroutines-android-1.10.1.pom` → 200 |
| Lifecycle ViewModel Compose | 2.8.7（另含 lifecycle-runtime-compose、lifecycle-runtime-ktx 2.8.7） | `https://dl.google.com/dl/android/maven2/androidx/lifecycle/lifecycle-viewmodel-compose/2.8.7/lifecycle-viewmodel-compose-2.8.7.pom` → 200 |
| activity-compose | 1.10.1 | `https://dl.google.com/dl/android/maven2/androidx/activity/activity-compose/1.10.1/activity-compose-1.10.1.pom` → 200 |
| core-ktx | 1.15.0 | `https://dl.google.com/dl/android/maven2/androidx/core/core-ktx/1.15.0/core-ktx-1.15.0.pom` → 200 |
| Coil（compose） | 2.7.0 | `https://repo1.maven.org/maven2/io/coil-kt/coil-compose/2.7.0/coil-compose-2.7.0.pom` → 200 |
| JUnit4（testImplementation） | 4.13.2 | `https://repo1.maven.org/maven2/junit/junit/4.13.2/junit-4.13.2.pom` → 200 |
| Robolectric（testImplementation） | 4.14.1 | `https://repo1.maven.org/maven2/org/robolectric/robolectric/4.14.1/robolectric-4.14.1.pom` → 200 |
| androidx.compose.ui:ui-test-junit4 | 由 Compose BOM 2025.04.01 管理（1.8.0） | Compose BOM pom 内的 dependencyManagement；`debugRuntimeClasspath` 外的 test 配置同样解析自 `https://dl.google.com/dl/android/maven2/androidx/compose/ui/ui-test-junit4/1.8.0/*.pom` |
| androidx.test.ext:junit | 1.2.1 | `https://dl.google.com/dl/android/maven2/androidx/test/ext/junit/1.2.1/junit-1.2.1.pom` → 200 |
| compileSdk / platform | 35（`platforms;android-35`） | `E:\android-sdk\platforms\android-35\android.jar` 存在，27,092,450 字节 |
| build-tools | 35.0.0 | `E:\android-sdk\build-tools\35.0.0\aapt2.exe` 存在，4,740,896 字节 |
| platform-tools（adb） | sdkmanager 当前 latest | `E:\android-sdk\platform-tools\adb.exe` 存在，8,273,560 字节 |
| JDK | 21.0.10（E:\jdk），编译 target 17 | `java -version` → `21.0.10` |

## 2. 与推荐组合的差异（必须说明）

1. **Gradle 发行版下载源替换**：`services.gradle.org` 直连会返回 `307`，跟随重定向后 `curl: (35) Recv failure: Connection was reset`；Java wrapper 报 `Downloading ... failed: timeout (10000ms)` / `java.net.SocketTimeoutException: Connect timed out`。因此 `gradle/wrapper/gradle-wrapper.properties` 的 `distributionUrl` 改为 `https\://mirrors.cloud.tencent.com/gradle/gradle-8.13-bin.zip`（同一 8.13 官方二进制，HTTP 200，137MB，11.4 秒）。**未使用任何代理**。
2. **gradle.properties 追加 `android.overridePathCheck=true`**：工程路径含中文，AGP 默认报错 `Your project path contains non-ASCII characters.`。
3. **GRADLE_USER_HOME 指向 E 盘**：设置用户级环境变量 `GRADLE_USER_HOME=E:\gradle-home`，避免依赖缓存与 wrapper 发行版写入剩余仅 13GB 的 C 盘。
4. **KSP + Room 未退化到 KAPT**：`2.1.20-1.0.32` + Room `2.7.0` 一次构建通过（`:app:kspDebugKotlin` 成功），无需替换。
5. 任务书未指定版本的库选了与 Kotlin 2.1.20 / compileSdk 35 同时代的稳定版：Media3 1.6.1（最新稳定为 1.11.1）、Lifecycle 2.8.7（最新稳定为 2.11.0）、Coil 2.7.0、Robolectric 4.14.1（最新 4.17）。

## 3. 依赖解析证据（`debugRuntimeClasspath` 关键行原文）

```
+--- androidx.compose.material3:material3 -> 1.3.2
+--- androidx.room:room-runtime:2.7.0
+--- androidx.room:room-ktx:2.7.0
+--- androidx.media3:media3-exoplayer:1.6.1
+--- androidx.media3:media3-ui:1.6.1
+--- com.squareup.okhttp3:okhttp:4.12.0
+--- org.jetbrains.kotlinx:kotlinx-serialization-json:1.8.1
+--- io.coil-kt:coil-compose:2.7.0
+--- androidx.activity:activity-compose:1.10.1
+--- androidx.core:core-ktx:1.15.0
+--- androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7
```
