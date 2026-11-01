package com.narnat.music.ui.components

import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.NavigationRailItemDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

enum class NarTab(val label: String) {
    Download("下载"),
    Batch("批量下载"),
    Library("库"),
    Settings("设置")
}

private fun iconOf(tab: NarTab): ImageVector = when (tab) {
    NarTab.Download -> NarIcons.Download
    NarTab.Batch -> NarIcons.PlaylistAdd
    NarTab.Library -> NarIcons.LibraryMusic
    NarTab.Settings -> Icons.Rounded.Settings
}

/** 竖屏底部导航：无分割线、无选中胶囊底色，仅用主色 + 加粗文字表达选中 */
@Composable
fun NarBottomBar(current: NarTab, onTabSelected: (NarTab) -> Unit) {
    val colorScheme = MaterialTheme.colorScheme
    NavigationBar(
        containerColor = Color.Transparent,
        tonalElevation = 0.dp
    ) {
        NarTab.entries.forEach { tab ->
            NavigationBarItem(
                selected = current == tab,
                onClick = { onTabSelected(tab) },
                icon = { Icon(imageVector = iconOf(tab), contentDescription = tab.label) },
                label = { Text(text = tab.label, fontSize = 11.sp) },
                alwaysShowLabel = true,
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = colorScheme.primary,
                    selectedTextColor = colorScheme.primary,
                    indicatorColor = Color.Transparent,
                    unselectedIconColor = colorScheme.onSurfaceVariant.copy(alpha = 0.55f),
                    unselectedTextColor = colorScheme.onSurfaceVariant.copy(alpha = 0.55f)
                )
            )
        }
    }
}

/** 横屏/大屏：左侧窄导航栏（80dp），选中态同样不使用胶囊指示底 */
@Composable
fun NarNavigationRail(current: NarTab, onTabSelected: (NarTab) -> Unit) {
    val colorScheme = MaterialTheme.colorScheme
    NavigationRail(
        containerColor = Color.Transparent,
        windowInsets = WindowInsets(0, 0, 0, 0),
        modifier = Modifier
            .width(80.dp)
            .windowInsetsPadding(WindowInsets.statusBars)
            .padding(top = 8.dp)
    ) {
        NarTab.entries.forEach { tab ->
            NavigationRailItem(
                selected = current == tab,
                onClick = { onTabSelected(tab) },
                icon = { Icon(imageVector = iconOf(tab), contentDescription = tab.label) },
                label = {
                    Text(
                        text = tab.label,
                        fontSize = 11.sp,
                        fontWeight = if (current == tab) FontWeight.Medium else FontWeight.Normal
                    )
                },
                alwaysShowLabel = true,
                colors = NavigationRailItemDefaults.colors(
                    selectedIconColor = colorScheme.primary,
                    selectedTextColor = colorScheme.primary,
                    indicatorColor = Color.Transparent,
                    unselectedIconColor = colorScheme.onSurfaceVariant.copy(alpha = 0.55f),
                    unselectedTextColor = colorScheme.onSurfaceVariant.copy(alpha = 0.55f)
                )
            )
        }
    }
}
