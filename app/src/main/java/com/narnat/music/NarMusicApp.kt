package com.narnat.music

import android.app.Application
import android.content.Context
import com.narnat.music.di.AppContainer

class NarMusicApp : Application() {

    private var containerRef: AppContainer? = null

    val container: AppContainer
        get() = containerRef ?: error("AppContainer 尚未初始化")

    override fun onCreate() {
        super.onCreate()
        containerRef = AppContainer(this)
    }
}

/** 从任意 Context 取到应用级依赖容器。 */
val Context.appContainer: AppContainer
    get() = (applicationContext as NarMusicApp).container
