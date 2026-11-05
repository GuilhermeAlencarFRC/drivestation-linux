# 0002 — Sequência de Practice determinística na demonstração

Data: 2026-11-05. Estado: aceita para a demonstração local.

## Comportamento

Adicionar um motor de sequência independente de widgets e de transporte.
A entrada de tempo é um intervalo em milissegundos; os testes avançam esse
tempo diretamente, sem esperar a duração de uma partida.

As cinco durações de Setup são segmentos consecutivos: Countdown, Autonomous,
Delay, Teleoperated e End Game. End Game é o trecho final habilitado em Teleop,
somado ao campo Teleoperated. Não há um novo modo de controle para End Game.
Os valores padrão existentes são preservados: 5, 15, 1, 105 e 30 segundos.
Esses valores reproduzem a referência visual provisória, não uma regra de
duração da temporada 2026. A duração exata e os sons da NI ainda exigem
comparação com o aplicativo de referência.

Countdown e Delay mantêm a demonstração desabilitada. Autonomous, Teleoperated
e End Game habilitam apenas a sessão local; Complete encerra desabilitado.
Segmentos com duração zero são ignorados. Cinco durações zero concluem
imediatamente sem habilitar. Cada duração deve estar entre 0 e 600 segundos.

## Interrupções

- A-Stop, por botão ou Backspace fora de campos de texto, só atua durante
  Autonomous habilitado. O tempo continua; a habilitação retorna em Teleop.
- Countdown, Delay e A-Stop mostram aviso de habilitação futura quando houver
  um trecho habilitado restante. O indicador de status pisca em laranja.
- Disable cancela a sequência inteira, inclusive quando já estiver desabilitada
  em Countdown, Delay ou A-Stop.
- E-Stop, desconexão, troca de modo, equipe, estação ou tempos cancelam a
  sequência. E-Stop continua travado até Reset demo.
- Perda de ativação da janela cancela a execução local. Não há retomada implícita.
- Um novo Enable explícito pode iniciar outra sequência após cancelamento ou fim.
- Estado e progresso da sequência não são persistidos.

## Tempo e interface

O adaptador da interface usa o relógio monotônico `QElapsedTimer`. Reiniciar a
medição ao pedir Enable evita consumir tempo anterior ao clique. Uma atualização
da sessão atrasada mais de um segundo cancela a execução local; isso cobre
travamentos prolongados do loop, sem representar uma garantia de detecção de
suspensão do sistema. A suspensão física continua sem validação.

A interface exibe o tempo da sequência, incluindo as esperas, e o tempo restante
do segmento. A seleção Practice permanece marcada durante todas as fases.
Os limites visuais da prévia e a ausência de comunicação com robôs permanecem.

## Validação esperada

Testar fronteiras exatas, fases zeradas, conclusão, A-Stop, cancelamento em cada
fase, passos grandes no motor puro, atraso no adaptador, alterações de tempos e
interação por teclado/mouse. Conferir cancelamento antes de qualquer avanço
posterior e persistência somente das preferências.

Referência comportamental: [NI Driver Station, Practice e A-Stop](https://docs.wpilib.org/en/stable/docs/software/driverstation/driver-station.html).
