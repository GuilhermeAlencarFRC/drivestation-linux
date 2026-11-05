# 0006 — Base UDP e verificação em loopback

Data: 2026-11-05. Estado: aceita para 0.5.0.

Avançar o marco de comunicação em uma biblioteca Qt Network independente da
interface e em `drivestation-network-check`, que usa exclusivamente IPv4
loopback e portas efêmeras. O aplicativo gráfico continua com sua demo local.
Não ligar DemoSession a um robô real nesta etapa.

## Referências e alcance

O levantamento de [DS → roboRIO][out] e [roboRIO → DS][in] documenta o perfil
UDP versão 1, transmissão em 20 ms e destinos 1110/1150. É documentação pública
de engenharia reversa, não uma especificação normativa da NI 26.0. Não comprova
compatibilidade com uma imagem roboRIO 2026.

A [HAL da WPILib 2026.2.1][hal] expõe seis controles, buffers de doze eixos e doze
POVs e máscara de 32 botões. Adotar esses limites no perfil experimental do codec;
não tratar os enums da HAL como os números no fio. A estação no UDP é 0–5.

Há divergências nas implementações públicas antigas. O perfil deste projeto usa
comprimento de tag incluindo ID, excluindo o próprio comprimento, e `ceil(n/8)`
bytes para a máscara de botões, em ordem de rede e bit zero para o primeiro botão.
A [implementação ds 1.2.4][rust] e sua [rotina de empacotamento][bits] dão referência
para essa interpretação. Não foram copiados módulos dessas implementações.
Vetores de teste são exemplos calculados a partir do perfil, não capturas NI.

## Contrato de implementação

- Codec puro: cabeçalhos, modos Teleop/Test/Autonomous, habilitação, E-Stop,
  estação, seis tags de joystick, status, bateria e saídas de joystick recebidas.
- Não emitir FMS, reinício de código, reboot, data/hora, TCP ou descoberta.
- Eixos no fio são int8; a função de quantização usa o byte alto de um int16
  SDL. Isso não valida o mapeamento XInput da NI.
- Rejeitar modos, contagens, comprimentos e POVs fora do perfil; não truncar
  entrada silenciosamente. Tags desconhecidas bem formadas podem ser ignoradas.
- Datagrama máximo de 1024 bytes; parsing por limites antes de cada leitura.
- Transporte com endereço e portas explícitos, sem broadcast ou descoberta.
  Vincular socket sem compartilhar a porta; validar endereço e porta da origem.
- Associar resposta a envio recente e aceitar apenas progresso da sequência.
  Essa correlação é uma hipótese do perfil de bancada a conferir com capturas NI.
- Watchdog local de 250 ms e cancelamento após intervalo de envio de 100 ms.
  São políticas desta implementação, não limites declarados para o firmware NI.
- Conexão, perda de código, mudança de modo/estação, stop/start e atraso de
  atualização limpam a solicitação de habilitação. Reconexão nunca reabilita.
- E-Stop fica travado durante a vida da sessão, inclusive entre stop/start.
  O status recebido não limpa essa trava. Close solicita um último Disable;
  entrega UDP não é garantida.
- Saídas/rumble recebidos são dados decodificados; não acionam hardware nesta etapa.

## Validação necessária

Vetores binários explícitos, rejeição de pacotes truncados/malformados, limites de
contagens, botões nas fronteiras de bytes, sequência com wrap, respostas repetidas
ou atrasadas, timeout sem renovar com lixo, E-Stop, restart e atraso do event loop.
Testar também sockets UDP reais em loopback, porta ocupada, origem errada e o
executável de verificação no CI Debug/Release e no pacote instalado.

TCP, data/hora, descritores USB, dados de jogo, descoberta e integração da UI com
rede ficam para etapas seguintes. A validação física com roboRIO e a comparação
com pacotes NI 26.0 continuam necessárias antes de declarar compatibilidade.

[out]: https://frcnetworking.readthedocs.io/en/latest/driverstation/ds_to_rio.html
[in]: https://frcnetworking.readthedocs.io/en/latest/driverstation/rio_to_ds.html
[hal]: https://github.com/wpilibsuite/allwpilib/blob/v2026.2.1/hal/src/main/native/include/hal/DriverStationTypes.h
[rust]: https://docs.rs/ds/1.2.4/src/ds/proto/udp/outbound/types/tags.rs.html
[bits]: https://docs.rs/ds/1.2.4/src/ds/util.rs.html
