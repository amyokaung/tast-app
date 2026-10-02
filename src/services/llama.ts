import {NativeEventEmitter, NativeModules} from 'react-native';

const {LlamaModule} = NativeModules;

if (!LlamaModule) {
  throw new Error(
    'LlamaModule is unavailable. Check the Android native build and package registration.',
  );
}

const emitter = new NativeEventEmitter(LlamaModule);

export type LlamaSettings = {
  temperature: number;
  maxTokens: number;
};

export async function loadModel(path: string, contextSize = 4096) {
  return LlamaModule.loadModel(path, contextSize);
}

export function generate(
  prompt: string,
  settings: LlamaSettings,
  onToken: (token: string) => void,
  onComplete?: () => void,
  onError?: (error: string) => void,
) {
  const tokenSubscription = emitter.addListener('LlamaToken', event => {
    onToken(String(event.token ?? ''));
  });

  const completeSubscription = emitter.addListener('LlamaComplete', () => {
    cleanup();
    onComplete?.();
  });

  const errorSubscription = emitter.addListener('LlamaError', event => {
    cleanup();
    onError?.(String(event.error ?? 'Unknown native error'));
  });

  const cleanup = () => {
    tokenSubscription.remove();
    completeSubscription.remove();
    errorSubscription.remove();
  };

  return LlamaModule.generate(
    prompt,
    settings.maxTokens,
    settings.temperature,
  );
}

export function stopGeneration() {
  LlamaModule.stop();
}

export function clearContext() {
  LlamaModule.clearContext();
}
