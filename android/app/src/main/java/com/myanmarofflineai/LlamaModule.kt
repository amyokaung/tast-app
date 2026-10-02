package com.myanmarofflineai

import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod
import java.util.concurrent.Executors

class LlamaModule(
    private val context: ReactApplicationContext
) : ReactContextBaseJavaModule(context) {

    private val executor = Executors.newSingleThreadExecutor()

    companion object {
        @Volatile
        private var nativeLoaded = false

        private fun ensureNativeLibraryLoaded() {
            if (!nativeLoaded) {
                synchronized(this) {
                    if (!nativeLoaded) {
                        System.loadLibrary("llama_bridge")
                        nativeLoaded = true
                    }
                }
            }
        }
    }

    override fun getName() = "LlamaModule"

    private external fun nativeLoadModel(
        path: String,
        contextSize: Int
    ): Boolean

    private external fun nativeGenerate(
        prompt: String,
        maxTokens: Int,
        temperature: Float,
        callback: NativeCallback
    )

    private external fun nativeStop()
    private external fun nativeClearContext()

    @ReactMethod
    fun loadModel(
        path: String,
        contextSize: Int,
        promise: Promise
    ) {
        executor.execute {
            try {
                ensureNativeLibraryLoaded()

                val ok = nativeLoadModel(
                    path,
                    contextSize
                )

                if (ok) {
                    promise.resolve(true)
                } else {
                    promise.reject(
                        "MODEL_LOAD_FAILED",
                        "Unable to load GGUF model."
                    )
                }

            } catch (e: UnsatisfiedLinkError) {
                promise.reject(
                    "NATIVE_LIBRARY_ERROR",
                    "Native llama library could not be loaded: ${e.message}"
                )

            } catch (e: OutOfMemoryError) {
                promise.reject(
                    "OUT_OF_MEMORY",
                    "Not enough RAM to load this model."
                )

            } catch (e: Exception) {
                promise.reject(
                    "MODEL_ERROR",
                    e.message
                )
            }
        }
    }

    @ReactMethod
    fun generate(
        prompt: String,
        maxTokens: Int,
        temperature: Double,
        promise: Promise
    ) {
        executor.execute {
            try {
                ensureNativeLibraryLoaded()

                nativeGenerate(
                    prompt,
                    maxTokens,
                    temperature.toFloat(),
                    NativeCallback()
                )

                promise.resolve(true)

            } catch (e: UnsatisfiedLinkError) {
                promise.reject(
                    "NATIVE_LIBRARY_ERROR",
                    "Native llama library could not be loaded: ${e.message}"
                )

            } catch (e: OutOfMemoryError) {
                promise.reject(
                    "OUT_OF_MEMORY",
                    "Not enough RAM for generation."
                )

            } catch (e: Exception) {
                promise.reject(
                    "GENERATION_ERROR",
                    e.message
                )
            }
        }
    }

    @ReactMethod
    fun stop() {
        try {
            ensureNativeLibraryLoaded()
            nativeStop()
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    @ReactMethod
    fun clearContext() {
        try {
            ensureNativeLibraryLoaded()
            nativeClearContext()
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    @ReactMethod
    fun addListener(eventName: String) {}

    @ReactMethod
    fun removeListeners(count: Int) {}

    inner class NativeCallback {

        fun onToken(token: String) {
            context
                .getJSModule(
                    com.facebook.react.modules.core.DeviceEventManagerModule
                        .RCTDeviceEventEmitter::class.java
                )
                .emit(
                    "LlamaToken",
                    com.facebook.react.bridge.Arguments.createMap().apply {
                        putString("token", token)
                    }
                )
        }

        fun onComplete() {
            context
                .getJSModule(
                    com.facebook.react.modules.core.DeviceEventManagerModule
                        .RCTDeviceEventEmitter::class.java
                )
                .emit(
                    "LlamaComplete",
                    com.facebook.react.bridge.Arguments.createMap()
                )
        }

        fun onError(message: String) {
            context
                .getJSModule(
                    com.facebook.react.modules.core.DeviceEventManagerModule
                        .RCTDeviceEventEmitter::class.java
                )
                .emit(
                    "LlamaError",
                    com.facebook.react.bridge.Arguments.createMap().apply {
                        putString("error", message)
                    }
                )
        }
    }
}
