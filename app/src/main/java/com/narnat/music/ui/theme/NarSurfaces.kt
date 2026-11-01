package com.narnat.music.ui.theme

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

val NarBlockShape = RoundedCornerShape(24.dp)
val NarFieldShape = RoundedCornerShape(16.dp)
val NarThumbShape = RoundedCornerShape(12.dp)

/** 极细分隔线：1 物理像素，只做"隐约的边界"，不构成边框卡片的观感 */
@Composable
fun NarHairline(modifier: Modifier = Modifier, insetStart: Dp = 0.dp, color: Color? = null) {
    val line = color ?: MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.10f)
    Row(modifier = modifier.fillMaxWidth()) {
        if (insetStart > 0.dp) Spacer(modifier = Modifier.width(insetStart))
        Box(
            modifier = Modifier
                .weight(1f)
                .height(0.6.dp)
                .background(line)
        )
    }
}

/** 大留白 + 大圆角的整块 surface，无描边；承载分组内容 */
@Composable
fun NarSurface(
    modifier: Modifier = Modifier,
    shape: Shape = NarBlockShape,
    color: Color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
    contentPadding: PaddingValues = PaddingValues(20.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    Surface(shape = shape, color = color, modifier = modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(contentPadding), content = content)
    }
}

/** 分组：外层小标题 + 大圆角块，块内行之间用 hairline 分隔 */
@Composable
fun NarGroup(
    modifier: Modifier = Modifier,
    title: String? = null,
    contentPadding: PaddingValues = PaddingValues(horizontal = 20.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    Column(modifier = modifier.fillMaxWidth()) {
        if (title != null) {
            Text(
                text = title,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                modifier = Modifier.padding(start = 20.dp, end = 20.dp, bottom = 8.dp)
            )
        }
        NarSurface(contentPadding = contentPadding, content = content)
    }
}

/** 列表/设置行：行高 72dp、主副两行文本、行尾可选操作 */
@Composable
fun NarRow(
    title: String,
    modifier: Modifier = Modifier,
    subtitle: String? = null,
    leading: (@Composable () -> Unit)? = null,
    trailing: (@Composable RowScope.() -> Unit)? = null,
    onClick: (() -> Unit)? = null,
    titleColor: Color = MaterialTheme.colorScheme.onSurface,
    horizontalPadding: Dp = 0.dp
) {
    val base = if (onClick != null) {
        modifier.fillMaxWidth().clickable(onClick = onClick)
    } else {
        modifier.fillMaxWidth()
    }
    Row(
        modifier = base
            .heightIn(min = 72.dp)
            .padding(horizontal = horizontalPadding, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (leading != null) {
            leading()
            Spacer(modifier = Modifier.width(16.dp))
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleSmall.copy(fontWeight = FontWeight.Medium),
                fontSize = 17.sp,
                color = titleColor,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
            if (subtitle != null) {
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
        if (trailing != null) {
            Spacer(modifier = Modifier.width(12.dp))
            trailing()
        }
    }
}

/** 页面大标题：28sp SemiBold */
@Composable
fun NarSectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        fontSize = 28.sp,
        lineHeight = 36.sp,
        fontWeight = FontWeight.SemiBold,
        color = MaterialTheme.colorScheme.onBackground,
        modifier = modifier
    )
}

/** 唯一视觉重心的主操作：胶囊形实心按钮 */
@Composable
fun NarPillButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true
) {
    Button(
        onClick = onClick,
        enabled = enabled,
        shape = CircleShape,
        contentPadding = PaddingValues(horizontal = 28.dp, vertical = 14.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.primary,
            contentColor = MaterialTheme.colorScheme.onPrimary
        ),
        modifier = modifier.heightIn(min = 52.dp)
    ) {
        Text(text = text, fontSize = 16.sp, fontWeight = FontWeight.Medium)
    }
}

/** 次操作：纯文字按钮，不做第二个胶囊 */
@Composable
fun NarTextAction(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    color: Color = MaterialTheme.colorScheme.primary
) {
    TextButton(onClick = onClick, enabled = enabled, modifier = modifier) {
        Text(
            text = text,
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium,
            color = if (enabled) color else color.copy(alpha = 0.4f)
        )
    }
}

/** 无边框填充式输入：圆角 16dp 的 fill 容器，无描边 */
@Composable
fun NarFillField(
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    placeholder: String = "",
    minLines: Int = 1,
    singleLine: Boolean = false,
    enabled: Boolean = true,
    leading: (@Composable () -> Unit)? = null,
    trailing: (@Composable () -> Unit)? = null,
    keyboardOptions: KeyboardOptions = KeyboardOptions.Default,
    keyboardActions: KeyboardActions = KeyboardActions.Default,
    textStyle: TextStyle = LocalTextStyle.current
) {
    val colorScheme = MaterialTheme.colorScheme
    val fill = colorScheme.onSurface.copy(alpha = if (colorScheme.surface.luminance() > 0.5f) 0.075f else 0.11f)
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clip(NarFieldShape)
            .background(fill)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = if (singleLine) Alignment.CenterVertically else Alignment.Top
    ) {
        if (leading != null) {
            leading()
            Spacer(modifier = Modifier.width(12.dp))
        }
        Box(modifier = Modifier.weight(1f)) {
            if (value.isEmpty() && placeholder.isNotEmpty()) {
                Text(
                    text = placeholder,
                    fontSize = 15.sp,
                    color = colorScheme.onSurfaceVariant.copy(alpha = 0.6f)
                )
            }
            BasicTextField(
                value = value,
                onValueChange = onValueChange,
                enabled = enabled,
                singleLine = singleLine,
                minLines = minLines,
                textStyle = textStyle.copy(fontSize = 15.sp, color = colorScheme.onSurface),
                cursorBrush = SolidColor(colorScheme.primary),
                keyboardOptions = keyboardOptions,
                keyboardActions = keyboardActions,
                modifier = Modifier.fillMaxWidth()
            )
        }
        if (trailing != null) {
            Spacer(modifier = Modifier.width(8.dp))
            trailing()
        }
    }
}

