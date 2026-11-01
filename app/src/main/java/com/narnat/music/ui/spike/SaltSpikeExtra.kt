package com.narnat.music.ui.spike

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import com.moriafly.salt.ui.ItemButton
import com.moriafly.salt.ui.ItemCheck
import com.moriafly.salt.ui.ItemDivider
import com.moriafly.salt.ui.ItemEdit
import com.moriafly.salt.ui.ItemOuterTitle
import com.moriafly.salt.ui.ItemSelect
import com.moriafly.salt.ui.ItemSlider
import com.moriafly.salt.ui.RoundedColumn
import com.moriafly.salt.ui.SaltTheme
import com.moriafly.salt.ui.UnstableSaltUiApi
import com.moriafly.salt.ui.dialog.InputDialog
import com.moriafly.salt.ui.dialog.YesNoDialog
import com.moriafly.salt.ui.popup.PopupMenu
import com.moriafly.salt.ui.popup.PopupMenuItem
import com.moriafly.salt.ui.popup.rememberPopupState

@OptIn(UnstableSaltUiApi::class)
@Composable
fun SaltSpikeExtraComponents() {
    val popup = rememberPopupState()
    var checked by remember { mutableStateOf(false) }
    var editText by remember { mutableStateOf("") }
    var sliderValue by remember { mutableStateOf(0.5f) }
    var confirmVisible by remember { mutableStateOf(false) }
    var inputVisible by remember { mutableStateOf(false) }
    var password by remember { mutableStateOf("") }

    RoundedColumn(modifier = Modifier.fillMaxWidth(), color = SaltTheme.colors.subBackground) {
        ItemOuterTitle("批量操作")
        ItemCheck(state = checked, onChange = { checked = it }, text = "全选")
        ItemSelect(state = popup, text = "排序", value = "全部") {
            PopupMenuItem(onClick = { popup.dismiss() }, text = "A-Z", selected = true)
            PopupMenuItem(onClick = { popup.dismiss() }, text = "最新")
        }
        ItemEdit(text = editText, onChange = { editText = it }, hint = "输入密码")
        ItemSlider(value = sliderValue, onValueChange = { sliderValue = it }, text = "音量")
        ItemButton(onClick = { confirmVisible = true }, text = "批量删除", primary = false)
        ItemDivider()
    }

    PopupMenu(expanded = popup.expend, onDismissRequest = { popup.dismiss() }) {
        PopupMenuItem(onClick = { popup.dismiss() }, text = "A-Z", selected = true)
        PopupMenuItem(onClick = { popup.dismiss() }, text = "最新")
    }

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
