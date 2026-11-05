# 0004 — Fixar NI 26.0 como referência e retirar acentos extras

Data: 2026-11-05. O dono confirmou explicitamente a versão **NI 26.0** após
rejeitar a paleta da prévia 0.3.0.

## Pesquisa visual

Foram abertas e inspecionadas três capturas com o título `Version 26.0`:

- [Windows, Diagnostics, publicada em 12/02/2026](https://www.chiefdelphi.com/t/driver-station-roborio-connection-issue/514227).
- [Windows, Diagnostics, publicada em 19/02/2026](https://www.chiefdelphi.com/t/we-have-a-bunch-of-roborios-and-we-are-not-able-to-reimage-any-of-them-or-deploy-code/514651).
- [Operation via Wine/macOS, publicada em 08/02/2026](https://www.chiefdelphi.com/t/frc-driver-station-on-macos/513948).

São registros do aplicativo feitos pelos próprios autores das publicações.
As duas fotos de Windows confirmam a versão e a seleção ciano nos ícones e
marcadores das abas. A captura de Operation também confirma esse detalhe, mas
seus problemas de fonte/Wine impedem usá-la como padrão de tipografia do Windows.
Esses arquivos de terceiros não são redistribuídos no repositório.

## Correção

O port tinha espalhado ciano para elementos sem correspondência demonstrada:
foco de botões, botões comuns selecionados, checkbox, indicador de simulação e
ícone próprio do aplicativo. Essa aplicação de cor foi retirada na prévia 0.3.1.

- Navegação ativa: ciano restrito ao ícone e marcador lateral, como nas capturas.
- Foco, checkboxes, listas e botões comuns: escala de cinza.
- Indicador de dados simulados: cinza, preservando identificação explícita.
- Ícone próprio do port: neutro, sem apresentar a marca da NI.
- Fundo geral mais escuro; fechar com campo vermelho escuro.
- Enable e Disable: campos separados e estado atual com fundo escuro.
- Cores de estado e curvas continuam semânticas: verde, vermelho e laranja.

Não foi adotada uma identidade azul/ciano para a aplicação. A referência é a
distribuição dos acentos observada na NI 26.0. Setup, USB e CAN/Power ainda
precisam de capturas dessa versão para uma comparação completa; não se presume
que cada detalhe das imagens 24.0b96 seja válido em 26.0.
