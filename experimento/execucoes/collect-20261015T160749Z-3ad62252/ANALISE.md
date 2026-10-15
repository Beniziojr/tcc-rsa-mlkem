# Coleta do benchmark — análise exploratória

Foram conferidos 2400 lotes válidos: 300 blocos por combinação. Nenhuma amostra foi excluída.

A unidade estatística é a média por operação de um lote. As iterações do lote não são tratadas como observações independentes.

| Mecanismo | Operação | Iterações/lote | Mediana (µs/operação) | CV (%) | Duração dos lotes (ms) |
|---|---|---:|---:|---:|---:|
| ML-KEM-512 | Ciclo | 512 | 66.886 | 3.69 | 33.91–48.87 |
| ML-KEM-512 | Geração | 1024 | 31.836 | 3.95 | 31.39–43.59 |
| ML-KEM-512 | Recepção | 512 | 40.370 | 3.82 | 20.48–30.37 |
| ML-KEM-512 | Envio | 1024 | 26.413 | 3.79 | 26.76–35.37 |
| RSA-3072-OAEP-SHA256 | Ciclo | 16 | 1525.391 | 7.07 | 23.77–51.77 |
| RSA-3072-OAEP-SHA256 | Geração | 1 | 110773.000 | 63.82 | 19.26–479.00 |
| RSA-3072-OAEP-SHA256 | Recepção | 16 | 1497.719 | 2.80 | 23.23–28.42 |
| RSA-3072-OAEP-SHA256 | Envio | 1024 | 33.126 | 3.91 | 33.58–54.24 |

## Relógio

O contador tem frequência de 10,000,000 Hz e incremento nominal de 100.0 ns. Nas 2.000 diferenças adjacentes, a média foi 26.5 ns e a mediana 0.0 ns. A mediana zero decorre da quantização possível; não demonstra custo nulo.

Esse custo é registrado separadamente. Não foi subtraído dos lotes.

## Limites da leitura

- Configuracao limitada a este ambiente.
- Independência temporal e caudas precisam ser avaliadas.
- Snapshots de processos podem ser limitados pelas permissoes do terminal; ausencia nao comprova que um aplicativo estava fechado.
- Diferencas entre leituras adjacentes do QPC nao demonstram precisao universal; mediana zero nao significa custo zero.

Percentis e intervalos bootstrap estão em `analysis.json`; no piloto são apenas exploratórios. Não serão apresentados como caracterização definitiva com sete blocos.

Os dados brutos de aquecimento e calibração continuam preservados. Nenhuma linha da fase `sample` foi removida. A ordem, os formatos das chaves e os hashes estão nos arquivos da execução.
