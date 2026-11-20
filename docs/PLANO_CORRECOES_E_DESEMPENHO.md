# Correcoes da auditoria e desempenho do slot 2

Base: main cc7ac2a, 20/11/2026. Branch fix/audit-and-slot2-performance.

## Fatos e fontes oficiais (consulta em 20/11/2026)

- O ngspice compartilha vetores pela API ngGet_Vec_Info e executa comandos por ngSpice_Command: https://github.com/ngspice/ngspice/blob/master/src/include/ngspice/sharedspice.h
- XSPICE ja executa funcoes digitais por eventos, coordenadas com o solver analogico: https://ngspice.sourceforge.io/xspice.html
- O modelo atual ja usa esse host isolado e XSPICE. O slot 2 possui 235 pecas/333 fios internos, embora os blocos externos sejam poucos.
- Medicao inicial sem interface: 20 lotes de 20 ms simulados, media de 51,55 ms reais por lote (0,388x), sem pecas queimadas. Compilacao estrita de desenvolvimento. Perfil temporario: build/audit-fixes/slot2-before.log.
- protect_sources reconstroi conectividade em cada lote; leituras de vetores fazem uma ida e volta ao processo por valor. Passo eletrico: 1 ms; clock da montagem: 10 Hz.

## Comparacao e recomendacao

| Opcao | Beneficio | Custo/risco |
|---|---|---|
| Reutilizar a conectividade enquanto a revisao nao muda | Elimina calculo repetido, sem mudar equacoes | Invalidar ao editar conexoes/controles e recarregar sessao |
| Ler vetores em lotes no protocolo existente | Reduz idas e voltas, mantem medidas completas | Limites de mensagem e erros devem continuar validados |
| Trocar por outro backend digital | Pode beneficiar circuitos exclusivamente digitais | Novo modelo e validacao extensa; fora deste incremento |

Decisao: medir e otimizar os dois primeiros pontos conforme o perfil. Preservar medidas, deteccao de curto, estado sequencial e tempos eletricos. Nao reduzir precisao para melhorar o numero de velocidade.
Hipotese: esses custos de orquestracao explicam parte importante da lentidao; validar por medicao antes/depois, sem prometer um multiplicador antes dos testes.

## Incrementos verificaveis

1. Regressoes para os oito erros da auditoria: aritmetica, numeros, tipos/chamadas, troca de projeto, runtime, protoboard larga e persistencia da sonda.
2. Correcoes pequenas nos modulos existentes, com casos opostos e testes de salvar/reabrir.
3. Otimizacao guiada pelo perfil e comparacao das leituras/estado do mesmo circuito.
4. Atualizar roadmap, registrar limites e executar a suite completa e testes graficos.

Nao alterar o circuito pessoal para mascara de desempenho. Backup anterior em saves/backup, fora do controle de versao.
