# Validação de Practice local — 0.2.0

Data: 2026-11-05. Ambiente: Arch Linux x86_64, Qt 6.12.0, GCC 16.2.1,
CMake/Ninja, build Debug. Nenhuma comunicação com robô foi implementada nesta etapa.

## Resultado local

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

As duas suítes passaram: **67 cenários**, sendo 11 da interface original e 56
de Practice, contando cada linha parametrizada. A saída Qt Test registra 71
entradas aprovadas ao incluir os quatro eventos de inicialização/encerramento.
Zero falhas e zero testes pulados. A formatação C++ e `git diff --check` passaram.

ASM Gate 5 aprovado com 79/70 no projeto `drivestation-linux`. Foram enviados
os arquivos completos e a saída dos testes. A análise remota inicial cobriu
30.000 de 90.268 caracteres; a revisão integral do agente no loop concluiu
a avaliação. Os avisos genéricos foram confrontados com ownership Qt, validação
de entradas, tratamento de erros e cobertura local.

## Evidências

- Fronteiras exatas em milissegundos e equivalência entre passos grandes e
  particionados no motor puro.
- As 32 combinações de segmentos presentes/zerados, incluindo duração total zero.
- Limites das durações, entrada negativa, intervalo extremo e tentativa de
  reiniciar uma sequência já ativa.
- A-Stop sem congelar o relógio; retomada em Teleop; ausência de aviso de retomada
  quando não há trecho habilitado futuro.
- Trinta combinações de fases/interrupções: Disable, desconexão, E-Stop, modo e
  alteração de tempos em Countdown, Autonomous, Delay, Teleoperated, End Game
  e Autonomous interrompido por A-Stop.
- Cancelamento após atualização atrasada, sem avanço posterior para um estado
  habilitado.
- Interface: Disable disponível durante Countdown, Backspace preservado nos
  campos de texto, cancelamento por alterações, perda de ativação e fechamento.

As capturas [Countdown](images/practice-countdown.png),
[A-Stop](images/practice-astop.png) e [Teleop](images/practice-teleop.png) foram
geradas da janela Qt real em modo offscreen, com tempos reduzidos para inspeção.
Foram conferidos os textos, botões, seleção Practice, tempo restante, indicação
da origem simulada e aviso de habilitação futura.

## Limites

Este resultado cobre a demonstração. Não valida rede, watchdog do roboRIO,
joysticks físicos, FMS, captura global de teclado ou suspensão real do computador.
O limite de atraso do loop é uma proteção local, não uma garantia de detectar
suspensão. Não houve operação de um robô.

As durações padrão continuam sendo as da referência visual provisória. A ordem
dos segmentos e o A-Stop foram orientados pela
[documentação da NI Driver Station publicada pela WPILib](https://docs.wpilib.org/en/stable/docs/software/driverstation/driver-station.html).
A implementação ainda exige comparação com a versão exata de referência. O
indicador de status pisca em laranja; não há efeito de piscar em toda a janela
nem sons de partida nesta etapa.
