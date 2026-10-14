# RSA-3072/OAEP e ML-KEM-512: acervo experimental

Material de reprodução do TCC **Análise comparativa de desempenho entre RSA e ML-KEM em ambiente computacional controlado**, de Benízio Lázaro Júnior, Engenharia da Computação, FAESA, 2026.

O estudo compara a disponibilização de um segredo de 32 bytes por RSA-3072 com RSAES-OAEP (SHA-256 e MGF1-SHA-256, rótulo vazio) e por ML-KEM-512. A coleta final contém **30 blocos, 240 lotes válidos e nenhuma exclusão**. Dois pilotos de sete blocos permanecem em diretórios separados.

## Conteúdo

- `experimento/benchmark.c`: programa C original utilizado na medição.
- `experimento/PROTOCOLO_BENCHMARK.md`: fronteiras do cronômetro, validação e organização da coleta.
- `experimento/execucoes/collect-20261012T224200Z-1c7547d6/`: dados brutos e relatórios da coleta final.
- `experimento/execucoes/pilot-*/`: dados dos pilotos; não integram as estatísticas finais.
- `experimento/logs/`: versões, fontes das dependências e validação funcional.
- `proveniencia.json`: descrição da exportação dos metadados e hashes dos originais.
- `SHA256SUMS.txt`: integridade dos arquivos publicados.

Os dados numéricos brutos foram preservados byte a byte. A exportação abrevia caminhos locais e omite snapshots de processos. Não contém chaves privadas, segredos, documentos administrativos, fontes comerciais ou cópias das dependências de terceiros.

## Resultado principal e limites

As medianas do ciclo operacional foram 1.561,944 µs para RSA e 68,179 µs para ML-KEM, com razão RSA/ML-KEM de aproximadamente 22,91. Os ciphertexts medidos tiveram 384 e 768 bytes, respectivamente.

Os tempos são médias por operação dentro de cada lote; a mediana é calculada entre 30 lotes. Não representam latências individuais. Percentis de médias de lote não garantem latência de cauda em uma implantação. Os intervalos bootstrap são aproximados, com 10.000 reamostragens; o pareamento dos mecanismos é mantido nas razões. A geração RSA apresentou CV de aproximadamente 58%, com incerteza maior que a das operações de transporte.

Os resultados descrevem Ryzen 5 5600, Windows 11 Pro, OpenSSL 3.5.5, provider default e MSVC 19.51.36257. Contextos são reutilizados. A geração prévia do segredo RSA, a validação e a serialização ficam fora do relógio. O custo interno de aleatoriedade das operações permanece incluído. O estudo não mede rede, TLS, canais laterais, certificação FIPS 140 ou superioridade de segurança.

## Recalcular a análise sem repetir a coleta

Python com biblioteca padrão é suficiente para gerar `analysis.json` e `ANALISE.md`. Execute a partir da raiz:

```powershell
python experimento/analisar_benchmark.py experimento/execucoes/collect-20261012T224200Z-1c7547d6
```

Para as figuras e os relatórios complementares, use um ambiente Python separado:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe experimento/gerar_resultados.py experimento/execucoes/collect-20261012T224200Z-1c7547d6
```

Os scripts escrevem os relatórios no diretório informado. Para manter uma cópia intacta dos arquivos publicados, faça a reprodução em outra cópia do repositório. A figura solicita Arial; a disponibilidade da fonte pode alterar sua aparência sem mudar as estatísticas.

## Compilar e executar uma nova coleta

1. Use Windows x64, MSVC x64, Strawberry Perl 5.42.3.1 e NASM 2.16.03. Os links de origem e hashes registrados estão em `experimento/logs/dependencias.json`.
2. Extraia o código-fonte oficial OpenSSL 3.5.5 em `experimento/dependencias/openssl-3.5.5`, o Perl portátil em `experimento/dependencias/perl` e o NASM em `experimento/dependencias/nasm-2.16.03`.
3. Crie `experimento/build` e `experimento/logs` se não existirem. Os scripts `.cmd` utilizam o caminho `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`; ajuste-o apenas se a instalação local for diferente e registre essa alteração.
4. Execute `experimento/compilar_openssl.cmd`, depois `experimento/compilar_validacao.cmd`, `experimento/testar_openssl.cmd` e `experimento/compilar_benchmark.cmd`. A configuração OpenSSL é `VC-WIN64A shared`, com assembly habilitado.
5. Execute a validação e os pilotos antes de uma nova coleta. Os seis grupos selecionados de testes da biblioteca não correspondem à suíte completa.

```powershell
.\experimento\executar_benchmark.ps1 -Mode validate
.\experimento\executar_benchmark.ps1 -Mode pilot -Blocks 7 -ComputadorLivre
.\experimento\executar_benchmark.ps1 -Mode collect -Blocks 30 -Cpu 0 -Seed 20261014 -WarmRsa 3 -WarmFast 1000 -Batches '1,1024,1024,1024,16,512,16,512' -ComputadorLivre
```

Prepare as condições antes de confirmar `-ComputadorLivre`. O programa não encerra aplicativos nem pausa sincronizações. A ordem dos lotes é: RSA geração, ML-KEM geração, RSA envio, ML-KEM envio, RSA recepção, ML-KEM recepção, RSA ciclo e ML-KEM ciclo.

`executar_coleta.ps1` preserva uma verificação de hashes dos binários da coleta original. Um binário recompilado pode ter hash diferente, mesmo com o mesmo código. Para uma nova execução, use o comando explícito acima ou congele uma nova configuração documentada após os pilotos. Não substitua os hashes para representar uma nova compilação como a original.

## Identificação da versão no TCC

Após a publicação, registrar a URL do repositório e o identificador do commit que contém os dados usados no trabalho. Uma tag poderá identificar a versão apresentada à banca. O conteúdo experimental não deve ser alterado para coincidir artificialmente com resultados esperados.
