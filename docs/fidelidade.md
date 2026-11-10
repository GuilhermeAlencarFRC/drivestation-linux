# Matriz de fidelidade

Referência: [NI FRC Driver Station documentada pela WPILib][ni-ds], com alvo
inicial na temporada 2026 e no roboRIO. Versão-alvo confirmada: 26.0.
A comparação completa de todas as abas permanece pendente.

Primeira prévia desktop implementada em C++/Qt 6. A validação descrita abaixo
cobre a demonstração local, entrada por dispositivos virtuais SDL e
comunicação UDP com um par local de teste. Uma tela desenhada não comprova o
funcionamento do recurso correspondente com um robô real.

| Área | Estado atual | Evidência e pendências |
| --- | --- | --- |
| Estrutura visual | Painel de 1024 × 200, modos retangulares e referência NI 26.0 | Paleta revista com capturas reais da 26.0; comparação completa de todas as abas pendente |
| Operação | Quatro modos locais; Teleop/Autonomous/Test também em UDP experimental | Practice e A-Stop permanecem demonstrativos; sem sons ou validação física |
| Habilitação | Enable/Disable/E-Stop locais ou UDP; A-Stop local | Pedidos separados da confirmação UDP; cancelamentos e reconexão desabilitada testados em loopback |
| Teclado | Atalhos dentro da aplicação | Enter, Space, Backspace e combinação de habilitação testados; captura global pendente |
| Status | Demo identificada; perfil UDP mostra bateria/código/estado recebidos; Joysticks usa entrada Linux | Recepção validada com par em loopback; falta equipamento físico |
| Controles | SDL2: descoberta, seis slots estáveis, eixos, botões, POV e rumble | Testes com dispositivos virtuais; modelos físicos, ordem persistente e equivalência de mapeamento NI pendentes |
| Configuração | Preferências, Practice local e envio de Game Data TCP | Alterar tempos/dados cancela a execução; execução de dashboard pendente |
| Diagnóstico | UDP TX/RX, rejeições, RTT; estado TCP e versões recebidas | Versões exercitadas em loopback; demais serviços e ações no robô pendentes |
| Console e gráficos | Console TCP; gráficos de RTT/bateria recebidos por UDP; exportação manual JSON | Histórico limitado, lacunas sem interpolação e separação demo/rede; formato próprio, sem leitor de logs NI; demais curvas e compatibilidade física pendentes |
| Rede | UDP/TCP, descritores, Game Data, UTC/fuso e busca mDNS IPv4 por equipe | Busca/hints com seleção manual e testes em loopback; tipos SDL e compatibilidade física NI ainda pendentes |
| Desktop Linux | Janela flutuante, instalação CMake e arquivo alfa CPack identificado como testes | Pacote com bibliotecas do sistema; outras distribuições e docking pendentes; Wayland foi verificado em etapa anterior |

Veja [procedimentos e limites da validação](validacao-previa.md). Nenhum item
desta tabela representa validação de compatibilidade física com roboRIO.
Os registros de [Practice](validacao-practice.md) e [USB](validacao-usb.md)
descrevem os testes adicionais.

## Como registrar a aprovação

Para cada item implementado, registrar:

1. Versão da referência e comportamento esperado.
2. Commit avaliado, ambiente e procedimento de teste.
3. Resultado observado e evidências.
4. Diferenças conhecidas e limitações.

Recursos dependentes de hardware permanecem sem validação até haver evidência
com o equipamento correspondente. Dados demonstrativos devem aparecer como
simulados na interface.

[ni-ds]: https://docs.wpilib.org/en/stable/docs/software/driverstation/driver-station.html
