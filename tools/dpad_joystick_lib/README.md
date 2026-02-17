# Biblioteca D-Pad & Joystick para BU

Esta biblioteca fornece funções reutilizáveis para adicionar controles de D-pad e botão de tiro em projetos BU.

## Arquivos
- `dpad_joystick.bu`: Biblioteca com funções de setup e desenho do D-pad.
- `demo_dpad.bu`: Exemplo de uso simples.

## Como usar
1. Inclua o arquivo `dpad_joystick.bu` no seu script:
   ```
   include "dpad_joystick.bu"
   ```
2. Chame `setup_dpad_controls()` no início.
3. No seu draw, chame `draw_dpad_hud()` para desenhar o HUD.
4. Use `key_down(KEY_LEFT)`, etc, para ler o estado dos botões.

## Exemplo
Veja `demo_dpad.bu` para um exemplo completo.
