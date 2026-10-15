# Coleta do benchmark — análise exploratória

Foram conferidos 2400 lotes válidos: 300 blocos por combinação. Nenhuma amostra foi excluída.

A unidade estatística é a média por operação de um lote. As iterações do lote não são tratadas como observações independentes.

| Mecanismo | Operação | Iterações/lote | Mediana (µs/operação) | CV (%) | Duração dos lotes (ms) |
|---|---|---:|---:|---:|---:|
| ML-KEM-512 | Ciclo | 512 | 66.867 | 3.76 | 33.93–43.28 |
| ML-KEM-512 | Geração | 1024 | 31.819 | 4.69 | 31.36–44.89 |
| ML-KEM-512 | Recepção | 512 | 40.388 | 3.74 | 20.52–26.54 |
| ML-KEM-512 | Envio | 1024 | 26.343 | 4.28 | 26.73–36.56 |
| RSA-3072-OAEP-SHA256 | Ciclo | 16 | 1532.044 | 3.42 | 23.79–30.14 |
| RSA-3072-OAEP-SHA256 | Geração | 1 | 114632.200 | 63.52 | 16.09–685.72 |
| RSA-3072-OAEP-SHA256 | Recepção | 16 | 1496.975 | 3.46 | 23.27–30.96 |
| RSA-3072-OAEP-SHA256 | Envio | 1024 | 33.113 | 4.34 | 33.63–47.23 |

## Relógio

O contador tem frequência de 10,000,000 Hz e incremento nominal de 100.0 ns. Nas 2.000 diferenças adjacentes, a média foi 23.6 ns e a mediana 0.0 ns. A mediana zero decorre da quantização possível; não demonstra custo nulo.

Esse custo é registrado separadamente. Não foi subtraído dos lotes.

## Limites da leitura

- Configuracao limitada a este ambiente.
- Independência temporal e caudas precisam ser avaliadas.
- Snapshots de processos podem ser limitados pelas permissoes do terminal; ausencia nao comprova que um aplicativo estava fechado.
- Diferencas entre leituras adjacentes do QPC nao demonstram precisao universal; mediana zero nao significa custo zero.

Percentis e intervalos bootstrap estão em `analysis.json`; no piloto são apenas exploratórios. Não serão apresentados como caracterização definitiva com sete blocos.

Os dados brutos de aquecimento e calibração continuam preservados. Nenhuma linha da fase `sample` foi removida. A ordem, os formatos das chaves e os hashes estão nos arquivos da execução.
