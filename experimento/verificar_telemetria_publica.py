"""Confere os CSVs publicados, sem acessar hardware ou registros privados."""
import argparse,csv,json,math,statistics as st
from pathlib import Path
from datetime import datetime,timedelta,timezone
ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);run=ap.parse_args().directory
meta=json.loads((run/'run.json').read_text(encoding='utf-8-sig'))
audit=json.loads((run/'CONFERENCIA_PUBLICA.json').read_text(encoding='utf-8'))
with (run/'telemetria_sensores.csv').open(newline='',encoding='utf-8-sig') as f: rows=list(csv.reader(f))
ids,names=rows[:2]; data=[r for r in rows[2:] if r and r[0]]
zone=timezone(timedelta(hours=-3))
times=[datetime.strptime(r[0],'%m/%d/%Y %H:%M:%S').replace(tzinfo=zone).astimezone(timezone.utc) for r in data]
start=datetime.fromisoformat(meta['DataInicioUTC']);end=datetime.fromisoformat(meta['DataFinalUTC'])
assert times[0]<=start and times[-1]>=end
chosen=[(t,r) for t,r in zip(times,data) if start<=t<=end]
assert len(chosen)==audit['telemetry_samples_during_run']
gaps=[(b[0]-a[0]).total_seconds() for a,b in zip(chosen,chosen[1:])]
assert all(g>=0 for g in gaps)
assert sum(g==0 for g in gaps)==audit.get('same_second_timestamp_pairs',0)
assert max(gaps)==audit['gap_max_seconds']
for s in audit['sensors']:
    i=ids.index(s['id']);v=[float(r[i]) for _,r in chosen]
    assert all(math.isfinite(x) for x in v)
    for actual,expected in [(min(v),s['min']),(max(v),s['max']),(st.mean(v),s['mean'])]:
        assert math.isclose(actual,expected,rel_tol=1e-12,abs_tol=1e-8)
print(json.dumps(dict(verified=True,samples=len(chosen),sensor_groups=len(audit['sensors']),same_second_pairs=sum(g==0 for g in gaps),gap_max_seconds=max(gaps))))
