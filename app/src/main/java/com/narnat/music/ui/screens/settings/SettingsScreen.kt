package com.narnat.music.ui.screens.settings

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Check
import androidx.compose.material.icons.rounded.KeyboardArrowRight
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarFillField
import com.narnat.music.ui.theme.NarGroup
import com.narnat.music.ui.theme.NarHairline
import com.narnat.music.ui.theme.NarPagePadding
import com.narnat.music.ui.theme.NarPillButton
import com.narnat.music.ui.theme.NarRow
import com.narnat.music.ui.theme.NarSectionTitle
import com.narnat.music.ui.theme.NarTextAction

enum class LyricsPlatform(val label: String) {
    Netease("网易云音乐"),
    Kugou("酷狗音乐"),
    QQ("QQ音乐"),
    Soda("汽水音乐");

    companion object {
        fun fromLabel(label: String): LyricsPlatform =
            entries.firstOrNull { it.label == label } ?: Kugou
    }
}

enum class DarkModeOption(val label: String) {
    Follow("跟随系统"),
    On("开启"),
    Off("关闭");

    companion object {
        fun fromName(name: String): DarkModeOption =
            entries.firstOrNull { it.name == name } ?: Follow
    }
}

data class SettingsUiState(
    val platform: LyricsPlatform = LyricsPlatform.Kugou,
    val delayText: String = "0",
    val delayError: Boolean = false,
    val darkMode: DarkModeOption = DarkModeOption.Follow,
    val musicCount: Int = 0
)

@Composable
fun SettingsScreen(
    state: SettingsUiState,
    onPlatformChange: (LyricsPlatform) -> Unit,
    onDelayChange: (String) -> Unit,
    onDarkModeChange: (DarkModeOption) -> Unit,
    onSave: () -> Unit,
    onReset: () -> Unit,
    onOpenFolder: () -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(NarPagePadding)
            .padding(bottom = 24.dp)
    ) {
        NarSectionTitle(text = "设置", modifier = Modifier.padding(horizontal = 4.dp, vertical = 8.dp))
        Spacer(modifier = Modifier.height(12.dp))
        NarGroup(title = "下载设置") {
            SettingPickerRow(
                title = "歌词提取平台",
                value = state.platform.label,
                options = LyricsPlatform.entries.map { it.label },
                onPick = { onPlatformChange(LyricsPlatform.entries[it]) }
            )
            NarHairline()
            Column(modifier = Modifier.padding(vertical = 12.dp)) {
                NarCaptionText(text = "延迟 (ms)")
                Spacer(modifier = Modifier.height(10.dp))
                NarFillField(
                    value = state.delayText,
                    onValueChange = onDelayChange,
                    placeholder = "0",
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
                )
                if (state.delayError) {
                    Text(
                        text = "延迟参数必须是整数",
                        fontSize = 13.sp,
                        color = colorScheme.error,
                        modifier = Modifier.padding(top = 8.dp)
                    )
                }
            }
            NarHairline()
            NarRow(
                title = "歌词延迟调整",
                subtitle = "按毫秒整体平移歌词时间轴"
            )
        }
        Spacer(modifier = Modifier.height(16.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            NarPillButton(
                text = "保存设置",
                onClick = onSave,
                modifier = Modifier.weight(1f)
            )
            Spacer(modifier = Modifier.width(8.dp))
            NarTextAction(
                text = "恢复默认",
                onClick = onReset,
                color = colorScheme.onSurfaceVariant
            )
        }
        Spacer(modifier = Modifier.height(24.dp))
        NarGroup(title = "外观") {
            SettingPickerRow(
                title = "深色模式",
                value = state.darkMode.label,
                options = DarkModeOption.entries.map { it.label },
                onPick = { onDarkModeChange(DarkModeOption.entries[it]) }
            )
        }
        Spacer(modifier = Modifier.height(24.dp))
        NarGroup(title = "存储") {
            NarRow(title = "音乐存放位置", subtitle = "Music/NarMusic")
            NarHairline()
            NarRow(
                title = "打开音乐文件夹",
                onClick = onOpenFolder,
                trailing = {
                    Icon(
                        imageVector = Icons.Rounded.KeyboardArrowRight,
                        contentDescription = null,
                        tint = colorScheme.onSurfaceVariant.copy(alpha = 0.5f),
                        modifier = Modifier.height(20.dp)
                    )
                }
            )
            NarHairline()
            NarRow(title = "当前音乐数", subtitle = "${state.musicCount} 首")
        }
        Spacer(modifier = Modifier.height(24.dp))
        NarGroup(title = "关于纳儿音乐") {
            AboutLine(text = "基于B站歌曲链接下载音乐文件")
            AboutLine(text = "-支持封面与歌词封装")
            AboutLine(text = "-支持多平台歌词获取")
            AboutLine(text = "-支持歌词与音频同步")
            AboutLine(text = "-支持通过歌曲名搜索下载")
            AboutLine(text = "-支持精准或模糊批量下载")
        }
        Spacer(modifier = Modifier.height(16.dp))
    }
}

@Composable
private fun SettingPickerRow(
    title: String,
    value: String,
    options: List<String>,
    onPick: (Int) -> Unit
) {
    var expanded by remember { mutableStateOf(false) }
    Box {
        NarRow(
            title = title,
            subtitle = value,
            onClick = { expanded = true },
            trailing = {
                Icon(
                    imageVector = Icons.Rounded.KeyboardArrowRight,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f),
                    modifier = Modifier.height(20.dp)
                )
            }
        )
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEachIndexed { index, label ->
                DropdownMenuItem(
                    text = {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                text = label,
                                fontSize = 15.sp,
                                fontWeight = if (label == value) FontWeight.Medium else FontWeight.Normal
                            )
                            if (label == value) {
                                Spacer(modifier = Modifier.width(8.dp))
                                Icon(
                                    imageVector = Icons.Rounded.Check,
                                    contentDescription = null,
                                    tint = MaterialTheme.colorScheme.primary,
                                    modifier = Modifier.height(18.dp)
                                )
                            }
                        }
                    },
                    onClick = {
                        expanded = false
                        onPick(index)
                    }
                )
            }
        }
    }
}

@Composable
private fun AboutLine(text: String) {
    Text(
        text = text,
        fontSize = 14.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp)
    )
}
