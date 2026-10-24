# BLACK VR — variante Unity (OpenXR)

Terceira abordagem do projeto: a mesma ideia de treino de tiro, mas em **Unity 6 / 2022 LTS com OpenXR**,
que é o caminho natural para levar o conceito ao PC de verdade (build standalone, link com o port do BLACK
ou com um emulador de headset).

> Esta pasta **não** contém um projeto Unity completo (não há `.meta`, cenas nem `ProjectSettings`).
> Ela entrega os **scripts de gameplay** prontos para colar em um projeto OpenXR novo.
> Motivo: cenas `.unity` são binárias/YAML gigantes e não fazem sentido versionadas sem o projeto inteiro.

## O que tem aqui

| Arquivo | Papel |
|---|---|
| `Assets/Scripts/VRPlayerRig.cs` | Rig OpenXR: locomoção contínua, snap turn, gravidade, colisão e fallback desktop (WASD/mouse) |
| `Assets/Scripts/XRWeapon.cs` | Arma hitscan: munição, recarga, recuo, traçante, impacto, vibração háptica |
| `Assets/Scripts/Target.cs` | Alvo pop-up com zona crítica e animação de subida/queda |
| `Assets/Scripts/TargetRangeSystem.cs` | Fila de alvos, alvos simultâneos e curva de dificuldade |
| `Assets/Scripts/GameManagerVR.cs` | Round de 75 s, combo, HUD, menu e tela de resultados |

## Passo a passo para montar a cena

1. **Unity Hub** → novo projeto **3D (URP ou Built-in)** com Unity **2022.3 LTS** ou **Unity 6**.
2. Window → Package Manager → instale:
   - `OpenXR Plugin` (`com.unity.xr.openxr`)
   - `XR Interaction Toolkit` (`com.unity.xr.interaction.toolkit`) — 2.5 ou 3.x
   - `Input System` (`com.unity.inputsystem`)
   - `TextMeshPro`
3. **Edit → Project Settings → XR Plug-in Management** → marque **OpenXR**.
   Em *OpenXR → Features*, ative **Meta Quest Support** (se for testar no Quest) ou o runtime do seu headset.
4. Converta a cena para XR: `GameObject → XR → Convert Main Camera to XR Rig`, ou arraste o prefab
   **XR Origin (VR)** para a cena.
5. Copie a pasta `Assets/Scripts` deste repositório para o projeto.
6. No **XR Origin**:
   - adicione `CharacterController` (o `VRPlayerRig` cria se não existir);
   - adicione o componente `VRPlayerRig` e arraste: `rigRoot = XR Origin`, `headCamera = Main Camera`, `xrOrigin = XR Origin`;
   - adicione `Continuous Move Provider` (locomoção) e `Snap Turn Provider` (giro de 30°).
7. **Arma**: crie um GameObject filho do controlador direito, adicione `XRWeapon`.
   Preencha `muzzle` (um empty na ponta do cano), `haptics` (`Haptic Impulse Player`), opcionalmente
   `tracerPrefab` (LineRenderer), `muzzleFlash` (ParticleSystem) e `impactPrefab`.
   Ligue o evento do `XRController` (`activate`) em `XRWeapon.TriggerPressed`.
8. **Alvos**: duplique um prefab de placa (Quad/Cube com Collider) 8 vezes, cada um com o componente `Target`.
   Coloque-os em um objeto pai com `TargetRangeSystem` e arraste todos no array `targets`.
9. **GameManager**: um GameObject vazio com `GameManagerVR`; arraste range, player, weapons, o `startPoint`
   (empty onde o jogador nasce) e os textos TMP do HUD.
10. Build para **Windows x64 + OpenXR** (ou Android para Quest). Rode com o headset conectado via Link/Air Link.

## Testar sem headset

`VRPlayerRig` detecta ausência de HMD e ativa o modo desktop:

- **WASD / setas** mover
- **Botão direito do mouse (segurando)** olhar
- Clique esquerdo dispara (ligue o `activate` do controlador ao mesmo método, ou chame `TriggerPressed` num `InputAction` de mouse)

Isso permite validar level design, alvos e pontuação antes de colocar o headset.

## Ideias de evolução (ver `../docs/ROADMAP-VR.md`)

- Portar o `XRWeapon` para um sistema de duas mãos (empunhadura + apoio).
- Trocar os alvos planos por silhuetas genéricas animadas.
- Ler o estado do jogo do BLACK (via port) e usar o cenário real como level — apenas com assets do usuário.
- Modo "relaxado": teleporte por ponto + giro suave, para quem passa mal com locomoção contínua.