/** 行首方形封面缩略图（48dp / 圆角 12dp），无封面时铺主色浅底 */
@Composable
fun NarCoverThumb(
    modifier: Modifier = Modifier,
    size: Dp = 48.dp,
    label: String? = null,
    content: (@Composable () -> Unit)? = null
) {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = modifier
            .size(size)
            .clip(NarThumbShape)
            .background(colorScheme.primary.copy(alpha = 0.16f)),
        contentAlignment = Alignment.Center
    ) {
        if (content != null) {
            content()
        } else if (label != null) {
            Text(
                text = label,
                fontSize = 13.sp,
                fontWeight = FontWeight.Medium,
                color = colorScheme.primary
            )
        }
    }
}

private fun Color.luminance(): Float = 0.2126f * red + 0.7152f * green + 0.0722f * blue

/** 等宽数字，避免进度/时间数字跳动 */
val NarTabularNumber = TextStyle(
    fontFeatureSettings = "tnum",
    fontWeight = FontWeight.Medium
)

@Composable
fun NarValueText(text: String, modifier: Modifier = Modifier, size: Int = 20) {
    Text(
        text = text,
        fontSize = size.sp,
        fontWeight = FontWeight.SemiBold,
        style = NarTabularNumber.copy(fontSize = size.sp),
        color = MaterialTheme.colorScheme.onSurface,
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        modifier = modifier
    )
}

@Composable
fun NarCaptionText(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        fontSize = 13.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
        modifier = modifier
    )
}

/** 页面的统一内外边距：横屏内容限宽后靠它保持呼吸感 */
val NarPagePadding = PaddingValues(horizontal = 20.dp, vertical = 8.dp)

@Composable
fun NarSpacer(height: Dp) {
    Spacer(modifier = Modifier.height(height))
}

@Composable
fun NarRowGap(width: Dp) {
    Spacer(modifier = Modifier.width(width))
}

@Composable
fun NarFillRow(
    modifier: Modifier = Modifier,
    horizontalArrangement: Arrangement.Horizontal = Arrangement.Start,
    verticalAlignment: Alignment.Vertical = Alignment.CenterVertically,
    content: @Composable RowScope.() -> Unit
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = horizontalArrangement,
        verticalAlignment = verticalAlignment,
        content = content
    )
}
