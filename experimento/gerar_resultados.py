"""Gera tabelas, efeito pareado e figuras da coleta, diretamente dos dados."""
import argparse
import csv
import importlib.metadata
import json
import os
import random
import statistics
import sys
from collections import defaultdict
from pathlib import Path

BASE = Path(__file__).resolve().parent
sys.path.insert(0, str(BASE / 'ferramentas_python'))
os.environ['MPLCONFIGDIR'] = str(BASE / 'cache_matplotlib')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from analisar_benchmark import percentile


def bootstrap_ratio(rsa, kem):
    rng = random.Random(20261012)
    values = []
    for _ in range(10000):
        index = [rng.randrange(len(rsa)) for _ in rsa]
        values.append(statistics.median([rsa[i] for i in index]) /
                      statistics.median([kem[i] for i in index]))
    return [percentile(values, .025), percentile(values, .975)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    directory = args.directory.resolve()
    analysis = json.loads((directory / 'analysis.json').read_text(encoding='utf-8'))
    if analysis['mode'] != 'collect' or analysis['valid_batches'] != 240:
        raise ValueError('Este relatório foi preparado para a coleta inicial de 30 blocos.')
    rows = {(r['algorithm'], r['operation']): r for r in analysis['results']}
    effects = []
    for op in ['keygen', 'send', 'receive', 'cycle']:
        rsa = rows[('RSA-3072-OAEP-SHA256', op)]
        kem = rows[('ML-KEM-512', op)]
        effects.append({
            'operation': op, 'ratio_of_medians_RSA_over_MLKEM': rsa['median_ns'] / kem['median_ns'],
            'paired_block_bootstrap_ci95': bootstrap_ratio(rsa['all_block_means_ns'], kem['all_block_means_ns']),
            'assumption': 'blocos aproximadamente intercambiaveis; mesma reamostragem para os dois mecanismos',
        })
    diagnostics = []
    for row in analysis['results']:
        values = row['all_block_means_ns']
        center = statistics.mean(values)
        denominator = sum((v-center)**2 for v in values)
        lag1 = sum((a-center)*(b-center) for a,b in zip(values[:-1], values[1:])) / denominator if denominator else None
        diagnostics.append({
            'algorithm': row['algorithm'], 'operation': row['operation'],
            'lag1_autocorrelation': lag1,
            'median_first15_ns': statistics.median(values[:15]),
            'median_last15_ns': statistics.median(values[15:]),
            'interpretation': 'diagnostico descritivo; nao comprova independencia temporal nem estacionariedade',
        })
    sizes = defaultdict(list)
    with (directory / 'sizes.csv').open(newline='') as file:
        for row in csv.DictReader(file):
            sizes[(row['algorithm'], row['artifact'], row['representation'])].append(int(row['bytes']))
    artifacts = [{'algorithm': key[0], 'artifact': key[1], 'representation': key[2],
                  'n': len(values), 'min_bytes': min(values), 'max_bytes': max(values)}
                 for key, values in sorted(sizes.items())]
    (directory / 'effects_and_diagnostics.json').write_text(json.dumps({
        'effects': effects, 'diagnostics': diagnostics, 'artifacts': artifacts,
        'percentiles_scope': 'distribuicao das 30 medias de lote, nao de latencias individuais',
        'libraries': {name: importlib.metadata.version(name) for name in ['matplotlib','numpy','pillow']}
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    with (directory / 'estatisticas.csv').open('w', newline='', encoding='utf-8') as file:
        fields = ['algorithm','operation','n_blocks','iterations_per_block','mean_ns','median_ns','stdev_ns',
                  'p5_ns','p25_ns','p75_ns','p95_ns','p99_ns','cv_percent','ci95_median_low_ns','ci95_median_high_ns']
        writer = csv.DictWriter(file, fieldnames=fields); writer.writeheader()
        for row in analysis['results']:
            record = {key:row[key] for key in fields if key in row}
            record['ci95_median_low_ns'], record['ci95_median_high_ns'] = row['median_ci95_bootstrap_ns']
            writer.writerow(record)
    plt.rcParams.update({'font.family':'Arial','font.size':10,'axes.labelsize':10,'axes.titlesize':11,
                         'figure.dpi':140,'savefig.dpi':300,'axes.spines.top':False,'axes.spines.right':False})
    colors = ['#355a78','#9a542c']
    labels = {'keygen':'Geração de chaves','send':'Envio','receive':'Recepção','cycle':'Ciclo operacional'}
    fig, axes = plt.subplots(2, 2, figsize=(8.2, 6.8))
    for ax, op in zip(axes.flat, labels):
        for i, alg in enumerate(['RSA-3072-OAEP-SHA256','ML-KEM-512']):
            row = rows[(alg,op)]
            values = np.array(row['all_block_means_ns']) / 1000
            jitter = np.linspace(-.14, .14, len(values))
            ax.scatter(i+jitter, values, color=colors[i], s=15, alpha=.65, label='Médias de lote' if i==0 else None)
            median = row['median_ns'] / 1000
            low, high = [v/1000 for v in row['median_ci95_bootstrap_ns']]
            ax.errorbar(i, median, yerr=[[median-low],[high-median]], color='black', marker='_',
                        markersize=16, linewidth=1.7, capsize=5, label='Mediana e IC 95% bootstrap' if i==0 else None)
        ax.set_yscale('log'); ax.set_xticks([0,1], ['RSA-3072/OAEP','ML-KEM-512'])
        ax.set_title(labels[op]); ax.set_ylabel('Tempo (µs/operação; escala log)')
        ax.set_xlim(-.45,1.45); ax.grid(axis='y', which='major', alpha=.2)
    handles, legend_labels = axes[0,0].get_legend_handles_labels()
    fig.legend(handles, legend_labels, loc='lower center', ncol=2, bbox_to_anchor=(.5,.045), frameon=False)
    fig.suptitle('Distribuição das médias de lote por operação', fontsize=12)
    fig.text(.5,.012,'n = 30 lotes por combinação · Fonte: elaboração própria',ha='center',fontsize=9)
    fig.tight_layout(rect=[0,.10,1,.95])
    fig.savefig(directory/'figura_01_distribuicoes.png'); fig.savefig(directory/'figura_01_distribuicoes.svg')
    plt.close(fig)
    fig, ax = plt.subplots(figsize=(7.6,3.3))
    values = [rows[(alg,'cycle')]['median_ns']/1000 for alg in ['RSA-3072-OAEP-SHA256','ML-KEM-512']]
    ax.barh([1,0],values,color=colors,height=.48)
    for y,alg,median in zip([1,0],['RSA-3072-OAEP-SHA256','ML-KEM-512'],values):
        low,high=[v/1000 for v in rows[(alg,'cycle')]['median_ci95_bootstrap_ns']]
        ax.errorbar(median,y,xerr=[[median-low],[high-median]],color='black',capsize=5)
        ax.text(median+35,y,f'{median:.2f} µs',va='center',fontsize=10)
    ax.set_yticks([1,0],['RSA: cifração + decifração','ML-KEM: encaps. + decaps.'])
    ax.set_xlim(0,max(values)*1.2);ax.set_xlabel('Mediana do tempo por ciclo (µs)')
    ax.set_title('Ciclo operacional: mediana e IC 95% bootstrap')
    ax.grid(axis='x',alpha=.2); ax.set_axisbelow(True)
    fig.text(.5,.025,'n = 30 lotes por mecanismo · Fonte: elaboração própria',ha='center',fontsize=9)
    fig.tight_layout(rect=[0,.07,1,1]);fig.savefig(directory/'figura_02_ciclo.png');fig.savefig(directory/'figura_02_ciclo.svg')
    plt.close(fig)
    lines = ['# Resultados da coleta inicial de 30 blocos', '',
             'A coleta concluiu 240 lotes válidos, com 30 observações por combinação. Nenhuma amostra foi excluída. Os pilotos permanecem separados.', '',
             'Os tempos abaixo são médias por operação de cada lote. O resumo usa a mediana dessas 30 médias. Os intervalos de confiança são aproximações por bootstrap percentil com 10.000 reamostragens; não demonstram independência temporal por si só.', '',
             '| Mecanismo | Operação | Mediana (µs) | IC 95% da mediana (µs) | CV (%) |',
             '|---|---|---:|---:|---:|']
    for row in analysis['results']:
        low, high = row['median_ci95_bootstrap_ns']
        lines.append(f'| {row["algorithm"]} | {labels[row["operation"]]} | {row["median_ns"]/1000:.3f} | {low/1000:.3f}–{high/1000:.3f} | {row["cv_percent"]:.2f} |')
    lines += ['', '## Comparação operacional', '', '| Operação | Razão RSA/ML-KEM das medianas | IC 95% pareado |', '|---|---:|---:|']
    for effect in effects:
        low,high=effect['paired_block_bootstrap_ci95']
        lines.append(f'| {labels[effect["operation"]]} | {effect["ratio_of_medians_RSA_over_MLKEM"]:.2f} | {low:.2f}–{high:.2f} |')
    lines += ['', 'A razão compara as medianas dos tempos; valores maiores que 1 indicam maior tempo no caminho RSA neste ambiente. Não representa equivalência de segurança. O intervalo utiliza reamostragem dos blocos preservando o pareamento dos mecanismos.', '',
              '## Tamanhos observados', '', '| Mecanismo | Artefato | Representação | Intervalo (bytes) |', '|---|---|---|---:|']
    for artifact in artifacts:
        interval = str(artifact['min_bytes']) if artifact['min_bytes']==artifact['max_bytes'] else f'{artifact["min_bytes"]}–{artifact["max_bytes"]}'
        lines.append(f'| {artifact["algorithm"]} | {artifact["artifact"]} | {artifact["representation"]} | {interval} |')
    lines += ['', '## Avaliação da amostra e limites', '',
              'As operações de transporte apresentaram CV de aproximadamente 0,9% a 2,7%. Os intervalos das medianas foram estreitos, com semilargura relativa entre aproximadamente 0,3% e 1,2%. Esta coleta fornece evidência para descrever as diferenças de transporte nas condições registradas.', '',
              'A geração RSA apresentou CV de aproximadamente 58%, mediana de 103,539 ms e intervalo bootstrap aproximado de 91,992 a 126,919 ms. O intervalo é consideravelmente mais amplo que os das operações de transporte. A comparação com ML-KEM permanece claramente separada nesta amostra, mas a estimativa detalhada da geração RSA deve manter essa incerteza explícita.', '',
              'Os coeficientes de autocorrelação de defasagem 1 ficaram em magnitude abaixo de 0,2. Isso é um diagnóstico descritivo, sem provar independência temporal. As medianas da geração RSA nas metades da execução também diferiram; com 30 observações e alta dispersão, não é possível atribuir isso automaticamente a uma tendência térmica ou excluir a influência do ambiente.', '',
              'Não é obrigatório repetir a coleta apenas porque a geração RSA tem alta dispersão: ela pode ser relatada com os intervalos e limitações observados. Uma amostra maior será necessária se o objetivo passar a exigir estimativa mais precisa dessa operação ou caracterização forte de caudas. Essa decisão não deve depender de obter um resultado favorável.', '',
              '**P95 e P99 em `estatisticas.csv` são percentis de médias de lote, não da latência individual.** Com 30 lotes, a descrição dos extremos é limitada. Não serão apresentados como garantia de latência de uma implantação.', '',
              'Os resultados são restritos ao Ryzen 5 5600, Windows 11, OpenSSL 3.5.5, MSVC e à definição da região cronometrada. Contextos são reutilizados; geração prévia das entradas RSA, serialização e validação ficam fora do relógio. O custo de comunicação e as diferenças conceituais e de segurança continuam separados da avaliação de tempo.', '',
              'Os snapshots de processos podem ter alcance limitado pelas permissões do terminal. A confirmação de computador livre foi registrada, mas a ausência de um processo nesses snapshots não comprova que ele estava fechado. Os resultados também representam a condição de execução do processo de benchmark, incluindo eventuais restrições do terminal.', '',
              'Estes resultados fundamentam as tabelas e a discussão do TCC.', '']
    (directory/'RESULTADOS.md').write_text('\n'.join(lines),encoding='utf-8')
    for effect in effects:print(effect)
    print(directory/'RESULTADOS.md')


if __name__ == '__main__':main()
