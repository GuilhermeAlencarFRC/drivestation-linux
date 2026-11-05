# 0003 — Aproximar o painel diretamente da NI Driver Station

Data: 2026-11-05. Estado: implementada na prévia 0.3.0.

O dono pediu que a interface acompanhasse mais de perto a Driver Station
original. A referência disponível continua sendo o conjunto de imagens da
documentação WPILib; a captura de Operation identifica a NI 24.0b96. Não foi
obtida uma captura exata da NI 2026 nesta etapa.

## Ajustes

| Área | Organização adotada |
| --- | --- |
| Janela | 1024 × 200 pixels lógicos de conteúdo, com largura expansível |
| Colunas | Navegação de 36 px, painel de 398 px, status de 150 px, console expansível e navegação direita de 34 px |
| Modos | Quatro botões retangulares, seleção escura, sem indicadores de rádio |
| Operação | Enable/Disable agrupados; métricas do PC, controles de janela e estação nas posições da referência |
| Navegação | Ícones cinza quando inativos e ciano na seleção; indicador lateral e botão de fechar à direita |
| Console | Engrenagem no canto superior, sem título nem botão Clear ocupando a área |
| Setup | Equipe centralizada em campo curto, dashboard escuro, dados de jogo à esquerda e tempos à direita |
| USB | Lista com seis posições; eixos estreitos, indicadores verticais de botões, POV e área de rumble |
| Diagnostics | Comunicação à esquerda, versões e ações à direita |
| CAN/Power | Três grupos: Disable, Short e CAN Metrics |

Os controles de simulação ficam no menu da engrenagem, acessível também por
F10 ou pelo menu de contexto da janela. O console inicia vazio. A simulação
continua identificada no status e o título indica a prévia offline; a faixa inferior foi retirada.
O tempo restante de Practice aparece no tooltip do cronômetro.

## Limites preservados

Não apresentar valores fictícios como métricas do PC, POV, rumble ou diagnóstico
de hardware. Os controles indisponíveis ficam desabilitados e os valores sem
fonte aparecem vazios ou como indisponíveis. Não foram incorporadas imagens,
logos ou binários da NI; os ícones são desenhos próprios.

As mudanças aproximam organização, medidas e cores da referência. Isso não é
uma aprovação de equivalência pixel a pixel ou de compatibilidade física.
O efeito de Practice continua restrito ao status; sons e encaixe da janela
seguem pendentes.

## Verificação

As suítes de estado e interface incluem o uso do menu durante Practice:
abrir/fechar o menu preserva a sequência, e E-Stop continua cancelando. O teste
de Reset usa os controles visíveis do popup. A perda de ativação da aplicação
continua cancelando a execução.
