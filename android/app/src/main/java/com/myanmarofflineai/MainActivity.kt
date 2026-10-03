package com.myanmarofflineai

import android.app.Activity
import android.os.Bundle
import android.graphics.Color
import android.widget.TextView
import com.facebook.react.ReactActivity

class MainActivity : ReactActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
        } catch (e: Throwable) {
            val errorText = TextView(this).apply {
                text = "REACT NATIVE CRASH\n\n${e.stackTraceToString()}"
                textSize = 14f
                setTextColor(Color.BLACK)
                setBackgroundColor(Color.WHITE)
                setPadding(30, 30, 30, 30)
            }

            setContentView(errorText)
        }
    }

    override fun getMainComponentName(): String {
        return "MyanmarOfflineAI"
    }
}
