# Plano de desenvolvimento

## Situação em 2026-11-05

A primeira prévia desktop está implementada no commit `abb1f18`: C++17, Qt
Widgets, cinco abas principais, três visualizações à direita, preferências e
sessão local de demonstração. Os 11 cenários de teste passaram; instalação
isolada, renderização offscreen e inicialização Wayland foram verificadas.

A evolução 0.2.0 acrescenta o motor determinístico de Practice, A-Stop,
cancelamento em todas as fases, tempo restante e aviso de habilitação futura.
As duas suítes locais somam 67 cenários aprovados, contando as linhas de dados
parametrizadas. Veja a [validação de Practice](validacao-practice.md).

A prévia 0.3.0 aproxima o painel da organização da NI: 1024 × 200, modos
retangulares, ícones cinza/ciano, console com engrenagem e controles locais
no menu. Veja a [decisão visual](decisoes/0003-fidelidade-visual.md).

A versão-alvo foi confirmada como NI 26.0; a prévia 0.3.1 restringe o acento de
cor aos elementos observados nas capturas dessa versão. A
[pesquisa visual](decisoes/0004-paleta-ni26.md) documenta essa correção.

O marco 1 ainda exige capturas de todas as abas da NI 26.0. O marco 2 tem uma entrega
executável, com comparação visual final pendente. O motor de Practice avança
o marco 3, mas a demonstração não valida controle de um robô nem suspensão
física. A prévia 0.4.0 implementa entrada por SDL2 com descoberta, seis slots
estáveis, eixos, botões, POV e rumble. O marco 4 tem testes com dispositivos
virtuais, mas ainda requer validação em modelos físicos, ordem persistente e
mapeamento equivalente ao da NI. O transporte do marco 5 continua pendente.
Veja a [validação de USB](validacao-usb.md).

## 1. Fixar a referência

- Registrar a versão exata da NI Driver Station 2026 usada como referência.
- Catalogar as telas, dimensões, escalas, estados e interações a reproduzir.
- Conferir fontes técnicas e licenças antes de reutilizar dependências ou código.
- Definir o primeiro ambiente Linux suportado a partir de um ambiente verificável.

**Conclusão do marco:** referência identificada e critérios de comparação
registrados na matriz de fidelidade.

## 2. Escolher a stack e montar a interface

- Avaliar uma stack desktop capaz de oferecer controle preciso do layout,
  integração com dispositivos e distribuição no Linux.
- Manter interface, estado de operação, entradas e transporte em módulos separados.
- Reproduzir a estrutura visual e as telas com dados demonstrativos identificados.
- Documentar a decisão de stack antes de consolidar a implementação.

**Conclusão do marco:** aplicação navegável, instruções reproduzíveis de execução
e capturas comparativas. Nenhum comando é enviado a robôs neste marco.

## 3. Implementar o núcleo de operação

- Modelar os modos e as transições com estados explícitos.
- Iniciar desabilitado e impedir habilitação implícita após reconexão ou suspensão.
- Dar prioridade a Disable e à parada de emergência.
- Implementar a sequência de prática com relógio monotônico e testes de tempo.
- Validar perda de comunicação, encerramento e mudanças de configuração.

**Conclusão do marco:** testes automatizados dos estados, transições e falhas,
sem depender da aparência dos botões para determinar o estado do robô.

## 4. Integrar os dispositivos Linux

- Ler controles físicos, preservar a ordem dos slots e tratar conexão e remoção.
- Validar eixos, botões e POVs com dispositivos identificados.
- Projetar a captura de teclado e a parada fora de foco para o ambiente suportado.
- Registrar diferenças entre X11, Wayland, permissões e suspensão do sistema.

**Conclusão do marco:** entradas verificadas em hardware e limitações documentadas.

## 5. Implementar comunicação com roboRIO

- Confirmar formato, sequência, temporização e limites dos pacotes em fontes
  identificadas; não tratar o protocolo do Systemcore como equivalente.
- Começar com testes de codificação, decodificação e um par de teste local.
- Integrar descoberta, envio de entradas e recepção de estado.
- Testar perda, atraso, duplicação e corrupção de pacotes antes da integração física.
- Validar com roboRIO e programa de teste em ambiente controlado.

**Conclusão do marco:** evidência reproduzível de comunicação e comportamento;
resultados com simuladores e hardware registrados separadamente.

## 6. Fechar a paridade e distribuir

- Implementar os itens restantes de diagnóstico, console, gráficos e dashboards.
- Comparar os fluxos completos com a referência e registrar diferenças.
- Escolher formatos de pacote e verificar instalação em ambiente limpo.
- Publicar versões apenas com o escopo efetivamente testado.

**Conclusão do marco:** versão instalável com matriz de fidelidade atualizada,
ambientes suportados e limitações conhecidas.
