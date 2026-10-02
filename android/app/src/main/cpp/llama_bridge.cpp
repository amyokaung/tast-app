#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "llama.h"

#define TAG "MyanmarOfflineAI"

static llama_model * g_model = nullptr;
static llama_context * g_ctx = nullptr;
static llama_sampler * g_sampler = nullptr;

static std::mutex g_mutex;
static std::atomic<bool> g_stop(false);

static JavaVM * g_vm = nullptr;
static jobject g_callback = nullptr;
static jmethodID g_onToken = nullptr;
static jmethodID g_onComplete = nullptr;
static jmethodID g_onError = nullptr;

static void emit_error(JNIEnv * env, const char * message) {
    if (g_callback && g_onError) {
        env->CallVoidMethod(
            g_callback,
            g_onError,
            env->NewStringUTF(message)
        );
    }
}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM * vm, void *) {
    g_vm = vm;
    llama_backend_init();
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeLoadModel(
    JNIEnv * env,
    jobject,
    jstring path,
    jint context_size
) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const char * model_path = env->GetStringUTFChars(path, nullptr);

    if (g_ctx) {
        llama_free(g_ctx);
        g_ctx = nullptr;
    }

    if (g_model) {
        llama_model_free(g_model);
        g_model = nullptr;
    }

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;

    g_model = llama_model_load_from_file(model_path, model_params);

    env->ReleaseStringUTFChars(path, model_path);

    if (!g_model) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Model load failed");
        return JNI_FALSE;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = static_cast<uint32_t>(context_size);
    ctx_params.n_batch = 512;
    ctx_params.n_threads = 4;
    ctx_params.n_threads_batch = 4;

    g_ctx = llama_init_from_model(g_model, ctx_params);

    if (!g_ctx) {
        llama_model_free(g_model);
        g_model = nullptr;
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeClearContext(
    JNIEnv *,
    jobject
) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_ctx) {
        llama_memory_clear(llama_get_memory(g_ctx), true);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeStop(
    JNIEnv *,
    jobject
) {
    g_stop.store(true);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_myanmarofflineai_LlamaModule_nativeGenerate(
    JNIEnv * env,
    jobject,
    jstring prompt,
    jint max_tokens,
    jfloat temperature,
    jobject callback
) {
    if (!g_model || !g_ctx) {
        emit_error(env, "No model is loaded.");
        return;
    }

    g_stop.store(false);

    if (g_callback) {
        env->DeleteGlobalRef(g_callback);
        g_callback = nullptr;
    }

    g_callback = env->NewGlobalRef(callback);

    jclass callback_class = env->GetObjectClass(callback);

    g_onToken = env->GetMethodID(
        callback_class, "onToken", "(Ljava/lang/String;)V"
    );
    g_onComplete = env->GetMethodID(
        callback_class, "onComplete", "()V"
    );
    g_onError = env->GetMethodID(
        callback_class, "onError", "(Ljava/lang/String;)V"
    );

    const char * prompt_text = env->GetStringUTFChars(prompt, nullptr);
    const llama_vocab * vocab = llama_model_get_vocab(g_model);

    int n_tokens = -llama_tokenize(
        vocab,
        prompt_text,
        std::strlen(prompt_text),
        nullptr,
        0,
        true,
        true
    );

    if (n_tokens <= 0) {
        env->ReleaseStringUTFChars(prompt, prompt_text);
        emit_error(env, "Tokenization failed.");
        return;
    }

    std::vector<llama_token> tokens(n_tokens);

    if (llama_tokenize(
        vocab,
        prompt_text,
        std::strlen(prompt_text),
        tokens.data(),
        static_cast<int32_t>(tokens.size()),
        true,
        true
    ) < 0) {
        env->ReleaseStringUTFChars(prompt, prompt_text);
        emit_error(env, "Tokenization failed.");
        return;
    }

    env->ReleaseStringUTFChars(prompt, prompt_text);

    llama_sampler_chain_params sampler_params =
        llama_sampler_chain_default_params();

    g_sampler = llama_sampler_chain_init(sampler_params);

    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_top_k(40)
    );
    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_top_p(0.95f, 1)
    );
    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_temp(temperature)
    );
    llama_sampler_chain_add(
        g_sampler,
        llama_sampler_init_dist(LLAMA_DEFAULT_SEED)
    );

    llama_batch batch = llama_batch_init(
        static_cast<int32_t>(tokens.size()),
        0,
        1
    );

    for (size_t i = 0; i < tokens.size(); ++i) {
        batch.token[i] = tokens[i];
        batch.pos[i] = static_cast<int32_t>(i);
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == tokens.size() - 1);
    }

    batch.n_tokens = static_cast<int32_t>(tokens.size());

    if (llama_decode(g_ctx, batch) != 0) {
        llama_batch_free(batch);
        llama_sampler_free(g_sampler);
        g_sampler = nullptr;
        emit_error(env, "llama_decode failed.");
        return;
    }

    int32_t pos = static_cast<int32_t>(tokens.size());

    for (int i = 0; i < max_tokens && !g_stop.load(); ++i) {
        llama_token token = llama_sampler_sample(g_sampler, g_ctx, -1);

        if (llama_vocab_is_eog(vocab, token)) {
            break;
        }

        llama_sampler_accept(g_sampler, token);

        int piece_size = -llama_token_to_piece(
            vocab,
            token,
            nullptr,
            0,
            0,
            true
        );

        if (piece_size > 0) {
            std::vector<char> buffer(piece_size + 1);

            int written = llama_token_to_piece(
                vocab,
                token,
                buffer.data(),
                static_cast<int32_t>(buffer.size()),
                0,
                true
            );

            if (written > 0) {
                jstring piece = env->NewStringUTF(buffer.data());
                env->CallVoidMethod(g_callback, g_onToken, piece);
                env->DeleteLocalRef(piece);
            }
        }

        llama_batch next = llama_batch_init(1, 0, 1);
        next.token[0] = token;
        next.pos[0] = pos++;
        next.n_seq_id[0] = 1;
        next.seq_id[0][0] = 0;
        next.logits[0] = true;
        next.n_tokens = 1;

        if (llama_decode(g_ctx, next) != 0) {
            llama_batch_free(next);
            break;
        }

        llama_batch_free(next);
    }

    llama_batch_free(batch);

    if (g_sampler) {
        llama_sampler_free(g_sampler);
        g_sampler = nullptr;
    }

    if (g_callback && g_onComplete) {
        env->CallVoidMethod(g_callback, g_onComplete);
    }
}
