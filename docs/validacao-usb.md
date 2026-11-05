# Validação de entrada Linux — 0.4.0

Data: 2026-11-05. Referência visual preservada: NI FRC Driver Station 26.0.
Esta etapa implementa o caminho de entrada SDL2; não envia pacotes a robôs.

## Ambiente e resultados locais

- Arch Linux x86_64, GCC 16.2.1, Qt 6.12.0; build CMake Debug sem avisos do projeto.
- SDL2-compat 2.32.74 / SDL3 3.4.18 do sistema: três suítes CTest aprovadas.
- SDL2 original 2.32.10, compilado do release oficial em diretório local isolado:
  as mesmas três suítes aprovadas. A biblioteca carregada foi conferida pelo loader.
- **88 cenários**, contando os dados parametrizados; 94 entradas QtTest com
  fixtures. Zero falhas, testes pulados ou testes de hardware físico alegados.
- Instalação isolada e `--version`: `drivestation-linux 0.4.0`.
- `clang-format`, `git diff --check` e actionlint 1.7.12 aprovados.
- ASM Gate 5: núcleo **76/70**, interface **79/70**. Foram analisados integralmente
  dois lotes de código e testes com 26.462 e 24.742 caracteres, respectivamente.
  Os avisos genéricos foram conferidos: ownership Qt, verificação de abertura SDL,
  limites dos slots, tratamento de erro de descoberta e fechamento dos handles.

## Cobertura de comportamento

A suíte de controles acrescenta 20 cenários aos 68 existentes:

- Normalização com extremos −32768, zero e 32767.
- Seis slots estáveis, remoção sem deslocar os demais, reconexão e oito dispositivos
  sem emitir mudanças de topologia repetidas por causa dos excedentes.
- Falha de inicialização com nova tentativa, erro de descoberta e falha de leitura
  limpando os snapshots; logs repetidos são suprimidos até mudança ou recuperação.
- Backend SDL real com joystick virtual: oito eixos, dezesseis botões e dois POVs;
  todos os oito sentidos, centro, soltura de botão e remoção foram verificados.
- Dispositivo virtual conectado depois da inicialização e preservação da referência
  SDL de outro consumidor ao destruir o backend.
- Roteamento independente dos motores de rumble e comando de parada no callback
  virtual; liberação dos handles e rejeição de saída após fechamento.
- UI com backend injetado: seleção de slot, eixos, botão, POV, indicador Joysticks,
  F1, ausência de entradas fictícias na demo e interrupção de Practice em espera.
- Vibração interrompida ao liberar, selecionar outro slot, trocar de aba, perder
  ativação, desconectar, fechar ou receber erro de saída. Os timers não retomam
  o efeito depois do cancelamento; atraso na entrega do timer também o cancela.

O teste de erro de rumble verifica que a falha desabilita novas tentativas até
Rescan. Cada pulso solicita 100 ms ao SDL; não foi medida a duração física de
qualquer efeito em hardware.

## Particularidade do ambiente Arch

Foi observada uma incompatibilidade no callback de joystick **virtual** do
SDL2-compat 2.32.74: o retorno inteiro zero, usado como sucesso pelo SDL2,
é repassado como booleano ao SDL3. O
[código do adaptador](https://github.com/libsdl-org/sdl2-compat/blob/main/src/sdl2_compat.c)
foi consultado para conferir a causa.

A fixture virtual testa primeiro a convenção documentada e adapta o retorno
somente quando observa essa falha sem mensagem de erro e com callback executado.
Essa adaptação existe apenas no teste; o backend de produção exige sucesso
conforme a API SDL2. A execução com SDL2 original passou sem essa adaptação.
Não foram alteradas bibliotecas ou permissões do sistema.

## Capturas e limites

A captura [USB com joystick virtual](images/ni26-usb-sdl-virtual.png) mostra a entrada
produzida por SDL, com comunicação e código do robô indisponíveis. A captura
[USB sem dispositivos](images/ni26-usb-empty.png) mostra os campos vazios.
Ambas foram geradas pelo aplicativo Qt e inspecionadas em 1024 × 200.
Capturas anteriores são registros históricos de outras etapas.

Nenhum controle físico foi identificado neste ambiente. Modelo físico, mapeamento
exato da NI, rumble real, permissões de uma distribuição limpa, Bluetooth,
suspensão, captura global de teclado, FMS e comunicação com roboRIO permanecem
sem validação. Ordem persistente e reordenação manual ainda não estão implementadas.

## Reproduzir

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/controller_tests
```

Os dispositivos virtuais são criados e removidos dentro do processo de teste.
Não é necessário acesso a `/dev/uinput` nem executar como administrador.
