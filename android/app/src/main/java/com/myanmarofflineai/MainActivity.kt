package com.myanmarofflineai

import android.app.Activity
import android.os.Bundle
import android.graphics.Color
import android.widget.TextView

class MainActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val text = TextView(this).apply {
            text = "ANDROID TEST OK"
            textSize = 24f
            setTextColor(Color.BLACK)
            setBackgroundColor(Color.WHITE)
            gravity = android.view.Gravity.CENTER
        }

        setContentView(text)
    }
}
