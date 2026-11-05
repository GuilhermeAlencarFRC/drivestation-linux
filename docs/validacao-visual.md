# Validação da interface 0.3.0

Data: 2026-11-05. Referência de organização e dimensões: imagens oficiais da
documentação WPILib instaladas com 2026.2.1, incluindo a NI 24.0b96.

## Resultado

Build Debug C++17/Qt 6.12.0 no Arch Linux concluído sem warnings. As duas suítes
CTest passaram com **68 cenários**, ou 72 entradas Qt Test ao incluir os quatro
eventos de inicialização/encerramento; zero falhas e zero testes pulados.
Formatação e `git diff --check` passaram.

A interface de 1024 × 200 foi renderizada em modo offscreen. A inicialização e
captura no Wayland local também passaram. Foram inspecionadas as capturas de
[Operation](images/ni-operation.png), [Setup](images/ni-setup.png),
[USB](images/ni-usb.png), [Diagnostics](images/ni-diagnostics.png),
[CAN/Power](images/ni-power.png), [Charts](images/ni-charts.png),
[Countdown](images/ni-practice-countdown.png), [A-Stop](images/ni-practice-astop.png),
[Teleop](images/ni-practice-teleop.png) e [menu](images/ni-menu.png).

Foi conferida a largura das lâmpadas e a fonte em todas as abas. O texto
renderizado do indicador de comunicação tem a mesma extensão em Operation e
Diagnostics. O teste adicional de popup verifica que abrir/fechar a engrenagem
preserva Practice e que Space continua acionando E-Stop. O teste de Reset usa
o controle visível no menu.

ASM Gate 5 aprovado com **79/70**, com os arquivos completos e a saída real dos
testes. O modelo remoto analisou 30.000 de 99.689 caracteres; a revisão integral
do agente no loop concluiu a análise. Ownership de QMenu/QWidgetAction, ordem
de construção, ativação, atalhos e preservação dos estados foram conferidos.
Um aprendizado sobre cancelamento de sequências pendentes foi salvo no ASM
com escopo explícito `drivestation-linux`.

## Limites

As mudanças aproximam a referência disponível; não comprovam equivalência
pixel a pixel com a NI 2026. Fontes e decorações da janela dependem do sistema.
Não há validação física de roboRIO, USB, POV, rumble, FMS ou suspensão. As áreas
sem fonte real de dados continuam indisponíveis; a demonstração não envia
comandos a robôs. Sons e docking permanecem pendentes.
