"""Analisa lotes validados, sem misturar aquecimento, calibracao ou validacao."""
import argparse
import csv
import hashlib
import json
import math
import random
import statistics
from collections import defaultdict
from pathlib import Path


def percentile(values, p):
    ordered = sorted(values)
    location = (len(ordered) - 1) * p
    low = math.floor(location)
    high = math.ceil(location)
    return ordered[low] + (ordered[high] - ordered[low]) * (location - low)


def bootstrap_median(values, repetitions=10000):
    rng = random.Random(20261012)
    medians = [statistics.median(rng.choices(values, k=len(values))) for _ in range(repetitions)]
    return [percentile(medians, 0.025), percentile(medians, 0.975)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    directory = args.directory.resolve()
    config = json.loads((directory / 'config.json').read_text(encoding='utf-8'))
    manifest = json.loads((directory / 'run.json').read_text(encoding='utf-8-sig'))
    if config['mode'] not in ('pilot', 'collect'):
        raise ValueError('Validacao de desenvolvimento nao produz resultados de desempenho.')
    if not (directory / 'SUCCESS.txt').exists() or (directory / 'FAILURE.txt').exists() or manifest['ExitCode'] != 0:
        raise ValueError('A execucao nao terminou com validacao completa.')
    with (directory / 'raw.csv').open(newline='', encoding='utf-8') as file:
        all_rows = list(csv.DictReader(file))
    rows = [row for row in all_rows if row['phase'] == 'sample']
    if len(rows) != config['blocks'] * 8:
        raise ValueError('Numero de lotes diferente do protocolo.')
    groups = defaultdict(list)
    expected = {(a, op) for a in ('RSA-3072-OAEP-SHA256', 'ML-KEM-512') for op in ('keygen', 'send', 'receive', 'cycle')}
    for block in range(1, config['blocks'] + 1):
        block_rows = [row for row in rows if int(row['block']) == block]
        if {(row['algorithm'], row['operation']) for row in block_rows} != expected:
            raise ValueError(f'Bloco {block} incompleto.')
        if sorted(int(row['sequence']) for row in block_rows) != list(range(8)):
            raise ValueError(f'Ordem incompleta no bloco {block}.')
    for row in rows:
        if row['validated'] != '1' or row['timed'] != '1':
            raise ValueError('Lote nao validado ou nao cronometrado.')
        total_ticks = int(row['total_ticks'])
        iterations = int(row['iterations'])
        if total_ticks <= 0 or iterations <= 0:
            raise ValueError('Duracao ou iteracoes invalidas.')
        value = total_ticks * 1e9 / config['qpc_frequency_hz'] / iterations
        if abs(value - float(row['ns_per_operation'])) > 0.001:
            raise ValueError('Conversao de tempo inconsistente.')
        groups[(row['algorithm'], row['operation'])].append((row, value))

    results = []
    for algorithm, operation in sorted(groups):
        records = groups[(algorithm, operation)]
        values = [value for _, value in records]
        iteration_counts = {int(row['iterations']) for row, _ in records}
        if len(iteration_counts) != 1:
            raise ValueError('Lotes de tamanhos distintos dentro da coleta.')
        durations_ms = [int(row['total_ticks']) * 1000 / config['qpc_frequency_hz'] for row, _ in records]
        mean = statistics.mean(values)
        stdev = statistics.stdev(values) if len(values) > 1 else 0
        results.append({
            'algorithm': algorithm, 'operation': operation,
            'n_blocks': len(values), 'iterations_per_block': iteration_counts.pop(),
            'mean_ns': mean, 'median_ns': statistics.median(values), 'stdev_ns': stdev,
            'cv_percent': stdev / mean * 100,
            'min_ns': min(values), 'max_ns': max(values),
            'p5_ns': percentile(values, .05), 'p25_ns': percentile(values, .25),
            'p75_ns': percentile(values, .75), 'p95_ns': percentile(values, .95),
            'p99_ns': percentile(values, .99),
            'median_ci95_bootstrap_ns': bootstrap_median(values),
            'min_batch_ms': min(durations_ms), 'max_batch_ms': max(durations_ms),
            'all_block_means_ns': values,
        })
    with (directory / 'clock.csv').open(newline='', encoding='utf-8') as file:
        clock = [int(row['delta_ticks']) * 1e9 / config['qpc_frequency_hz'] for row in csv.DictReader(file)]
    if len(clock) != 2000 or any(value < 0 for value in clock):
        raise ValueError('Sondagem do relogio invalida.')
    clock_summary = {
        'n': len(clock), 'nominal_tick_ns': 1e9 / config['qpc_frequency_hz'],
        'mean_delta_ns': statistics.mean(clock), 'median_delta_ns': statistics.median(clock),
        'p95_delta_ns': percentile(clock, .95), 'max_delta_ns': max(clock),
        'min_positive_delta_ns': min((x for x in clock if x > 0), default=None),
    }
    source_hashes = {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
                     for name in ('raw.csv', 'sizes.csv', 'config.json', 'run.json', 'clock.csv', 'selected_batches.txt')}
    report = {
        'mode': config['mode'], 'run_directory': directory.name,
        'observational_unit': 'media por operacao de um lote; blocos nao sao reinicios independentes do sistema',
        'samples_per_combination': config['blocks'], 'valid_batches': len(rows),
        'excluded_sample_rows': 0,
        'non_sample_rows_kept_in_original': len(all_rows) - len(rows),
        'bootstrap': {'seed': 20261012, 'resamples': 10000, 'method': 'percentile CI for block median'},
        'power_scheme_equal_at_start_and_end': manifest['PlanoEnergiaInicio'] == manifest['PlanoEnergiaFinal'],
        'computer_free_confirmed_by_user': manifest['ComputadorLivreConfirmado'],
        'clock': clock_summary, 'results': results, 'source_sha256': source_hashes,
        'limits': [
            'Piloto nao e resultado definitivo do TCC.' if config['mode']=='pilot' else 'Configuracao limitada a este ambiente.',
            'Sete blocos sao insuficientes para afirmar estabilidade, independência temporal ou caracterizar caudas.' if config['blocks']==7 else 'Independência temporal e caudas precisam ser avaliadas.',
            'Snapshots de processos podem ser limitados pelas permissoes do terminal; ausencia nao comprova que um aplicativo estava fechado.',
            'Diferencas entre leituras adjacentes do QPC nao demonstram precisao universal; mediana zero nao significa custo zero.',
        ],
    }
    (directory / 'analysis.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    label = 'Piloto' if config['mode'] == 'pilot' else 'Coleta'
    lines = [f'# {label} do benchmark — análise exploratória', '',
             f'Foram conferidos {len(rows)} lotes válidos: {config["blocks"]} blocos por combinação. Nenhuma amostra foi excluída.', '',
             'A unidade estatística é a média por operação de um lote. As iterações do lote não são tratadas como observações independentes.', '',
             '| Mecanismo | Operação | Iterações/lote | Mediana (µs/operação) | CV (%) | Duração dos lotes (ms) |',
             '|---|---|---:|---:|---:|---:|']
    labels={'keygen':'Geração','send':'Envio','receive':'Recepção','cycle':'Ciclo'}
    for result in results:
        lines.append(f'| {result["algorithm"]} | {labels[result["operation"]]} | {result["iterations_per_block"]} | {result["median_ns"]/1000:.3f} | {result["cv_percent"]:.2f} | {result["min_batch_ms"]:.2f}–{result["max_batch_ms"]:.2f} |')
    lines += ['', '## Relógio', '',
              f'O contador tem frequência de {config["qpc_frequency_hz"]:,} Hz e incremento nominal de {clock_summary["nominal_tick_ns"]:.1f} ns. Nas 2.000 diferenças adjacentes, a média foi {clock_summary["mean_delta_ns"]:.1f} ns e a mediana {clock_summary["median_delta_ns"]:.1f} ns. A mediana zero decorre da quantização possível; não demonstra custo nulo.', '',
              'Esse custo é registrado separadamente. Não foi subtraído dos lotes.', '', '## Limites da leitura', '']
    lines += [f'- {limit}' for limit in report['limits']]
    lines += ['', 'Percentis e intervalos bootstrap estão em `analysis.json`; no piloto são apenas exploratórios. Não serão apresentados como caracterização definitiva com sete blocos.', '',
              'Os dados brutos de aquecimento e calibração continuam preservados. Nenhuma linha da fase `sample` foi removida. A ordem, os formatos das chaves e os hashes estão nos arquivos da execução.', '']
    (directory / 'ANALISE.md').write_text('\n'.join(lines), encoding='utf-8')
    print(directory / 'ANALISE.md')
    for result in results:
        print(result['algorithm'], result['operation'], 'mediana_us=',round(result['median_ns']/1000,3), 'CV=',round(result['cv_percent'],2))


if __name__ == '__main__':
    main()
