# Myanmar Offline AI Chat App

React Native + TypeScript + Android Native Module/JNI + llama.cpp + GGUF + SQLite.

## Important

This repository is a **GitHub-ready production starter**. The llama.cpp source is intentionally kept as a Git submodule so the repository does not contain a multi-megabyte/rapidly changing third-party source tree.

After cloning:

```bash
git clone --recurse-submodules <YOUR_REPO_URL>
cd myanmar-offline-ai
npm install
```

If you downloaded this repository as a ZIP from GitHub, initialize the submodule:

```bash
git submodule update --init --recursive
```

Then verify the Android toolchain and run:

```bash
npm run android
```

## Model

Do not commit `.gguf` files. Import a GGUF model from the Android file picker. The app copies the selected file into its private model directory.

## Current native design

React Native UI
→ Kotlin Native Module
→ JNI
→ C++ llama.cpp bridge
→ GGUF

Streaming tokens are emitted back to JavaScript through `NativeEventEmitter`.

## Production notes

- Android target is ARM64 (`arm64-v8a`) by default.
- Storage Access Framework is used instead of broad storage permissions.
- GGUF files are ignored by Git.
- Pin the llama.cpp submodule to a known-good commit before a production release.
- The C++ bridge is intentionally isolated so it can later be migrated to a Turbo Native Module/JSI implementation.
- Test model size against the actual phone RAM. A model file's size is not the same as its runtime RAM requirement.

## Native build

The included CMake file expects:

```text
cpp/llama.cpp
```

to exist as a submodule.

## GitHub Actions

`.github/workflows/build-android.yml` builds a debug APK and uploads it as an artifact.

Release signing is intentionally not committed. Add the required keystore secrets in GitHub Actions before enabling signed releases.

## First production checklist

- [ ] Pin llama.cpp commit
- [ ] Verify model/chat-template compatibility
- [ ] Add runtime model metadata inspection
- [ ] Pass context size from JS to native
- [ ] Add low-memory/device capability checks
- [ ] Add signed release configuration
- [ ] Test Android 10–16 devices
- [ ] Test 4 GB / 6 GB / 8 GB RAM devices
- [ ] Test Burmese Unicode and long streaming responses
