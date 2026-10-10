"""Fit a low-speed yaw model to telemetry. No third-party numeric dependency.

Log timestamps select windows; uptime_ms determines physical sample intervals.
Model: dr/dt = (gain * delayed steering + bias - r) / tau.
Input and r both use the driver's left-positive convention. Units are %, deg/s.
This is a local approximation, not a verified model of the vessel.
"""
import bisect
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LOG = Path('E:/zhudian/实时仿真/logs/20261008_下位机上报数据.jsonl')


def load():
    rows = {}
    invalid = 0
    for line in LOG.read_text(encoding='utf-8').splitlines():
        raw = json.loads(line)
        d = raw.get('payload', {})
        if not all(k in d for k in ('uptime_ms', 'telemetry_seq', 'gz', 'output_steer_speed')):
            invalid += 1
            continue
        d = dict(d, received_at=raw['ts'])
        # Keep the first received copy. _raw fragments are not reconstructed.
        rows.setdefault((d['uptime_ms'], d['telemetry_seq']), d)
    return sorted(rows.values(), key=lambda d: d['uptime_ms']), invalid


def window(rows, start, end):
    return [d for d in rows if start <= d['received_at'][11:23] < end
            and d.get('heading_valid') == 1 and d.get('control_mode') == 'stable_anchor']


def effective(u, deadband):
    return math.copysign(max(abs(u) - deadband, 0), u)


def features(rows, tau, delay, deadband):
    times = [d['uptime_ms'] / 1000 for d in rows]
    inputs = [effective(d['output_steer_speed'], deadband) for d in rows]
    z = b = 0.0
    initial = rows[0]['gz']
    base = initial
    out = []
    for i in range(1, len(rows)):
        dt = times[i] - times[i-1]
        if dt <= 0:
            continue
        # Integrate with piecewise-constant input, including delayed transitions.
        cuts = [times[i-1], times[i]]
        first = bisect.bisect_right(times, times[i-1]-delay)
        last = bisect.bisect_left(times, times[i]-delay)
        cuts[1:1] = [times[j]+delay for j in range(first, last)]
        for left, right in zip(cuts, cuts[1:]):
            idx = max(0, bisect.bisect_right(times, (left+right)/2-delay)-1)
            a = math.exp(-(right-left)/tau)
            z = a*z + (1-a)*inputs[idx]
            b = a*b + 1-a
            base *= a
        # Exclude periods with unknown input during large receive gaps.
        if dt <= 1.5:
            out.append((z, b, rows[i]['gz']-base, base, rows[i]['gz']))
        else:
            z = b = 0.0
            base = rows[i]['gz']
    return out


def fit(rows, tau, delay, deadband):
    f = features(rows, tau, delay, deadband)
    zz = sum(z*z for z,b,y,base,actual in f)
    bb = sum(b*b for z,b,y,base,actual in f)
    zb = sum(z*b for z,b,y,base,actual in f)
    zy = sum(z*y for z,b,y,base,actual in f)
    by = sum(b*y for z,b,y,base,actual in f)
    det = zz*bb-zb*zb
    if det <= 1e-9:
        return None
    gain = (zy*bb-by*zb)/det
    bias = (by*zz-zy*zb)/det
    if gain <= 0:
        return None
    rmse = math.sqrt(sum((gain*z+bias*b-y)**2 for z,b,y,base,actual in f)/len(f))
    return dict(tau=tau, delay=delay, deadband=deadband, gain=gain, bias=bias, rmse=rmse, n=len(f))


def score(rows, model):
    f = features(rows, model['tau'], model['delay'], model['deadband'])
    pred = [base+model['gain']*z+model['bias']*b for z,b,y,base,actual in f]
    actual = [v[-1] for v in f]
    rmse = math.sqrt(sum((x-y)**2 for x,y in zip(pred,actual))/len(f))
    mean = sum(actual)/len(actual)
    r2 = 1-sum((x-y)**2 for x,y in zip(pred,actual))/sum((y-mean)**2 for y in actual)
    return dict(rmse=rmse, r2=r2, n=len(f))


def main():
    rows, invalid = load()
    train = window(rows, '16:45:05', '16:49:09')
    valid = window(rows, '16:57:11', '16:58:15')
    fits = []
    for deadband in (0, 1, 2, 3):
        for tau in (.2,.3,.5,.7,1,1.5,2,2.5,3,4,5,6):
            for delay in (0,.1,.2,.3,.4,.5,.7,1,1.2,1.5,2):
                m = fit(train,tau,delay,deadband)
                if m:
                    fits.append(m)
    fits.sort(key=lambda m:m['rmse'])
    for m in fits[:12]:
        m['validation'] = score(valid,m)
    result = dict(source=str(LOG), skipped_incomplete_or_missing_fields=invalid,
                  train_rows=len(train), validation_rows=len(valid), top_fits=fits[:12])
    (ROOT/'yaw_model_fit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
