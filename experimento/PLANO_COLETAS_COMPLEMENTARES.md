# Protocolo prospectivo de coletas complementares

Status: três sessões principais concluídas e conferidas. Sensibilidade de lotes não executada; transferida para continuidade do estudo em 15/10/2026. A coleta original permanece separada.

## Perguntas e desenho

1. A direção e a magnitude das diferenças observadas persistem entre sessões do mesmo computador?
2. Qual a precisão da média e da mediana da geração RSA em uma amostra maior?
3. Os resultados de transporte se mantêm com uma alteração controlada do tamanho dos lotes?

Etapa principal: três sessões, com 300 blocos completos em cada sessão, iniciadas após reinício realizado pelo autor e recuperação das condições de repouso. Usar o mesmo executável, biblioteca, CPU lógica, lotes e procedimento da coleta original. Sementes de ordem distintas, fixadas em 20261021, 20261022 e 20261023. Isso produz 900 gerações RSA cronometradas, além das demais combinações. Não confundir o reinício do processo entre sessões com independência física comprovada.

Não executar as três sessões consecutivamente como se fossem sessões independentes. Registrar data, reinício, condições de fundo e intervalo desde a sessão anterior. Manter a coleta original identificada como exploratória; não incorporá-la automaticamente às novas estimativas.

## Condições e registros

- O autor confirma que o computador pode ficar livre antes de cada sessão.
- Registrar plano de energia, afinidade, versão e hashes do executável e da biblioteca, início e fim da execução e tarefas relevantes de fundo.
- Registrar frequência efetiva e temperatura com ferramenta apropriada ao Ryzen. Validar a origem, a unidade e o intervalo de amostragem antes da coleta; valores nominais de WMI não substituem frequência efetiva nem temperatura do processador. Ferramenta validada funcionalmente em 14/10/2026: LibreHardwareMonitor 0.9.6 com PawnIO 2.2.0, coleta administrativa no .NET Framework, intervalo nominal de 1 s. Relatórios em experimento/validacoes/telemetria/VALIDACAO_FINAL.md e VALIDACAO_FINAL.json. Essa validação não certifica calibração metrológica.
- Aplicar o mesmo procedimento de telemetria nas três sessões; seu próprio custo integra as condições observadas. Documentar sua versão e configuração.
- Manter as proteções do sistema. Não desabilitar antivírus como parte deste protocolo.
- Não apagar execuções malsucedidas, não remover valores altos e não substituir sessões por apresentarem resultados desfavoráveis. Registrar falhas e a razão de qualquer repetição.

## Plano de análise fixado antes dos novos dados

Apresentar, por sessão e combinação, média, mediana, DP e IC de 95%. Para RSA keygen, meta descritiva de semilargura relativa de até 5% para a mediana; se não for atingida com o tamanho fixado, reportar o resultado e a precisão alcançada, sem prolongar a coleta até obter um resultado desejado. A meta não foi estabelecida para a coleta antiga e não deve ser atribuída retroativamente a ela.

Calcular razões pareadas dentro de cada sessão. Apresentar a variação entre as três estimativas de sessão separadamente; três sessões não sustentam uma caracterização abrangente da distribuição entre sessões. Não reamostrar todos os blocos como se fossem uma única população homogênea. Intervalos dentro de sessão permanecem condicionados às hipóteses de reamostragem.

Análise de sensibilidade separada: na terceira sessão, após a etapa principal e um intervalo documentado, executar dois processos adicionais de 30 blocos: um com lotes de transporte reduzidos à metade e outro com lotes dobrados. Fixar suas sementes em 20261024 e 20261025. Manter keygen RSA em uma operação/lote e ML-KEM em 1.024; interpretar a sensibilidade somente para transporte. Não incorporar esses lotes às estimativas principais. A ordem dos dois processos é fixa e sua comparação não isola todos os efeitos de ordem.

## Execução preparada

O script executar_sessao_complementar.ps1 inicia somente uma sessão por chamada e exige confirmação explícita de computador livre e caminho para o CSV de telemetria. O script não reinicia o Windows, não instala ferramentas e não executa a análise automaticamente.

O cenário com geração do segredo RSA dentro do relógio e os contextos criados por operação exigem uma variante instrumentada e validada do benchmark. Permanecem uma extensão distinta: seus resultados não podem ser simulados somando estimativas de custos não medidos.

## Estado após validar a ferramenta

A ferramenta foi validada em registro de 65 atualizações, com 63 úteis sem lacunas nos sensores obrigatórios. Antes da próxima coleta, o registro contínuo deve estar iniciado e cobrir integralmente o início e o fim do benchmark; um CSV antigo de validação não atende a esse requisito. Usar as mesmas configurações de telemetria nas sessões complementares. As três sessões criptográficas principais foram concluídas e conferidas; análise de sensibilidade pendente.

## Conferência da sessão 1

300 blocos, 2.400 lotes válidos e nenhuma exclusão. Registro térmico cobre a sessão. Meta de semilargura relativa de 5% para a mediana da geração RSA não atingida: observado 8,07%. Preservado o tamanho fixado de 300 blocos. Relatório: experimento/execucoes/collect-20261015T005644Z-bb3109c8/VERIFICACAO_FINAL.md. Os dados não foram agregados à coleta original.

## Conferência da sessão 2

Execução collect-20261015T160749Z-3ad62252: 300 blocos, 2.400 lotes válidos, sem exclusões. Autor declarou reinício sem os cinco minutos de repouso; desvio preservado em arquivo separado e no relatório. Um par de leituras de sensores compartilha o mesmo carimbo de segundo, sem exclusão de linhas. Meta de 5% para mediana da geração RSA não atingida: 11,43%. Sessão 3 pendente. Nenhuma agregação ou publicação dos novos dados realizada.

## Conferência da sessão 3

300 blocos e 2.400 lotes válidos. Autor declarou repouso de cinco minutos. Mediana geração RSA 116,541 ms; semilargura relativa 9,34%, meta de 5% não atingida. Razão das medianas do ciclo 22,839. Um par de leituras distintas com carimbo de segundo repetido foi preservado. Relatórios na pasta da execução collect-20261015T164240Z-0005e229 e em experimento/COMPARACAO_SESSOES_COMPLEMENTARES.md. Três sessões principais encerradas; sensibilidade de lotes ainda pendente.

## Decisão de encerramento das correções — 15/10/2026

As três sessões principais foram incorporadas ao manuscrito com resultados separados. A sensibilidade, originalmente prevista após a terceira sessão, não foi executada e foi transferida explicitamente para continuidade do estudo. Isso é uma alteração de encaminhamento posterior às coletas, não cumprimento daquela etapa nem resultado negativo do teste. Não foram iniciadas novas execuções para perseguir a meta de 5%.
