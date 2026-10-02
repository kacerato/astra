# 11 — Áudio com miniaudio

Backend: **miniaudio 0.11.25** (domínio público ou MIT-0). Referências de comportamento: Unity 6.0 [Audio Source](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html), Audio Listener, Audio Mixer, Audio Clip; Godot 4.7 `AudioStreamPlayer3D`, `AudioServer` (barramentos e efeitos).

---

## 1. Arquitetura

```
AudioSource / AudioListener / AudioMixer (asset) / AudioReverbZone
        │ AudioSyncSystem (fase AudioSync): posições, velocidades, parâmetros alterados, comandos
        ▼
audio::World (servidor: vozes, grupos, efeitos, parâmetros expostos, tempo DSP)
        │ fila de comandos sem lock → thread de áudio
        ▼
MiniaudioBackend: ma_engine · ma_sound · ma_sound_group · ma_resource_manager · grafo de ma_node
        ▼
Dispositivo: AAudio (preferido) / OpenSL ES no Android · WASAPI no Windows
```

- A thread de áudio (callback do miniaudio) **não aloca nem bloqueia**. Alocação de vozes e decodificação acontecem fora dela; o resource manager do miniaudio decodifica e faz streaming em suas próprias threads.
- Um `audio::World` por `astra::World`; o editor tem um mundo de pré-visualização para ouvir clipes no Inspector.

## 2. AudioClip

| *Load type* | Mapeamento miniaudio | Uso típico |
|---|---|---|
| `DecompressOnLoad` | `MA_SOUND_FLAG_DECODE` (PCM em memória) | Efeitos curtos e frequentes |
| `CompressedInMemory` | Dados comprimidos em memória, decodificação sob demanda | Efeitos médios |
| `Streaming` | `MA_SOUND_FLAG_STREAM` | Música, ambiente longo |

Outros: `preload`, `loadInBackground`, comprimento, canais, taxa. O Inspector mostra a forma de onda.

## 3. AudioSource

| Propriedade | Consumidor miniaudio | Classificação |
|---|---|---|
| `clip`, `output` (grupo do mixer) | `ma_sound` ligado ao `ma_sound_group` | Equivalente |
| `mute`, `volume`, `pitch`, `stereoPan` | `ma_sound_set_volume/pitch/pan` | Equivalente |
| `playOnAwake`, `loop`, `priority` | Agendador de vozes Astra | Equivalente |
| `bypassEffects`, `bypassListenerEffects`, `bypassReverbZones` | Roteamento no grafo de nós | Equivalente |
| `spatialBlend` (0 = 2D, 1 = 3D) | 0 e 1 diretos; valores intermediários por crossfade entre caminho 2D e espacializado | Adaptação explícita (custo medido) |
| `dopplerLevel` | `ma_sound_set_doppler_factor` | Equivalente |
| `spread` | Largura estéreo da fonte | Adaptação |
| `volumeRolloff` (`Logarithmic`, `Linear`, `Custom`) | Modelos de atenuação do miniaudio; `Custom` = curva Astra aplicada como ganho | Equivalente |
| `minDistance`, `maxDistance` | `ma_sound_set_min/max_distance` | Equivalente |
| Cone (ângulos interno/externo, ganho externo) | `ma_sound_set_cone` | Extra |
| `reverbZoneMix` | Envio para o barramento de reverb | Equivalente |

API: `Play`, `PlayDelayed(s)`, `PlayScheduled(dspTime)`, `Stop`, `Pause`, `UnPause`, `PlayOneShot(clip, volume)`, `time`, `timeSamples`, `isPlaying`.

## 4. AudioListener

Um ativo por mundo (aviso se houver mais de um). Posição, orientação e velocidade → `ma_engine_listener_*`. `AudioListener.volume` global e `AudioListener.pause`, como na Unity.

## 5. AudioMixer

| Recurso | Implementação | Fase |
|---|---|---|
| Grupos hierárquicos (Master → Música/Efeitos/Voz…) | `ma_sound_group` encadeados | F9 |
| Volume em dB, pitch, mute, solo | Parâmetros do grupo | F9 |
| Efeitos por grupo: passa-baixa/alta/banda, EQ paramétrico, shelves, notch | Nós de filtro do miniaudio | F9 |
| Eco/delay | Nó de delay do miniaudio | F9 |
| Reverb | Nó próprio (algoritmo Freeverb, domínio público) | F9 |
| Compressor/limitador | Nó próprio | F9 |
| Envios (send/return) | Nós de split/mix | F9 |
| Parâmetros expostos (`mixer.SetFloat("MusicVol", -10)`) | Tabela de parâmetros | F9 |
| Snapshots com transição | Interpolação de parâmetros no tempo | F9 |
| Ducking (sidechain) | Detector de envelope + ganho | Pendente |
| `AudioReverbZone` | Peso por distância no envio de reverb | F9, se o custo couber; senão pendente com motivo |

## 6. Vozes e desempenho

- Limite de vozes reais por tier; excedentes viram **virtuais** (posição atualizada, sem mixagem) e escolhidas por prioridade e audibilidade estimada.
- Taxa de amostragem nativa do aparelho (normalmente 48 kHz) para evitar reamostragem global.
- Profiler: vozes reais/virtuais, tempo do callback de áudio, memória de clipes, *underruns*.

## 7. Android

| Tema | Regra |
|---|---|
| Backend | AAudio em modo de baixa latência, com fallback para OpenSL ES |
| Foco de áudio | Perda permanente → pausa; perda transitória → pausa ou *duck* configurável; recuperação retoma (herança de `ANDROID-AUDIO-FOCUS-WAV-2026-09-30`) |
| Ciclo de vida | `onPause` para o dispositivo; `onResume` recria; troca de rota (fone/Bluetooth) recria o device sem perder o estado das vozes |
| Energia | No editor, o dispositivo de áudio fica parado quando nada toca |
| Latência Bluetooth | Informada no Profiler; nada de compensação automática silenciosa |

## 8. Editor

- Inspector de AudioClip: forma de onda, play/stop/loop, informações de formato.
- Gizmo do AudioSource: esferas de min/max e cone.
- Janela **Mixer**: faixas verticais com medidores (pico/RMS), faders em dB, mute/solo, slots de efeito, snapshots; ao vivo em Play.
- Botão global "mudo do editor" na barra superior.

## 9. Inventário

| Componente/asset | Unity 6.0 | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| AudioSource | ✓ | AudioStreamPlayer/3D | F9 | Equivalente (com adaptações de §3) |
| AudioListener | ✓ | AudioListener3D | F9 | Equivalente |
| AudioMixer | ✓ | Barramentos do AudioServer | F9 | Equivalente (ducking pendente) |
| AudioReverbZone | ✓ | Area3D com reverb | F9 | A confirmar por custo |
| AudioClip | ✓ | AudioStream | F9 | Equivalente |

## 10. Aceite (F9)

- Cena com música em streaming, passos 3D (evento de animação), porta com som posicional e mixer com snapshot "pausa" (música abafada por passa-baixa) acionado por script.
- Ligação/desligamento de fone Bluetooth durante o Play sem crash nem silêncio permanente.
- Chamada telefônica recebida (perda de foco) pausa e retoma.
- 64 vozes simultâneas em T1 sem *underrun* na captura do Profiler.
