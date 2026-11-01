package com.narnat.music.ui.theme

import android.graphics.BitmapFactory
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

/**
 * 从封面字节数组提取的主色集合。自己实现的极简版取色：
 * 缩放到 32x32 → 求整体平均色（dominant）与饱和度权重最高的色（accent）。
 * 不引入 androidx.palette 依赖。
 */
@Immutable
data class NarPalette(
    val dominant: Color,
    val accent: Color
)

private const val SAMPLE = 32

fun paletteFromCover(bytes: ByteArray?): NarPalette? {
    if (bytes == null || bytes.isEmpty()) return null
    return runCatching {
        val opts = BitmapFactory.Options().apply { inJustDecodeBounds = false; inSampleSize = 4 }
        val src = BitmapFactory.decodeByteArray(bytes, 0, bytes.size, opts) ?: return null
        val small = android.graphics.Bitmap.createScaledBitmap(src, SAMPLE, SAMPLE, false)
        val pixels = IntArray(SAMPLE * SAMPLE)
        small.getPixels(pixels, 0, SAMPLE, 0, 0, SAMPLE, SAMPLE)
        if (!src.isRecycled) src.recycle()
        if (!small.isRecycled && small !== src) small.recycle()

        var sr = 0f
        var sg = 0f
        var sb = 0f
        var count = 0
        var bestScore = -1f
        var bestColor = Color(0xFF2D7BC1)

        val hsv = FloatArray(3)
        for (p in pixels) {
            val a = (p ushr 24) and 0xFF
            if (a < 32) continue
            val r = (p shr 16) and 0xFF
            val g = (p shr 8) and 0xFF
            val b = p and 0xFF
            sr += r
            sg += g
            sb += b
            count++
            android.graphics.Color.RGBToHSV(r, g, b, hsv)
            val score = hsv[1] * hsv[2]
            if (score > bestScore) {
                bestScore = score
                bestColor = Color(r / 255f, g / 255f, b / 255f)
            }
        }
        if (count == 0) return null
        val avg = Color(sr / count / 255f, sg / count / 255f, sb / count / 255f)
        NarPalette(dominant = normalize(avg, 0.46f, 0.72f), accent = normalize(bestColor, 0.40f, 0.66f))
    }.getOrNull()
}

private fun relativeLum(c: Color): Float = 0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue

/** 把颜色亮度压进 [min,max] 区间，避免封面过曝/过暗导致背景泛白或发黑 */
private fun normalize(c: Color, min: Float, max: Float): Color {
    val lum = relativeLum(c)
    val target = lum.coerceIn(min, max)
    if (lum <= 0.001f) return Color(target, target, target)
    val k = target / lum
    return Color(
        (c.red * k).coerceIn(0f, 1f),
        (c.green * k).coerceIn(0f, 1f),
        (c.blue * k).coerceIn(0f, 1f)
    )
}

/** 线性混合：fraction=0 取 from，=1 取 to */
fun mix(from: Color, to: Color, fraction: Float): Color {
    val f = fraction.coerceIn(0f, 1f)
    return Color(
        from.red + (to.red - from.red) * f,
        from.green + (to.green - from.green) * f,
        from.blue + (to.blue - from.blue) * f,
        from.alpha + (to.alpha - from.alpha) * f
    )
}

/** 把主色稀释到指定不透明度后叠在底色上，得到低饱和的"晕染"色 */
fun wash(color: Color, on: Color, alpha: Float): Color = mix(on, color, alpha)

@Composable
fun rememberNarPalette(coverBytes: ByteArray?): NarPalette? =
    remember(coverBytes) { paletteFromCover(coverBytes) }

/** 沉浸式渐变：从封面主色晕染到中性底色，整屏连续、无卡片边界 */
fun immersiveBrush(palette: NarPalette?, dark: Boolean): Brush {
    val base = if (dark) Color(0xFF0F1114) else Color(0xFFF4F5F7)
    val tint = palette?.accent ?: if (dark) Color(0xFF3C6E9B) else Color(0xFF2D7BC1)
    val strong = palette?.dominant ?: tint
    return if (dark) {
        Brush.verticalGradient(
            0f to wash(strong, Color.Black, 0.42f),
            0.34f to wash(tint, base, 0.72f),
            1f to base
        )
    } else {
        Brush.verticalGradient(
            0f to wash(tint, Color.White, 0.09f),
            0.30f to wash(tint, Color.White, 0.035f),
            0.70f to wash(tint, base, 0.012f),
            1f to base
        )
    }
}

/** 播放页用的整屏渐变（比主界面更强，作为全屏播放器的底） */
fun playerBrush(palette: NarPalette?, dark: Boolean): Brush {
    val tint = palette?.dominant ?: if (dark) Color(0xFF22303F) else Color(0xFF2D7BC1)
    return if (dark) {
        Brush.verticalGradient(
            0f to wash(tint, Color.Black, 0.58f),
            0.55f to wash(tint, Color.Black, 0.76f),
            1f to Color(0xFF0C0E11)
        )
    } else {
        Brush.verticalGradient(
            0f to wash(tint, Color.White, 0.16f),
            0.45f to wash(tint, Color.White, 0.24f),
            1f to wash(tint, Color(0xFFF7F7F9), 0.05f)
        )
    }
}

@Composable
fun NarImmersiveBackground(
    palette: NarPalette?,
    dark: Boolean,
    modifier: Modifier = Modifier,
    content: @Composable BoxScope.() -> Unit
) {
    Box(modifier = modifier.fillMaxSize().background(immersiveBrush(palette, dark)), content = content)
}
