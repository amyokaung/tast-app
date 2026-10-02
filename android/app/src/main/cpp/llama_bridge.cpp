#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "llama.h"

#define TAG "MyanmarOfflineAI"

static llama_model *g_model = nullptr;
static llama_context *g_ctx = nullptr;
static llama_sampler *g_sampler = nullptr;

static std::mutex g_mutex;
static std::atomic<bool> g_stop(false);

static jobject g_callback = nullptr;
static jmethodID g_onToken = nullptr;
static jmethodID g_onComplete = nullptr;
static jmethodID g_onError = nullptr;

static void clear_callback(JNIEnv *env) {
    if (g_callback != nullptr) {
        env->DeleteGlobalRef(g_callback);
        g_callback = nullptr;
    }

    g_onToken = nullptr;
    g_onComplete = nullptr;
    g_onError = nullptr;
}

static void emit_error(JNIEnv *env, const char *message) {
    if (g_callback == nullptr || g_onError == nullptr) {
        return;
    }

    jstring error_message = env->NewStringUTF(message);

    if (error_message != nullptr) {
        env->CallVoidMethod(
            g_callback,
            g_onError,
            error_message
        );

        env->DeleteLocalRef(error_message);
    }
}

extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *) {
    llama_backend_init();

    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeLoadModel(
    JNIEnv *env,
    jobject,
    jstring path,
    jint context_size
) {
    if (path == nullptr) {
        return JNI_FALSE;
    }

    if (context_size <= 0) {
        return JNI_FALSE;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    const char *model_path =
        env->GetStringUTFChars(path, nullptr);

    if (model_path == nullptr) {
        return JNI_FALSE;
    }

    /*
     * Free previous context.
     */
    if (g_ctx != nullptr) {
        llama_free(g_ctx);
        g_ctx = nullptr;
    }

    /*
     * Free previous model.
     */
    if (g_model != nullptr) {
        llama_model_free(g_model);
        g_model = nullptr;
    }

    /*
     * Free previous sampler.
     */
    if (g_sampler != nullptr) {
        llama_sampler_free(g_sampler);
        g_sampler = nullptr;
    }

    llama_model_params model_params =
        llama_model_default_params();

    /*
     * CPU-only inference for Android.
     */
    model_params.n_gpu_layers = 0;

    g_model =
        llama_model_load_from_file(
            model_path,
            model_params
        );

    env->ReleaseStringUTFChars(
        path,
        model_path
    );

    if (g_model == nullptr) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            TAG,
            "Failed to load GGUF model"
        );

        return JNI_FALSE;
    }

    llama_context_params ctx_params =
        llama_context_default_params();

    ctx_params.n_ctx =
        static_cast<uint32_t>(context_size);

    /*
     * Keep batch size reasonable for Android RAM.
     */
    ctx_params.n_batch = 512;

    /*
     * CPU threads.
     *
     * 4 is a safe starting point.
     * Later we can make this configurable.
     */
    ctx_params.n_threads = 4;
    ctx_params.n_threads_batch = 4;

    g_ctx =
        llama_init_from_model(
            g_model,
            ctx_params
        );

    if (g_ctx == nullptr) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            TAG,
            "Failed to create llama context"
        );

        llama_model_free(g_model);
        g_model = nullptr;

        return JNI_FALSE;
    }

    g_stop.store(false);

    __android_log_print(
        ANDROID_LOG_INFO,
        TAG,
        "GGUF model loaded successfully"
    );

    return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeClearContext(
    JNIEnv *,
    jobject
) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_ctx != nullptr) {
        llama_memory_clear(
            llama_get_memory(g_ctx),
            true
        );
    }

    g_stop.store(false);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeStop(
    JNIEnv *,
    jobject
) {
    g_stop.store(true);

    __android_log_print(
        ANDROID_LOG_INFO,
        TAG,
        "Generation stop requested"
    );
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeGenerate(
    JNIEnv *env,
    jobject,
    jstring prompt,
    jint max_tokens,
    jfloat temperature,
    jobject callback
) {
    if (prompt == nullptr || callback == nullptr) {
        return;
    }

    if (max_tokens <= 0) {
        return;
    }

    /*
     * Model/context must already exist.
     */
    if (g_model == nullptr || g_ctx == nullptr) {
        emit_error(
            env,
            "No model is loaded."
        );

        return;
    }

    g_stop.store(false);

    /*
     * Replace previous callback.
     */
    if (g_callback != nullptr) {
        clear_callback(env);
    }

    g_callback =
        env->NewGlobalRef(callback);

    if (g_callback == nullptr) {
        return;
    }

    jclass callback_class =
        env->GetObjectClass(callback);

    if (callback_class == nullptr) {
        clear_callback(env);
        return;
    }

    g_onToken =
        env->GetMethodID(
            callback_class,
            "onToken",
            "(Ljava/lang/String;)V"
        );

    g_onComplete =
        env->GetMethodID(
            callback_class,
            "onComplete",
            "()V"
        );

    g_onError =
        env->GetMethodID(
            callback_class,
            "onError",
            "(Ljava/lang/String;)V"
        );

    env->DeleteLocalRef(callback_class);

    if (g_onToken == nullptr ||
        g_onComplete == nullptr ||
        g_onError == nullptr) {

        clear_callback(env);

        return;
    }

    const char *prompt_text =
        env->GetStringUTFChars(
            prompt,
            nullptr
        );

    if (prompt_text == nullptr) {
        emit_error(
            env,
            "Unable to read prompt."
        );

        clear_callback(env);

        return;
    }

    const llama_vocab *vocab =
        llama_model_get_vocab(g_model);

    if (vocab == nullptr) {
        env->ReleaseStringUTFChars(
            prompt,
            prompt_text
        );

        emit_error(
            env,
            "Unable to access model vocabulary."
        );

        clear_callback(env);

        return;
    }

    const int32_t prompt_length =
        static_cast<int32_t>(
            std::strlen(prompt_text)
        );

    /*
     * First call determines required token count.
     */
    int32_t n_tokens =
        llama_tokenize(
            vocab,
            prompt_text,
            prompt_length,
            nullptr,
            0,
            true,
            true
        );

    if (n_tokens >= 0) {
        /*
         * A successful call with zero tokens is invalid
         * for our generation path.
         */
        if (n_tokens == 0) {
            env->ReleaseStringUTFChars(
                prompt,
                prompt_text
            );

            emit_error(
                env,
                "Prompt produced zero tokens."
            );

            clear_callback(env);

            return;
        }
    } else {
        /*
         * Negative return value means the required size.
         */
        n_tokens = -n_tokens;
    }

    if (n_tokens <= 0) {
        env->ReleaseStringUTFChars(
            prompt,
            prompt_text
        );

        emit_error(
            env,
            "Tokenization failed."
        );

        clear_callback(env);

        return;
    }

    std::vector<llama_token> tokens(
        static_cast<size_t>(n_tokens)
    );

    const int32_t actual_tokens =
        llama_tokenize(
            vocab,
            prompt_text,
            prompt_length,
            tokens.data(),
            n_tokens,
            true,
            true
        );

    env->ReleaseStringUTFChars(
        prompt,
        prompt_text
    );

    if (actual_tokens < 0) {
        emit_error(
            env,
            "Tokenization failed."
        );

        clear_callback(env);

        return;
    }

    tokens.resize(
        static_cast<size_t>(actual_tokens)
    );

    if (tokens.empty()) {
        emit_error(
            env,
            "Prompt produced no tokens."
        );

        clear_callback(env);

        return;
    }

    /*
     * Create sampler chain.
     */
    llama_sampler_chain_params sampler_params =
        llama_sampler_chain_default_params();

    g_sampler =
        llama_sampler_chain_init(
            sampler_params
        );

    if (g_sampler == nullptr) {
        emit_error(
            env,
            "Failed to create sampler."
        );

        clear_callback(env);

        return;
    }

    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_top_k(40)
    );

    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_top_p(
            0.95f,
            1
        )
    );

    /*
     * Protect against invalid temperature.
     */
    float safe_temperature = temperature;

    if (safe_temperature <= 0.0f) {
        safe_temperature = 0.7f;
    }

    if (safe_temperature > 2.0f) {
        safe_temperature = 2.0f;
    }

    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_temp(
            safe_temperature
        )
    );

    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_dist(
            LLAMA_DEFAULT_SEED
        )
    );

    /*
     * Create prompt batch.
     */
    llama_batch batch =
        llama_batch_init(
            static_cast<int32_t>(
                tokens.size()
            ),
            0,
            1
        );

    for (size_t i = 0; i < tokens.size(); ++i) {
        batch.token[i] = tokens[i];

        batch.pos[i] =
            static_cast<llama_pos>(i);

        batch.n_seq_id[i] = 1;

        batch.seq_id[i][0] = 0;

        batch.logits[i] =
            (i == tokens.size() - 1);
    }

    batch.n_tokens =
        static_cast<int32_t>(
            tokens.size()
        );

    /*
     * Evaluate prompt.
     */
    const int decode_result =
        llama_decode(
            g_ctx,
            batch
        );

    if (decode_result != 0) {
        llama_batch_free(batch);

        llama_sampler_free(g_sampler);
        g_sampler = nullptr;

        emit_error(
            env,
            "llama_decode failed."
        );

        clear_callback(env);

        return;
    }

    /*
     * The next generated token starts after
     * the prompt tokens.
     */
    llama_pos pos =
        static_cast<llama_pos>(
            tokens.size()
        );

    /*
     * Generation loop.
     */
    for (
        int i = 0;
        i < max_tokens;
        ++i
    ) {
        if (g_stop.load()) {
            break;
        }

        llama_token token =
            llama_sampler_sample(
                g_sampler,
                g_ctx,
                -1
            );

        /*
         * End of generation.
         */
        if (
            token == LLAMA_TOKEN_NULL ||
            llama_vocab_is_eog(
                vocab,
                token
            )
        ) {
            break;
        }

        llama_sampler_accept(
            g_sampler,
            token
        );

        /*
         * Convert token into UTF-8 text.
         */
        int32_t piece_size =
            llama_token_to_piece(
                vocab,
                token,
                nullptr,
                0,
                0,
                true
            );

        if (piece_size < 0) {
            piece_size = -piece_size;
        }

        if (piece_size > 0) {
            std::vector<char> buffer(
                static_cast<size_t>(
                    piece_size + 1
                )
            );

            const int32_t written =
                llama_token_to_piece(
                    vocab,
                    token,
                    buffer.data(),
                    piece_size,
                    0,
                    true
                );

            if (written > 0) {
                buffer[
                    static_cast<size_t>(written)
                ] = '\0';

                jstring piece =
                    env->NewStringUTF(
                        buffer.data()
                    );

                if (piece != nullptr) {
                    env->CallVoidMethod(
                        g_callback,
                        g_onToken,
                        piece
                    );

                    env->DeleteLocalRef(piece);
                }
            }
        }

        /*
         * Stop can be requested while the
         * token callback is being delivered.
         */
        if (g_stop.load()) {
            break;
        }

        /*
         * Feed generated token back into
         * the model.
         */
        llama_batch next =
            llama_batch_init(
                1,
                0,
                1
            );

        next.token[0] = token;
        next.pos[0] = pos++;
        next.n_seq_id[0] = 1;
        next.seq_id[0][0] = 0;
        next.logits[0] = true;
        next.n_tokens = 1;

        const int next_result =
            llama_decode(
                g_ctx,
                next
            );

        llama_batch_free(next);

        if (next_result != 0) {
            emit_error(
                env,
                "llama_decode failed during generation."
            );

            break;
        }
    }

    /*
     * Free prompt batch.
     */
    llama_batch_free(batch);

    /*
     * Free sampler.
     */
    if (g_sampler != nullptr) {
        llama_sampler_free(g_sampler);
        g_sampler = nullptr;
    }

    /*
     * Tell JS that generation is finished.
     */
    if (
        g_callback != nullptr &&
        g_onComplete != nullptr
    ) {
        env->CallVoidMethod(
            g_callback,
            g_onComplete
        );
    }

    /*
     * Release Java callback reference.
     */
    clear_callback(env);
}
