# DriveStation Linux

Reimplementação independente para Linux da **NI FRC Driver Station**, com foco
em fidelidade visual e funcional à experiência usada com **roboRIO**.

**Estado atual: preparação do projeto.** Este repositório contém o escopo e o
plano de desenvolvimento. Ainda não há aplicação executável, comunicação com
robô implementada ou compatibilidade validada em hardware.

## Objetivo

Permitir que equipes FRC usem Linux mantendo uma experiência próxima da Driver
Station da NI: disposição dos controles, indicadores, modos de operação,
joysticks, diagnóstico e configuração.

A fidelidade será avaliada em duas frentes: comparação da interface com a
referência e testes do comportamento com simuladores e, posteriormente, com
roboRIO. Cada recurso terá seu estado registrado na
[matriz de fidelidade](docs/fidelidade.md).

## Referência do projeto

A referência inicial é a **NI FRC Driver Station da temporada 2026 para
roboRIO**. A versão exata do aplicativo usada nas comparações ainda será
registrada. Não há código-fonte da NI incorporado neste repositório.

A [FIRST Driver Station anunciada para 2027][nova-ds] já oferece suporte a Linux,
mas foi projetada para Systemcore e não é compatível com roboRIO. Este projeto
tem como alvo a experiência anterior da NI e o ecossistema roboRIO.

## Desenvolvimento

O primeiro marco será uma interface desktop navegável, inicialmente sem enviar
comandos ao robô. O núcleo de estados, a leitura de controles e a comunicação
serão desenvolvidos e validados em etapas próprias.

- [Matriz de fidelidade](docs/fidelidade.md)
- [Plano de desenvolvimento](docs/roadmap.md)
- [Referências técnicas](docs/referencias.md)

A stack, as distribuições Linux suportadas e os formatos de distribuição ainda
serão definidos. Por enquanto, não há instruções de instalação ou execução.

## Identidade e escopo

Projeto comunitário independente, sem vínculo oficial com NI, FIRST ou WPILib.
Uso em eventos e integração com FMS não fazem parte da validação inicial.
Compatibilidade e resultados de testes serão documentados conforme forem
comprovados.

[nova-ds]: https://wpilib.org/blog/the-2027-first-driver-station
