"""Recálculo a partir dos ticks e diagnóstico de uma sessão complementar."""
from pathlib import Path
from collections import defaultdict
import csv,json,statistics as st,random,math,hashlib,argparse

def pct(x,p):
    x=sorted(x);t=(len(x)-1)*p;i=math.floor(t);j=math.ceil(t)
    return x[i]+(x[j]-x[i])*(t-i)
def bootstrap(x,fn,seed=20261012):
    rng=random.Random(seed);v=[fn(rng.choices(x,k=len(x))) for _ in range(10000)]
    return [pct(v,.025),pct(v,.975)]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);args=ap.parse_args()
    run=args.directory.resolve();config=json.loads((run/'config.json').read_text(encoding='utf-8'));analysis=json.loads((run/'analysis.json').read_text(encoding='utf-8'))
    with (run/'raw.csv').open(encoding='utf-8',newline='') as f:raw=list(csv.DictReader(f))
    groups=defaultdict(list);ticks={}
    for row in raw:
        if row['phase']!='sample':continue
        value=int(row['total_ticks'])*1e9/config['qpc_frequency_hz']/int(row['iterations'])
        assert row['validated']==row['timed']=='1'
        assert math.isclose(value,float(row['ns_per_operation']),abs_tol=.001)
        assert math.isclose(int(row['total_ticks'])*1e9/config['qpc_frequency_hz'],float(row['total_ns']),abs_tol=.001)
        key=(row['algorithm'],row['operation']);block=int(row['block'])
        assert (key,block) not in ticks
        ticks[key,block]=value;groups[key].append((block,value))
    assert len(ticks)==2400
    stats=[]
    for key,records in sorted(groups.items()):
        records.sort();assert [b for b,_ in records]==list(range(1,301));x=[v for _,v in records]
        old=next(r for r in analysis['results'] if (r['algorithm'],r['operation'])==key)
        assert x==old['all_block_means_ns']
        fields={'mean_ns':st.mean(x),'median_ns':st.median(x),'stdev_ns':st.stdev(x),'cv_percent':100*st.stdev(x)/st.mean(x)}
        fields.update({f'p{p}_ns':pct(x,p/100) for p in [5,25,75,95,99]})
        for name,value in fields.items():assert math.isclose(value,old[name],rel_tol=1e-12,abs_tol=1e-6),(key,name)
        ci=bootstrap(x,st.median)
        assert all(math.isclose(a,b,abs_tol=1e-6) for a,b in zip(ci,old['median_ci95_bootstrap_ns']))
        # Média da reamostragem calculada por soma/n; mesma unidade observacional.
        mean_ci=bootstrap(x,lambda v:sum(v)/len(v))
        center=st.mean(x);den=sum((v-center)**2 for v in x)
        lag=sum((a-center)*(b-center) for a,b in zip(x[:-1],x[1:]))/den if den else None
        stats.append(dict(algorithm=key[0],operation=key[1],n=300,median_ns=st.median(x),mean_ns=st.mean(x),
            median_ci95_ns=ci,mean_ci95_ns=mean_ci,median_first150_ns=st.median(x[:150]),median_last150_ns=st.median(x[150:]),
            lag1_descriptive=lag,scope='Médias de lote dentro desta sessão; IC condicionado à adequação da reamostragem.'))
    effects=[]
    for op in ['keygen','send','receive','cycle']:
        a=[ticks[('RSA-3072-OAEP-SHA256',op),i] for i in range(1,301)];b=[ticks[('ML-KEM-512',op),i] for i in range(1,301)]
        rng=random.Random(20261012);v=[]
        for _ in range(10000):
            ids=[rng.randrange(300) for _ in range(300)]
            v.append(st.median([a[i] for i in ids])/st.median([b[i] for i in ids]))
        effects.append(dict(operation=op,ratio_of_medians=st.median(a)/st.median(b),paired_ci95=[pct(v,.025),pct(v,.975)],resamples=10000,seed=20261012))
    sizes=defaultdict(list)
    with (run/'sizes.csv').open(encoding='utf-8',newline='') as f:
        for row in csv.DictReader(f):sizes[(row['algorithm'],row['artifact'],row['representation'])].append((int(row['block']),int(row['bytes'])))
    size_report=[]
    for key,values in sorted(sizes.items()):
        assert sorted(b for b,_ in values)==list(range(1,301)),key
        size_report.append(dict(algorithm=key[0],artifact=key[1],representation=key[2],n=300,min_bytes=min(v for _,v in values),max_bytes=max(v for _,v in values)))
    artifacts=['raw.csv','sizes.csv','clock.csv','config.json','run.json','selected_batches.txt','SUCCESS.txt','telemetria_sensores.csv']
    report=dict(run=run.name,statistics_recomputed_from_raw_ticks=True,statistics=stats,paired_effects=effects,artifact_sizes=size_report,
        sha256={p:hashlib.sha256((run/p).read_bytes()).hexdigest() for p in artifacts},original_rows_preserved=True)
    (run/'CONFERENCIA_ESTATISTICA.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    (run/'MANIFESTO_SHA256.json').write_text(json.dumps(report['sha256'],indent=2),encoding='utf-8')
    print(json.dumps({'verified':True,'paired_effects':effects,'rsa_keygen':next(x for x in stats if x['algorithm'].startswith('RSA') and x['operation']=='keygen'),'size_groups':len(size_report)},ensure_ascii=False))
if __name__=='__main__':main()
