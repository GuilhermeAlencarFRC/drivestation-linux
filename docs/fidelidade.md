# Matriz de fidelidade

Referência: [NI FRC Driver Station documentada pela WPILib][ni-ds], com alvo
inicial na temporada 2026 e no roboRIO. Registrar a versão exata da aplicação
de referência antes de aprovar qualquer comparação visual.

Primeira prévia desktop implementada em C++/Qt 6. A validação descrita abaixo
cobre somente a demonstração local. Uma tela desenhada não comprova o
funcionamento do recurso correspondente com um robô real.

| Área | Estado atual | Evidência e pendências |
| --- | --- | --- |
| Estrutura visual | Painel compacto navegável | Capturas renderizadas; comparação exata com NI 2026 pendente |
| Operação | Seleção dos quatro modos; demonstração em Teleoperated, Autonomous e Test | Transições locais testadas; sequência Practice pendente |
| Habilitação | Enable/Disable e E-Stop locais | Bloqueios, reconexão, mudança de modo e reset testados; sem transporte |
| Teclado | Atalhos dentro da aplicação | Enter, Space e combinação de habilitação testados; captura global pendente |
| Status | Equipe persistida; bateria e indicadores demonstrativos | Origem simulada identificada; sem recepção de dados reais |
| Controles | Seis slots visíveis e entradas geradas na demonstração | Descoberta, ordenação, POV, rumble e hardware pendentes |
| Configuração | Preferências persistidas | Reinício, valores inválidos e falha de gravação testados; execução de dashboard e prática pendentes |
| Diagnóstico | Tela inicial com indisponibilidade explícita | Serviços, versões e ações no robô pendentes |
| Console e gráficos | Mensagens locais e curvas simuladas | Histórico e escala disponíveis; logs e telemetria reais pendentes |
| Desktop Linux | Janela flutuante e instalação CMake | Renderização offscreen 1x/2x e inicialização Wayland verificadas; demais ambientes e docking pendentes |

Veja [procedimentos e limites da validação](validacao-previa.md). Nenhum item
desta tabela representa validação de compatibilidade física com roboRIO.

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
