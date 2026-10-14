# Resultados da coleta inicial de 30 blocos

A coleta concluiu 240 lotes válidos, com 30 observações por combinação. Nenhuma amostra foi excluída. Os pilotos permanecem separados.

Os tempos abaixo são médias por operação de cada lote. O resumo usa a mediana dessas 30 médias. Os intervalos de confiança são aproximações por bootstrap percentil com 10.000 reamostragens; não demonstram independência temporal por si só.

| Mecanismo | Operação | Mediana (µs) | IC 95% da mediana (µs) | CV (%) |
|---|---|---:|---:|---:|
| ML-KEM-512 | Ciclo operacional | 68.179 | 68.030–68.382 | 0.88 |
| ML-KEM-512 | Geração de chaves | 32.680 | 32.599–32.811 | 2.08 |
| ML-KEM-512 | Recepção | 41.287 | 40.954–41.440 | 1.02 |
| ML-KEM-512 | Envio | 26.972 | 26.793–27.075 | 1.84 |
| RSA-3072-OAEP-SHA256 | Ciclo operacional | 1561.944 | 1554.506–1569.581 | 1.59 |
| RSA-3072-OAEP-SHA256 | Geração de chaves | 103538.500 | 91991.600–126918.850 | 57.82 |
| RSA-3072-OAEP-SHA256 | Recepção | 1532.744 | 1510.734–1547.303 | 2.66 |
| RSA-3072-OAEP-SHA256 | Envio | 33.932 | 33.781–34.069 | 2.06 |

## Comparação operacional

| Operação | Razão RSA/ML-KEM das medianas | IC 95% pareado |
|---|---:|---:|
| Geração de chaves | 3168.21 | 2822.65–3887.48 |
| Envio | 1.26 | 1.25–1.27 |
| Recepção | 37.12 | 36.52–37.61 |
| Ciclo operacional | 22.91 | 22.77–23.04 |

A razão compara as medianas dos tempos; valores maiores que 1 indicam maior tempo no caminho RSA neste ambiente. Não representa equivalência de segurança. O intervalo utiliza reamostragem dos blocos preservando o pareamento dos mecanismos.

## Tamanhos observados

| Mecanismo | Artefato | Representação | Intervalo (bytes) |
|---|---|---|---:|
| ML-KEM-512 | ciphertext | binary | 768 |
| ML-KEM-512 | private_key | FIPS203-dk | 1632 |
| ML-KEM-512 | private_key | PKCS8-DER-default-provider | 1730 |
| ML-KEM-512 | public_key | FIPS203-ek | 800 |
| ML-KEM-512 | public_key | SubjectPublicKeyInfo-DER | 822 |
| ML-KEM-512 | secret | binary | 32 |
| RSA-3072 | ciphertext | binary | 384 |
| RSA-3072 | private_key | PKCS8-DER-default-provider | 1792–1795 |
| RSA-3072 | public_key | SubjectPublicKeyInfo-DER | 422 |
| RSA-3072 | secret | binary | 32 |

## Avaliação da amostra e limites

As operações de transporte apresentaram CV de aproximadamente 0,9% a 2,7%. Os intervalos das medianas foram estreitos, com semilargura relativa entre aproximadamente 0,3% e 1,2%. Esta coleta fornece evidência para descrever as diferenças de transporte nas condições registradas.

A geração RSA apresentou CV de aproximadamente 58%, mediana de 103,539 ms e intervalo bootstrap aproximado de 91,992 a 126,919 ms. O intervalo é consideravelmente mais amplo que os das operações de transporte. A comparação com ML-KEM permanece claramente separada nesta amostra, mas a estimativa detalhada da geração RSA deve manter essa incerteza explícita.

Os coeficientes de autocorrelação de defasagem 1 ficaram em magnitude abaixo de 0,2. Isso é um diagnóstico descritivo, sem provar independência temporal. As medianas da geração RSA nas metades da execução também diferiram; com 30 observações e alta dispersão, não é possível atribuir isso automaticamente a uma tendência térmica ou excluir a influência do ambiente.

Não é obrigatório repetir a coleta apenas porque a geração RSA tem alta dispersão: ela pode ser relatada com os intervalos e limitações observados. Uma amostra maior será necessária se o objetivo passar a exigir estimativa mais precisa dessa operação ou caracterização forte de caudas. Essa decisão não deve depender de obter um resultado favorável.

**P95 e P99 em `estatisticas.csv` são percentis de médias de lote, não da latência individual.** Com 30 lotes, a descrição dos extremos é limitada. Não serão apresentados como garantia de latência de uma implantação.

Os resultados são restritos ao Ryzen 5 5600, Windows 11, OpenSSL 3.5.5, MSVC e à definição da região cronometrada. Contextos são reutilizados; geração prévia das entradas RSA, serialização e validação ficam fora do relógio. O custo de comunicação e as diferenças conceituais e de segurança continuam separados da avaliação de tempo.

Os snapshots de processos podem ter alcance limitado pelas permissões do terminal. A confirmação de computador livre foi registrada, mas a ausência de um processo nesses snapshots não comprova que ele estava fechado. Os resultados também representam a condição de execução do processo de benchmark, incluindo eventuais restrições do terminal.

Estes resultados fundamentam as tabelas e a discussão do TCC.
