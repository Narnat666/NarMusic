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
