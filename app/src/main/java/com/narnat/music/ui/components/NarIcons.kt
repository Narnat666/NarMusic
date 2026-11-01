package com.narnat.music.ui.components

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathParser
import androidx.compose.ui.unit.dp

/**
 * 自绘 Material Symbols（Rounded，Apache-2.0）图标，替代 material-icons-extended。
 * 官方 path data 的 viewBox 为 `0 -960 960 960`，这里用 960x960 视口 + translationY=960 的 group 归位。
 */
object NarIcons {

    val Download: ImageVector by lazy { build("NarDownload", DOWNLOAD) }

    val PlaylistAdd: ImageVector by lazy { build("NarPlaylistAdd", PLAYLIST_ADD) }

    val LibraryMusic: ImageVector by lazy { build("NarLibraryMusic", LIBRARY_MUSIC) }

    private fun build(name: String, pathData: String): ImageVector =
        ImageVector.Builder(
            name = name,
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = VIEWPORT,
            viewportHeight = VIEWPORT,
            autoMirror = false
        ).apply {
            addGroup(translationY = VIEWPORT)
            addPath(
                pathData = PathParser().parsePathString(pathData).toNodes(),
                fill = SolidColor(Color.Black)
            )
            clearGroup()
        }.build()

    private const val VIEWPORT = 960f

    private const val DOWNLOAD =
        "M465-339.5q-7-2.5-13-8.5L308-492q-12-12-11.5-28t11.5-28q12-12 28.5-12.5T365-549l75 " +
            "75v-286q0-17 11.5-28.5T480-800q17 0 28.5 11.5T520-760v286l75-75q12-12 " +
            "28.5-11.5T652-548q11 12 11.5 28T652-492L508-348q-6 6-13 8.5t-15 " +
            "2.5q-8 0-15-2.5ZM240-160q-33 0-56.5-23.5T160-240v-80q0-17 " +
            "11.5-28.5T200-360q17 0 28.5 11.5T240-320v80h480v-80q0-17 " +
            "11.5-28.5T760-360q17 0 28.5 11.5T800-320v80q0 33-23.5 56.5T720-160H240Z"

    private const val PLAYLIST_ADD =
        "M160-320q-17 0-28.5-11.5T120-360q0-17 11.5-28.5T160-400h200q17 0 " +
            "28.5 11.5T400-360q0 17-11.5 28.5T360-320H160Zm0-160q-17 0-28.5-11.5T120-520q0-17 " +
            "11.5-28.5T160-560h360q17 0 28.5 11.5T560-520q0 17-11.5 " +
            "28.5T520-480H160Zm0-160q-17 0-28.5-11.5T120-680q0-17 11.5-28.5T160-720h360q17 0 " +
            "28.5 11.5T560-680q0 17-11.5 28.5T520-640H160Zm491.5 468.5Q640-183 " +
            "640-200v-120H520q-17 0-28.5-11.5T480-360q0-17 11.5-28.5T520-400h120v-120q0-17 " +
            "11.5-28.5T680-560q17 0 28.5 11.5T720-520v120h120q17 0 " +
            "28.5 11.5T880-360q0 17-11.5 28.5T840-320H720v120q0 17-11.5 " +
            "28.5T680-160q-17 0-28.5-11.5Z"

    private const val LIBRARY_MUSIC =
        "M500-360q42 0 71-29t29-71v-220h80q17 0 28.5-11.5T720-720q0-17-11.5-28.5T680-760h-80q-17 " +
            "0-28.5 11.5T560-720v180q-13-10-28-15t-32-5q-42 0-71 29t-29 71q0 42 29 " +
            "71t71 29ZM320-240q-33 0-56.5-23.5T240-320v-480q0-33 " +
            "23.5-56.5T320-880h480q33 0 56.5 23.5T880-800v480q0 33-23.5 " +
            "56.5T800-240H320Zm0-80h480v-480H320v480ZM160-80q-33 0-56.5-23.5T80-160v-520q0-17 " +
            "11.5-28.5T120-720q17 0 28.5 11.5T160-680v520h520q17 0 " +
            "28.5 11.5T720-120q0 17-11.5 28.5T680-80H160Zm160-720v480-480Z"
}
