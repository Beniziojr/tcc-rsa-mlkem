# Coleta do benchmark — análise exploratória

Foram conferidos 240 lotes válidos: 30 blocos por combinação. Nenhuma amostra foi excluída.

A unidade estatística é a média por operação de um lote. As iterações do lote não são tratadas como observações independentes.

| Mecanismo | Operação | Iterações/lote | Mediana (µs/operação) | CV (%) | Duração dos lotes (ms) |
|---|---|---:|---:|---:|---:|
| ML-KEM-512 | Ciclo | 512 | 68.179 | 0.88 | 34.63–35.73 |
| ML-KEM-512 | Geração | 1024 | 32.680 | 2.08 | 33.09–36.84 |
| ML-KEM-512 | Recepção | 512 | 41.287 | 1.02 | 20.75–21.48 |
| ML-KEM-512 | Envio | 1024 | 26.972 | 1.84 | 27.03–29.88 |
| RSA-3072-OAEP-SHA256 | Ciclo | 16 | 1561.944 | 1.59 | 24.60–26.53 |
| RSA-3072-OAEP-SHA256 | Geração | 1 | 103538.500 | 57.82 | 42.40–368.85 |
| RSA-3072-OAEP-SHA256 | Recepção | 16 | 1532.744 | 2.66 | 23.68–26.42 |
| RSA-3072-OAEP-SHA256 | Envio | 1024 | 33.932 | 2.06 | 34.07–38.14 |

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
