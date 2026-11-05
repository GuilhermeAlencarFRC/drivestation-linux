# 0005 — Entrada de controles por SDL no Linux

Data: 2026-11-05. Estado: aceita para a etapa 0.4.0.

Usar SDL 2.24 ou posterior, sem criar uma janela SDL. Qt continua responsável
pela interface. SDL2 foi escolhido por estar disponível no Ubuntu 24.04 e por
oferecer joysticks virtuais para testes sem privilégios. Não alterar permissões
de `/dev/input` nem capturar teclas por essa integração.

## Contrato desta etapa

- Detectar dispositivos e mudanças de conexão a cada atualização de entrada.
- Manter seis slots. Conexões existentes conservam seus índices; uma entrada
  nova ocupa o primeiro slot vazio. Dispositivos excedentes não são abertos.
- Usar o instance ID do SDL durante a conexão. Ordem após reinício, travas
  persistentes e reordenação manual ficam para outra etapa.
- Capturar eixos de 16 bits, botões e POVs. A interface compacta mostra os seis
  primeiros eixos, doze primeiros botões e primeiro POV; os totais são informados
  no tooltip. O snapshot preserva entradas adicionais, com limite de 256 por tipo.
- Remover valores antigos quando um dispositivo desaparece ou a leitura falha.
- Rescan/F1 força nova detecção; seleção de slot altera apenas a inspeção.
- Alteração de conexão cancela a sessão demonstrativa, inclusive Practice
  aguardando habilitação. Nenhum estado real de controle é inventado pela demo.
- A simulação de robô continua local; USB mostra somente o backend de entrada.
- Rumble, quando suportado, responde às barras enquanto pressionadas. Cada
  comando solicita 100 ms; liberar, selecionar outro slot, ocultar a aba,
  perder ativação, desconectar ou fechar solicita a interrupção do efeito. A
  resposta física do driver ainda requer medição em hardware.

## Organização e testes

Separar o backend SDL da atribuição de slots e da interface. Injetar backend
nos testes de UI/erros; testar também o adaptador real com joysticks virtuais SDL,
incluindo valores extremos, diagonais, remoção e callback de rumble.

O ambiente atual não tem um joystick físico identificado. Testes virtuais
validam o caminho SDL e a integração, sem comprovar compatibilidade de um
modelo físico. Suspensão real, mapeamento de gamepads e protocolo roboRIO
continuam sem validação nesta etapa. Os eixos são os índices crus do dispositivo,
sem remapeamento automático para a convenção Xbox.

Fontes: [SDL JoystickUpdate](https://wiki.libsdl.org/SDL2/SDL_JoystickUpdate),
[joystick virtual](https://wiki.libsdl.org/SDL2/SDL_JoystickAttachVirtualEx) e
[rumble](https://wiki.libsdl.org/SDL2/SDL_JoystickRumble).
