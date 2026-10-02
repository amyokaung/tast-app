import React, {useState} from 'react';
import {Alert, Button, FlatList, Text, TextInput, View} from 'react-native';
import Markdown from 'react-native-markdown-display';
import {generate, stopGeneration} from '../services/llama';
import {useSettingsStore} from '../store/settingsStore';

type Message = {role: 'user' | 'assistant'; content: string};

export default function ChatScreen() {
  const [input, setInput] = useState('');
  const [messages, setMessages] = useState<Message[]>([]);
  const [generating, setGenerating] = useState(false);
  const {temperature, maxTokens} = useSettingsStore();

  function buildPrompt(userMessage: string) {
    const history = messages
      .slice(-10)
      .map(m => `${m.role}: ${m.content}`)
      .join('\n');

    return `You are a helpful Myanmar language AI assistant.
Answer naturally in Myanmar Unicode.

Conversation:
${history}

user: ${userMessage}
assistant:`;
  }

  async function send() {
    const text = input.trim();
    if (!text || generating) return;

    setInput('');
    setMessages(prev => [
      ...prev,
      {role: 'user', content: text},
      {role: 'assistant', content: ''},
    ]);
    setGenerating(true);

    try {
      await generate(
        buildPrompt(text),
        {temperature, maxTokens},
        token => {
          setMessages(prev => {
            const copy = [...prev];
            const last = copy.length - 1;
            copy[last] = {
              ...copy[last],
              content: copy[last].content + token,
            };
            return copy;
          });
        },
        () => setGenerating(false),
        error => {
          setGenerating(false);
          Alert.alert('AI Error', error);
        },
      );
    } catch (e) {
      setGenerating(false);
      Alert.alert('Generation failed', String(e));
    }
  }

  return (
    <View style={{flex: 1, padding: 12}}>
      <FlatList
        data={messages}
        keyExtractor={(_, i) => String(i)}
        renderItem={({item}) => (
          <View
            style={{
              padding: 12,
              marginVertical: 4,
              borderRadius: 12,
              backgroundColor: item.role === 'user' ? '#DCF8C6' : '#EEEEEE',
            }}>
            {item.role === 'assistant' ? (
              <Markdown
                style={{
                  body: {fontSize: 16, lineHeight: 28},
                }}>
                {item.content}
              </Markdown>
            ) : (
              <Text style={{fontSize: 16, lineHeight: 28}}>
                {item.content}
              </Text>
            )}
          </View>
        )}
      />

      {generating && (
        <Button
          title="Stop"
          onPress={() => {
            stopGeneration();
            setGenerating(false);
          }}
        />
      )}

      <TextInput
        value={input}
        onChangeText={setInput}
        placeholder="မေးချင်တာ ရိုက်ပါ..."
        multiline
        style={{
          minHeight: 50,
          maxHeight: 140,
          borderWidth: 1,
          borderRadius: 12,
          padding: 12,
          fontSize: 16,
          textAlignVertical: 'top',
        }}
      />
      <Button title="Send" onPress={send} disabled={generating} />
    </View>
  );
}
