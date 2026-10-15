# Coleta do benchmark — análise exploratória

Foram conferidos 2400 lotes válidos: 300 blocos por combinação. Nenhuma amostra foi excluída.

A unidade estatística é a média por operação de um lote. As iterações do lote não são tratadas como observações independentes.

| Mecanismo | Operação | Iterações/lote | Mediana (µs/operação) | CV (%) | Duração dos lotes (ms) |
|---|---|---:|---:|---:|---:|
| ML-KEM-512 | Ciclo | 512 | 67.030 | 2.87 | 33.96–45.47 |
| ML-KEM-512 | Geração | 1024 | 31.895 | 2.41 | 31.35–39.07 |
| ML-KEM-512 | Recepção | 512 | 40.427 | 4.52 | 20.50–34.94 |
| ML-KEM-512 | Envio | 1024 | 26.451 | 2.88 | 26.79–33.21 |
| RSA-3072-OAEP-SHA256 | Ciclo | 16 | 1530.931 | 2.18 | 23.79–28.98 |
| RSA-3072-OAEP-SHA256 | Geração | 1 | 116540.750 | 61.61 | 14.83–441.55 |
| RSA-3072-OAEP-SHA256 | Recepção | 16 | 1496.947 | 3.16 | 23.25–28.93 |
| RSA-3072-OAEP-SHA256 | Envio | 1024 | 33.157 | 1.97 | 33.57–39.53 |

## Relógio

O contador tem frequência de 10,000,000 Hz e incremento nominal de 100.0 ns. Nas 2.000 diferenças adjacentes, a média foi 25.2 ns e a mediana 0.0 ns. A mediana zero decorre da quantização possível; não demonstra custo nulo.

Esse custo é registrado separadamente. Não foi subtraído dos lotes.

## Limites da leitura

- Configuracao limitada a este ambiente.
- Independência temporal e caudas precisam ser avaliadas.
- Snapshots de processos podem ser limitados pelas permissoes do terminal; ausencia nao comprova que um aplicativo estava fechado.
- Diferencas entre leituras adjacentes do QPC nao demonstram precisao universal; mediana zero nao significa custo zero.

Percentis e intervalos bootstrap estão em `analysis.json`; no piloto são apenas exploratórios. Não serão apresentados como caracterização definitiva com sete blocos.

Os dados brutos de aquecimento e calibração continuam preservados. Nenhuma linha da fase `sample` foi removida. A ordem, os formatos das chaves e os hashes estão nos arquivos da execução.
