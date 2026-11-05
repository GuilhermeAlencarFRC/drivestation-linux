# Validação da base UDP — 0.5.0

Data: 2026-11-05. O visual continua com alvo NI 26.0; esta etapa é uma base de
comunicação experimental separada da janela gráfica.

## Resultado local

- Arch Linux x86_64, Qt 6.12.0, GCC 16.2.1, CMake/Ninja Debug.
- Cinco testes CTest aprovados: quatro suítes QtTest e o executável de verificação.
- **146 cenários QtTest**, incluindo dados parametrizados, mais **um cenário de
  verificação CLI**. São 154 entradas QtTest contando fixtures; zero falhas ou skips.
- As 58 situações da nova suíte de rede somam-se aos 88 cenários anteriores.
- Build separado dos alvos de rede com `-fsanitize=address,undefined
  -fno-omit-frame-pointer`: suíte e verificador aprovados, sem relatórios de
  AddressSanitizer ou UBSan. O teste inclui 10 mil datagramas pseudoaleatórios
  determinísticos, com tamanho e conteúdo malformados.
- Instalação isolada contém ambos os executáveis; o verificador instalado passou.
- `clang-format`, `git diff --check` e actionlint 1.7.12 aprovados.
- ASM Gate 5 aprovado em três lotes: codec **84/70**, conexão/transporte **76/70**
  e ferramenta/build **79/70**. Os lotes ficaram abaixo do limite de análise.
  Foram conferidos os avisos sobre limites de leitura, alocação dinâmica dos
  datagramas, validação do destino, lifetime de objetos C++/Qt e erros de socket.

## Comportamento verificado

| Área | Evidência |
| --- | --- |
| Cabeçalho | Bytes esperados explicitamente para Teleop, Test, Autonomous, Enable, E-Stop e estação |
| Entradas | Eixos negativos/extremos, buracos entre slots, seis controles, limites 12/32/12 e POV neutro/direcional |
| Botões | Máscaras para 0, 1, 8, 9, 16, 17 e 32 botões; ordem de bytes e bits conferida com vetores calculados |
| Estado | Bateria 12,5 V, modos, código, brownout, inicialização, pedido e saídas de joystick |
| Parsing | Cabeçalhos curtos, tags truncadas, tamanho zero, limites, bits não usados, modo inválido, versão errada, datagrama >1024 bytes e tags desconhecidas |
| Sequência | Wrap em 65535→0, duplicação, inversão e silêncio por mais de duas voltas completas do contador |
| Conexão | Precisa de resposta recente e código disponível; perda e recuperação não reabilitam |
| Cancelamento | Timeout 250 ms, atraso de envio 100 ms, perda/inicialização de código, configuração inválida, troca de modo/estação e restart |
| E-Stop | Solicitação local ou status remoto travam a sessão, inclusive após stop/start |
| UDP real | Loopback com origem/porta verificadas, datagrama excessivo/corrompido, porta ocupada, repetição de resposta antiga e Disable final |
| Executável | Habilita o par local, corta respostas, observa timeout, reconecta desabilitado, exige novo Enable e aplica E-Stop |

A suíte também verifica que um status emprestado do próprio snapshot pode ser
submetido ao vencimento sem permanecer como referência a um objeto já destruído.
O estado recebe esse valor por cópia antes de expirar o snapshot anterior.

## Executar

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/drivestation-network-check --json
```

A saída JSON registra `success`, verificação de entradas, habilitação observada,
timeout desabilitado, reconexão desabilitada, E-Stop e contadores de pacotes.
O processo retorna zero somente quando o cenário termina com todos os critérios.
Ele usa 127.0.0.1 e portas efêmeras, sem opção de destino externo ou captura de USB.

## Limites da conclusão

Os vetores binários foram calculados a partir do perfil documentado, não extraídos
de capturas NI. O par local compartilha o codec do projeto; por isso os vetores
explícitos e testes de rejeição complementam os testes de ida e volta.

A correlação por sequência e a política de porta de origem precisam ser conferidas
com tráfego NI 26.0/roboRIO. Watchdog e atraso de envio são políticas locais, não
validação de temporização em tempo real. Não foi verificada a imagem roboRIO 2026.

Faltam TCP, descoberta, data/hora, descritores de controles, dados de jogo,
integração dos comandos e USB da interface com a rede, atuação de rumble remoto
e testes com equipamento físico. O aplicativo gráfico continua sendo a demo
local. O marco de compatibilidade com roboRIO ainda não está concluído.
