# Protocolo do benchmark versão 1

O programa `benchmark.c` implementa os mecanismos e as métricas previstos no TCC. Este protocolo foi utilizado nos pilotos e na coleta de 30 blocos. Os pilotos permanecem separados da coleta final; a configuração congelada está nos arquivos da execução.

## O que está dentro do cronômetro

| Operação | RSA-3072/OAEP-SHA-256 | ML-KEM-512 |
|---|---|---|
| Geração | `EVP_PKEY_generate` | `EVP_PKEY_generate` |
| Envio | `EVP_PKEY_encrypt` | `EVP_PKEY_encapsulate` |
| Recepção | `EVP_PKEY_decrypt` | `EVP_PKEY_decapsulate` |
| Ciclo | Cifração seguida de decifração | Encapsulamento seguido de decapsulação |

Os laços, a preparação dos comprimentos de saída e o armazenamento dos códigos de retorno integram o tempo do lote. A geração de chaves inclui a alocação que ocorre dentro da API. A inicialização e a configuração dos contextos EVP são feitas fora do cronômetro; os contextos são reutilizados. Os resultados representarão esse modo de uso, e não um handshake completo nem o custo de criar um contexto por operação.

A geração prévia dos segredos RSA de 32 bytes fica fora do cronômetro. O custo interno de aleatoriedade do OAEP e do ML-KEM integra suas operações. Essa diferença é parte da comparação funcional e parcial já delimitada no TCC; não será apresentada como identidade conceitual das operações.

## O que fica fora do cronômetro

Alocação dos buffers do programa, primeiro acesso de escrita às suas páginas, exportação/importação da chave pública, geração das entradas para as medições de recepção, validação, liberação dos recursos, impressão, gravação e serialização. A medição de envio é validada por recepção executada depois de parar o relógio. A geração é validada por um ciclo de transporte para cada chave gerada, também fora do relógio.

Cada operação do lote recebe buffers próprios. Todos os códigos de retorno, tamanhos e segredos recuperados são verificados. O lote só é registrado como válido se todas as operações passarem. Uma falha aborta a execução, preservando os registros anteriores e a indicação do erro; uma execução interrompida não será utilizada como coleta completa.

## Chaves e tamanhos

Há um par RSA e um par ML-KEM novos para os testes de transporte de cada bloco. Esses pares são reutilizados pelas operações de envio, recepção e ciclo naquele bloco. As chaves da medição de geração são novas em cada iteração e não são confundidas com os pares preparados para o transporte.

O envio utiliza um objeto que contém apenas a chave pública. A representação DER intermediária que permite construir esse objeto é preparada fora das medições.

`sizes.csv` distingue:

- Chaves públicas em SubjectPublicKeyInfo DER, para os dois mecanismos.
- Chaves privadas em PKCS#8 DER, usando o comportamento padrão do provider.
- Chaves ML-KEM nas representações binárias FIPS 203 `ek` e `dk`, obtidas pelos parâmetros `pub` e `priv` (800 e 1.632 bytes).
- Ciphertexts binários (384 e 768 bytes) e segredos (32 bytes).

Uma chave privada ML-KEM pode conservar uma semente e exportá-la no contêiner PKCS#8. Por isso, o tamanho desse contêiner não substitui a medida do parâmetro `dk`. As representações serão comparadas com identificação explícita de formato, sem confundir PEM, DER, semente ou tamanho do módulo RSA.

## Organização

- Provider `default`, biblioteca local OpenSSL 3.5.5. O programa confere a versão em execução e registra o caminho da DLL realmente carregada.
- Uma thread fixada em uma CPU lógica permitida. A opção `--cpu` permite escolher a CPU. A máscara anterior é restaurada no final, sem alterar a afinidade dos demais programas.
- Ordem das oito combinações de mecanismo e operação embaralhada em cada bloco, por xorshift32 com semente registrada. Essa semente controla apenas a ordem, não a aleatoriedade criptográfica.
- Um lote constitui uma observação estatística. As iterações dentro dele não serão contadas como observações independentes.
- Prioridade normal. Nos modos com medição, a suspensão é impedida temporariamente durante a execução. O programa não encerra aplicativos nem pausa sincronizações.

## Modos

`validate`: testa o caminho das oito combinações, salva resultados e tamanhos, mas deixa os campos de duração vazios. Não constitui piloto ou evidência de desempenho.

`pilot`: por padrão, sete blocos. Usa inicialmente três gerações RSA e 1.000 operações de aquecimento nas demais combinações. Calibra os lotes por duplicação até alcançar cerca de 20 ms por lote, ou o limite de 4.096 iterações. Esses são pontos iniciais, não critérios universais de estabilidade. O piloto precisa confirmar ou modificar os valores, inclusive o aquecimento RSA, antes da coleta.

`collect`: por padrão, trinta blocos. Exige lotes definidos explicitamente após o piloto. Não recalibra os lotes durante a coleta. Quantidades, aquecimento, semente, CPU e binários serão registrados e fixados.

## Relógio e arquivos

QueryPerformanceCounter mede os lotes e QueryPerformanceFrequency fornece a base de conversão. São guardados os ticks inteiros, a duração total e a duração por operação, sem arredondar ticks intermediários. O timestamp UTC é lido fora da região cronometrada.

Nos modos com medição, `clock.csv` registra duas mil diferenças entre leituras adjacentes do relógio. Isso ajuda a estimar seu custo e os incrementos observados; não comprova precisão universal e não será subtraído automaticamente dos tempos das operações.

Cada execução cria uma pasta nova. Não substitui resultados anteriores. `raw.csv` distingue aquecimento, calibração e amostras; `selected_batches.txt` contém os lotes efetivamente utilizados. `config.json` contém a configuração inicial; `run.json` registra argumentos, código de saída e hashes. `SUCCESS.txt` identifica conclusão com validação, e `FAILURE.txt` registra falhas ocorridas após criar a pasta.

As análises da coleta utilizarão somente a fase `sample` de execuções completas no modo `collect`. Calibração, desenvolvimento e aquecimento não serão misturados aos resultados finais.

## Sessão do piloto

Antes de medir: encerrar tarefas pesadas, pausar OneDrive/downloads, manter a política de energia e informar que o computador está disponível. Registrar as condições, inclusive eventuais intervenções, sem desativar o antivírus. O computador pode ser utilizado durante os testes de correção; a janela livre é necessária para o piloto e a coleta.

## Fontes das APIs

- [OpenSSL 3.5 — cifração EVP](https://docs.openssl.org/3.5/man3/EVP_PKEY_encrypt/).
- [OpenSSL 3.5 — encapsulamento EVP](https://docs.openssl.org/3.5/man3/EVP_PKEY_encapsulate/).
- [OpenSSL 3.5 — parâmetros de chaves ML-KEM](https://docs.openssl.org/3.5/man7/EVP_PKEY-ML-KEM/).
- [Microsoft — QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
