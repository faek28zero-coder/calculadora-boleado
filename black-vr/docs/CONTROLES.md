# Controles e configuração

## Padrão no headset

| Ação | Controle |
|---|---|
| Atirar | gatilho direito |
| Recarregar | squeeze direito (borda de subida) |
| Andar / correr | analógico esquerdo |
| Girar o corpo | analógico direito (contínuo) |
| Giro por toque | analógico direito, empurrado até o fim (30° por padrão) |
| Confirmar / voltar / menu | A / B / botão menu |
| Olhar, mirar, agachar | a sua cabeça |

Teclado, mouse e controle continuam valendo ao mesmo tempo: os controles do headset são somados ao
pad que o jogo lê, depois do pad físico e das teclas.

## black.ini (seção de VR, sem seção: sem VR)

```ini
vr = 1
vr_ipd = 63
vr_move_scale = 1.0
vr_snap_turn = 30
vr_fov = 0
vr_input = 1
vr_hud = 1
vr_reload_button = 16384
```

| chave | o que é |
|---|---|
| `vr` | liga o VR. Sem runtime instalado, o jogo avisa e roda em tela plana. |
| `vr_ipd` | distância entre as pupilas em mm. Se a escala parecer errada, ajuste aqui antes de mexer em mais nada. |
| `vr_move_scale` | 1.0 = agachar na vida real abaixa a câmera 1:1. Menos que 1 suaviza, mais que 1 exagera. |
| `vr_snap_turn` | graus por toque lateral no analógico direito. 0 desliga (só giro contínuo). |
| `vr_fov` | FOV horizontal do jogo em VR. **0 = seguir o headset**, que é o que fecha a conta do estéreo. |
| `vr_input` | 0 = ignora os controles do headset (só teclado/controle). |
| `vr_hud` | 0 = não repete o HUD nos dois olhos. |
| `vr_reload_button` | máscara Xbox do botão de recarregar: 0x4000 X, 0x8000 Y, 0x2000 B, 0x1000 A. |

## Linha de comando

```
--vr                     liga o VR
--vr-ipd MM              distância interpupilar
--vr-move-scale F        movimento real da cabeça -> corpo
--vr-snap-turn D         graus por toque lateral (0 = desligado)
--vr-fov D               FOV do jogo em VR (0 = seguir o headset)
--vr-reload-button MASK  botão de recarregar
--no-vr-input            ignora os controles do headset
--no-vr-hud              sem HUD repetido
--vr-selftest N          cenário de calibração por N segundos, sem o jogo
```

## Calibração rápida

1. `black.exe --vr-selftest 30`.
2. Olhe para o chão: cada quadrado da grade tem **1 m**.
3. O alvo laranja está a **2 m** do centro do seu olhar.
4. A régua à esquerda marca de **10 em 10 cm**, de -1,5 m a +1,5 m de altura.
5. Se a escala estiver certa e não houver diferença entre os olhos (sem dor de cabeça em 1 minuto),
   a camada de VR está boa. Passe para o jogo.
