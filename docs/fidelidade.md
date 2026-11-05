# Matriz de fidelidade

Referência: [NI FRC Driver Station documentada pela WPILib][ni-ds], com alvo
inicial na temporada 2026 e no roboRIO. Registrar a versão exata da aplicação
de referência antes de aprovar qualquer comparação visual.

Todos os itens abaixo estão **planejados**, sem implementação ou validação.
Uma tela desenhada não comprova o funcionamento do recurso correspondente.

| Área | Alvo de reprodução | Evidência necessária |
| --- | --- | --- |
| Estrutura visual | Painel compacto, navegação e hierarquia dos indicadores | Capturas comparativas na mesma escala |
| Operação | Teleoperated, Autonomous, Practice e Test | Transições verificadas por cenário |
| Habilitação | Enable, Disable e indicação inequívoca do estado | Testes do núcleo de estados e da interface |
| Teclado | Atalhos equivalentes e parada de emergência | Testes com foco, sem foco e após suspensão |
| Status | Equipe, bateria, comunicação e código do robô | Valores confrontados com uma fonte de teste |
| Controles | Até seis slots, ordem dos dispositivos e inspeção de entradas | Testes com controles físicos e desconexão |
| Configuração | Equipe, dashboard, dados de jogo e tempos de prática | Persistência e aplicação verificadas |
| Diagnóstico | Conexões, versões e informações úteis do robô | Dados reais identificados e reproduzíveis |
| Console e gráficos | Mensagens e histórico operacional | Comparação com eventos conhecidos |
| Desktop Linux | Janela, posicionamento, escala e leitura de dispositivos | Testes nas sessões e distribuições declaradas |

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
