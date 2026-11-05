# Validação da correção de paleta — 0.3.1

Data: 2026-11-05. A versão-alvo **NI 26.0** foi confirmada pelo dono.
As [referências inspecionadas](decisoes/0004-paleta-ni26.md) mostram a versão na
barra de título; a correção não foi baseada apenas nas imagens antigas da 24.0b96.

## Verificações

- Build Debug concluído no Arch Linux com Qt 6.12.0 e GCC 16.2.1.
- As duas suítes CTest passaram: **68 cenários**, 72 entradas com fixtures,
  zero falhas e zero testes pulados.
- A regressão existente de mouse/Enter/Space/Reset passou após Enable/Disable
  passarem a refletir visualmente o estado da sessão.
- Formatação e `git diff --check` passaram.
- Gate 5 do ASM aprovado com **81/70**. Foram fornecidos os métodos alterados
  completos, o header, SVG, CMake, regressão de comandos e resultado dos testes;
  os 21.120 caracteres submetidos foram analisados integralmente. Houve avisos
  genéricos de revisão; ownership Qt e comportamento dos controles foram conferidos.

As capturas [Operation](images/ni26-operation.png),
[Setup](images/ni26-setup.png), [USB](images/ni26-usb.png),
[Diagnostics](images/ni26-diagnostics.png), [CAN/Power](images/ni26-power.png),
[Charts](images/ni26-charts.png), [menu](images/ni26-menu.png),
[Countdown](images/ni26-practice-countdown.png), [A-Stop](images/ni26-practice-astop.png)
e [Teleop](images/ni26-practice-teleop.png) foram geradas da aplicação Qt.
Operation, menu e estados da demonstração foram inspecionados após a mudança.

O ciano explícito no código de aparência ficou restrito ao ícone e marcador
das abas ativas. Foco, botões comuns, checkbox, listas, indicador da demonstração
e ícone próprio usam tons neutros. Enable/Disable têm campos separados e o
rótulo `TeleOperated` acompanha a captura de Operation.

## Limites

Não se declara igualdade pixel a pixel. As fotos Windows de 26.0 confirmam
Diagnostics; a captura de Operation usa Wine e tem diferenças de fonte.
Faltam referências de todas as abas dessa versão para uma comparação completa.
Esta etapa não implementa transporte, leitura física de controles ou FMS.
