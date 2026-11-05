# Matriz de fidelidade

Referência: [NI FRC Driver Station documentada pela WPILib][ni-ds], com alvo
inicial na temporada 2026 e no roboRIO. Versão-alvo confirmada: 26.0.
A comparação completa de todas as abas permanece pendente.

Primeira prévia desktop implementada em C++/Qt 6. A validação descrita abaixo
cobre a demonstração local e entrada por dispositivos virtuais SDL. Uma tela desenhada não comprova o
funcionamento do recurso correspondente com um robô real.

| Área | Estado atual | Evidência e pendências |
| --- | --- | --- |
| Estrutura visual | Painel de 1024 × 200, modos retangulares e referência NI 26.0 | Paleta revista com capturas reais da 26.0; comparação completa de todas as abas pendente |
| Operação | Quatro modos; sequência Practice demonstrativa com cinco segmentos | Fronteiras, durações zero, conclusão e A-Stop testados; sem protocolo ou sons |
| Habilitação | Enable/Disable, A-Stop e E-Stop locais | Cancelamento em cada fase e retomada prevista de A-Stop testados; sem transporte |
| Teclado | Atalhos dentro da aplicação | Enter, Space, Backspace e combinação de habilitação testados; captura global pendente |
| Status | Equipe persistida; bateria e estado do robô demonstrativos; Joysticks usa a entrada Linux | Sem recepção de dados reais do robô |
| Controles | SDL2: descoberta, seis slots estáveis, eixos, botões, POV e rumble | Testes com dispositivos virtuais; modelos físicos, ordem persistente e equivalência de mapeamento NI pendentes |
| Configuração | Preferências persistidas e tempos aplicados ao Practice local | Alterar tempos cancela a execução; execução de dashboard pendente |
| Diagnóstico | Tela inicial com indisponibilidade explícita | Serviços, versões e ações no robô pendentes |
| Console e gráficos | Mensagens locais e curvas simuladas | Histórico e escala disponíveis; logs e telemetria reais pendentes |
| Desktop Linux | Janela flutuante e instalação CMake | Renderização offscreen 1x/2x e inicialização Wayland verificadas; demais ambientes e docking pendentes |

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
