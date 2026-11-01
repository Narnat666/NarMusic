package com.narnat.music.ui.components

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Clear
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.R
import com.narnat.music.ui.theme.NarFillField

/** 轻量顶栏：品牌小字 + 右侧圆形搜索按钮（点击后才展开输入），没有整块 AppBar 底色与描边 */
@Composable
fun NarTopBar(
    title: String,
    searchText: String,
    onSearchTextChange: (String) -> Unit,
    onSearchSubmit: () -> Unit,
    searchEnabled: Boolean = true,
    showSearch: Boolean = true,
    onLogoTap: () -> Unit = {}
) {
    val colorScheme = MaterialTheme.colorScheme
    var searchExpanded by rememberSaveable { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .windowInsetsPadding(WindowInsets.statusBars)
            .padding(start = 20.dp, end = 12.dp, top = 8.dp, bottom = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.clickable(onClick = onLogoTap)
        ) {
            Image(
                painter = painterResource(R.drawable.ic_nar_logo),
                contentDescription = title,
                modifier = Modifier.size(22.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = title,
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.8f)
            )
        }
        Spacer(modifier = Modifier.width(12.dp))
        if (showSearch && searchExpanded) {
            NarFillField(
                value = searchText,
                onValueChange = onSearchTextChange,
                placeholder = "模糊搜索歌曲、歌手...",
                singleLine = true,
                enabled = searchEnabled,
                leading = {
                    Icon(
                        imageVector = Icons.Rounded.Search,
                        contentDescription = "搜索",
                        tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                        modifier = Modifier.size(18.dp)
                    )
                },
                trailing = {
                    if (searchText.isNotEmpty()) {
                        Icon(
                            imageVector = Icons.Rounded.Clear,
                            contentDescription = "清除",
                            tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                            modifier = Modifier
                                .size(18.dp)
                                .clickable { onSearchTextChange("") }
                        )
                    } else {
                        Icon(
                            imageVector = Icons.Rounded.Clear,
                            contentDescription = "收起搜索",
                            tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                            modifier = Modifier
                                .size(18.dp)
                                .clickable { searchExpanded = false }
                        )
                    }
                },
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
                keyboardActions = KeyboardActions(onSearch = { onSearchSubmit() }),
                modifier = Modifier.weight(1f)
            )
        } else {
            Spacer(modifier = Modifier.weight(1f))
            if (showSearch) {
                NarCircleIconButton(
                    onClick = { searchExpanded = true },
                    contentDescription = "搜索",
                    icon = Icons.Rounded.Search
                )
            }
        }
    }
}

/** 顶栏用的 40dp 圆形图标按钮（无描边、极浅填充） */
@Composable
fun NarCircleIconButton(
    onClick: () -> Unit,
    contentDescription: String,
    icon: ImageVector
) {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = Modifier
            .size(40.dp)
            .clip(CircleShape)
            .background(colorScheme.onSurface.copy(alpha = 0.055f))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Icon(
            imageVector = icon,
            contentDescription = contentDescription,
            tint = colorScheme.onSurfaceVariant.copy(alpha = 0.85f),
            modifier = Modifier.size(20.dp)
        )
    }
}
