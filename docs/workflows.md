# Workflows do GitHub

Os workflows ficam em `.github/workflows` e usam ações oficiais fixadas por
commit. Não exigem credenciais externas.

| Workflow | Quando executa | Resultado |
| --- | --- | --- |
| Build and tests | Push na main, pull request ou execução manual | Build Debug e Release no Ubuntu 24.04, testes offscreen, captura da interface e instalação isolada |
| CodeQL C++ | Push na main, pull request, segunda-feira ou execução manual | Análise estática C++ publicada em Code scanning |
| Linux alpha test package | Execução manual em Actions | Arquivo alfa TESTES.tar.gz, launcher, aviso de testes, metadados e SHA-256; verifica os binários extraídos |

Os relatórios e as capturas do CI ficam nos artefatos da execução por 14 dias;
o pacote fica disponível por 30 dias. Uma execução nova da mesma branch cancela
o CI e a análise anteriores ainda em andamento.

Para gerar um pacote no CI, abra **Actions → Linux alpha test package → Run workflow**
e escolha a branch. O workflow testa, usa CPack e verifica o próprio arquivo extraído
com `packaging/verify-alpha.py`. Nenhuma release é publicada automaticamente.
O CI compila no Ubuntu 24.04 x86_64; a alfa local de 2026-11-09 foi compilada no
Arch Linux. Não houve execução remota do workflow para essa entrega local.
Cada pacote identifica Qt e libc do build em `build-info.json` e depende das
bibliotecas/plugins Qt 6 e SDL2 do sistema; ele não é um AppImage.

Os testes de CI cobrem a demonstração local, erros de entrada, dispositivos
virtuais SDL, UDP/TCP e relógio em loopback sem acesso administrativo. O pacote
inclui `drivestation-network-check` além da interface gráfica. Uma execução aprovada
não comprova compatibilidade física com roboRIO ou autorização de uso em FMS.

Primeiras execuções verificadas no commit `cff5691`:
[Debug/Release](https://github.com/GuilhermeAlencarFRC/drivestation-linux/actions/runs/42089129171),
[CodeQL](https://github.com/GuilhermeAlencarFRC/drivestation-linux/actions/runs/42089129067) e
[pacote Linux](https://github.com/GuilhermeAlencarFRC/drivestation-linux/actions/runs/42089120513)
concluíram com sucesso.
