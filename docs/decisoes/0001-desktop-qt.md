# 0001 — Interface desktop com Qt Widgets e C++

Data: 2026-11-05. Estado: aceita para o primeiro marco.

Evolução posterior: o item Practice inicialmente pendente foi implementado na
demonstração local conforme a [decisão 0002](0002-practice-local.md). Os critérios
abaixo registram o escopo da primeira prévia.

## Objetivo desta entrega

Entregar uma aplicação Linux executável, compacta e navegável, que permita
avaliar a organização visual da NI Driver Station. Incluir as cinco abas
principais, o painel central persistente e as três opções de visualização à
direita. Preferências devem sobreviver à reinicialização.

Dados de demonstração são explicitamente simulados. O executável deste marco
não implementa sockets de comunicação com robôs, não descobre robôs e não envia
comandos ao roboRIO.

## Decisão

Usar C++17, Qt 6 Widgets, CMake e Qt Test, sem dependências obtidas durante o
build. O Qt oferece widgets desktop, pintura própria para indicadores compactos
e testes de eventos de teclado e mouse. O ambiente disponível possui Qt 6.12;
o projeto declara Qt 6.4 como mínimo, ainda sem validação nessa versão mínima.

Separar a sessão local de demonstração, preferências, widgets e janela. Não
tratar a sessão demonstrativa como implementação do protocolo de operação.
O transporte e os dispositivos reais terão módulos e validação próprios.

Qt Widgets foi escolhido pela integração desktop e pelo controle do layout
compacto. Uma interface web exigiria outra camada para dispositivos e captura
de teclado; QML acrescentaria uma tecnologia sem necessidade neste marco.

## Referência visual e limites

As imagens `ds-operation-tab.png`, `ds-setup-tab.png` e `ds-usb-tab.png` da
documentação WPILib instalada com 2026.2.1 mostram a versão **24.0b96** da NI.
Outras imagens, como Diagnostics e Charts, são mais antigas. Essas imagens
orientam a organização inicial; não aprovam paridade visual com a NI 2026.
A versão exata do aplicativo 2026 e suas diferenças ainda precisam de captura.

Preservar o cinza escuro, a seleção ciano, a navegação lateral, os rótulos em
inglês e a posição das áreas principais. Desenhar ícones próprios; não incorporar
binários, marcas ou imagens da NI ao produto.

## Critérios de aceitação

- Build local com instruções reproduzíveis e instalação via CMake.
- Cinco abas à esquerda e Messages/Charts/Both à direita navegáveis.
- Equipe e preferências persistidas em arquivo de configuração do usuário.
- Inicialização desconectada e desabilitada; simulação ativada explicitamente.
- Enable local apenas com simulação conectada; mudança de modo desabilita.
- Enter desabilita; Space trava E-Stop local; reset é explícito e desconecta.
- Atalhos valem dentro da aplicação; captura fora de foco não está implementada.
- Practice pode ser selecionado, mas sua sequência e habilitação ficam pendentes.
- Nenhum estado de habilitação, conexão ou E-Stop é persistido.
- Testes das transições locais, persistência e interação da interface.
- Capturas da aplicação renderizada em Qt offscreen, com limitações registradas.

Referências: [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html),
[CMake com Qt](https://doc.qt.io/qt-6/cmake-get-started.html),
[Qt Test](https://doc.qt.io/qt-6/qtest-overview.html) e
[NI Driver Station](https://docs.wpilib.org/en/stable/docs/software/driverstation/driver-station.html).
