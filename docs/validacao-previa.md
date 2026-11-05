# Validação da primeira prévia desktop

Data: 2026-11-05. Esta validação cobre a prévia local, sem robô e sem transporte.

Código avaliado: [commit abb1f18](https://github.com/GuilhermeAlencarFRC/drivestation-linux/commit/abb1f18).

## Ambiente

- Arch Linux x86_64, Qt 6.12.0 e GCC 16.2.1.
- Build Debug via CMake e Ninja, com `-Wall -Wextra -Wpedantic`.
- Testes Qt Test executados por CTest com `QT_QPA_PLATFORM=offscreen`.
- Capturas da própria aplicação em 1100 × 224 pixels lógicos.
- Inicialização no compositor Wayland local e captura com escala 2x verificadas.
- Instalação em prefixo isolado, incluindo binário, atalho e ícone, verificada;
  `--help` executado com sucesso a partir do binário instalado.

## Procedimentos

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Os testes verificam inicialização desconectada, habilitação explícita,
desconexão/reconexão, troca de modo, trava de E-Stop, reset local, relógio,
persistência e recuperação de configurações inválidas, falha de gravação,
navegação das abas, atalhos e perda de ativação da janela.

As capturas Operation, Setup e USB/Both foram inspecionadas visualmente.
Um problema no tamanho do editor interno dos campos numéricos foi encontrado
nessa inspeção e corrigido no estilo dos widgets.

Resultado: **11 cenários aprovados**, com 13 entradas aprovadas na saída do Qt
Test ao incluir inicialização e encerramento; zero falhas e zero testes pulados.

O **ASM Gate 5 foi aprovado com 79/70**, no projeto `drivestation-linux`, com o
conteúdo real dos arquivos e a saída dos testes fornecidos. A análise inicial do
modelo remoto cobriu 30.000 de 62.141 caracteres e ficou inconclusiva. A revisão
integral pelo agente no loop concluiu a avaliação; os avisos genéricos sobre
ponteiros, ownership, validação e erros de arquivo foram confrontados com o
código e os testes. Esse resultado não substitui as verificações locais.

## Limites

- A simulação não valida comportamento do roboRIO nem temporização de rede.
- As capturas offscreen e a inicialização Wayland não validam atalhos globais,
  encaixe da janela, suspensão, permissões de dispositivos ou instalação em
  outras distribuições.
- Não há teste físico de joystick, rede, protocolo, FMS ou robô.
- Não há aprovação de paridade com o aplicativo NI 2026; as imagens da
  documentação são referências provisórias de organização.
- A instalação via CMake distribui somente a aplicação e seus arquivos desktop;
  não empacota as bibliotecas do Qt.
