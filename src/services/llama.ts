import {NativeEventEmitter, NativeModules} from 'react-native';

const {LlamaModule} = NativeModules;

const emitter = LlamaModule
  ? new NativeEventEmitter(LlamaModule)
  : null;

export type LlamaSettings = {
  temperature: number;
  maxTokens: number;
};

export async function loadModel(
  path: string,
  contextSize = 4096,
) {
  if (!LlamaModule) {
    throw new Error(
      'Llama AI engine is not available yet.',
    );
  }

  return LlamaModule.loadModel(
    path,
    contextSize,
  );
}

export function generate(
  prompt: string,
  settings: LlamaSettings,
  onToken: (token: string) => void,
  onComplete?: () => void,
  onError?: (error: string) => void,
) {
  if (!LlamaModule || !emitter) {
    const error =
      'Llama AI engine is not available yet.';

    onError?.(error);

    return Promise.reject(
      new Error(error),
    );
  }

  let cleaned = false;

  let tokenSubscription:
    | {remove: () => void}
    | null = null;

  let completeSubscription:
    | {remove: () => void}
    | null = null;

  let errorSubscription:
    | {remove: () => void}
    | null = null;

  const cleanup = () => {
    if (cleaned) {
      return;
    }

    cleaned = true;

    tokenSubscription?.remove();
    completeSubscription?.remove();
    errorSubscription?.remove();

    tokenSubscription = null;
    completeSubscription = null;
    errorSubscription = null;
  };

  tokenSubscription = emitter.addListener(
    'LlamaToken',
    event => {
      onToken(
        String(event?.token ?? ''),
      );
    },
  );

  completeSubscription = emitter.addListener(
    'LlamaComplete',
    () => {
      cleanup();
      onComplete?.();
    },
  );

  errorSubscription = emitter.addListener(
    'LlamaError',
    event => {
      cleanup();

      onError?.(
        String(
          event?.error ??
            'Unknown native error',
        ),
      );
    },
  );

  return LlamaModule.generate(
    prompt,
    settings.maxTokens,
    settings.temperature,
  ).catch((error: unknown) => {
    cleanup();
    throw error;
  });
}

export function stopGeneration() {
  if (!LlamaModule) {
    return;
  }

  try {
    LlamaModule.stop();
  } catch {
    // Ignore when native engine is unavailable.
  }
}

export function clearContext() {
  if (!LlamaModule) {
    return;
  }

  try {
    LlamaModule.clearContext();
  } catch {
    // Ignore when native engine is unavailable.
  }
}
