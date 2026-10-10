"""Strictly decode full-day JSON telemetry; never repair incomplete frames."""
import collections
import datetime
import hashlib
import json
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parent
LOG=Path('E:/zhudian/实时仿真/logs/20261008_下位机上报数据.jsonl')


def load():
    counts=collections.Counter()
    decoded=[]
    decoder=json.JSONDecoder()
    for line_no,line in enumerate(LOG.read_text(encoding='utf-8').splitlines(),1):
        raw=json.loads(line)
        ts=datetime.datetime.fromisoformat(raw['ts']).timestamp()
        d=raw.get('payload',{})
        objects=[]
        if '_raw' not in d:
            objects=[d]
            counts['structured_log_records']+=1
        else:
            text=d['_raw'];i=0
            while i<len(text):
                while i<len(text) and text[i].isspace():i+=1
                if i==len(text):break
                try:obj,end=decoder.raw_decode(text,i)
                except json.JSONDecodeError:
                    counts['raw_records_with_invalid_tail']+=1
                    break
                if isinstance(obj,dict):
                    objects.append(obj)
                    counts['strictly_decoded_full_objects_in_raw']+=1
                i=end
        for obj in objects:
            if not all(k in obj for k in ('heading','speed','gz','raw_lat','raw_lon')):
                counts['not_telemetry']+=1;continue
            d=dict(obj,received_at=raw['ts'],wall=ts,log_line=line_no)
            d['forward']=d['speed']*math.cos(math.radians(d.get('gnss_yaw',d['heading'])-d['heading']))
            decoded.append(d)
    decoded.sort(key=lambda d:d['wall'])
    seen=set();rows=[];epoch=0;last_clock=None;last_wall=None
    for d in decoded:
        clock=d.get('uptime_ms')
        if clock is not None:
            if last_clock is not None and clock<last_clock-5000 and d['wall']-last_wall>5:
                epoch+=1;counts['detected_clock_restart']+=1
            last_clock=clock;last_wall=d['wall']
            key=(epoch,clock,d.get('telemetry_seq'))
            if key in seen:
                counts['duplicate_clocked_objects']+=1;continue
            seen.add(key)
            d['epoch']=epoch;d['t']=clock/1000
        else:
            # Legacy records are retained, but excluded from physical model fits.
            d['epoch']=None;d['t']=d['wall']
        rows.append(d)
    counts['decoded_telemetry_objects']=len(decoded)
    counts['retained_telemetry_objects']=len(rows)
    return rows,dict(counts)


def usable(d):
    return d.get('data_valid')==1 and d.get('heading_valid')==1


def runs(rows,predicate):
    groups=[];current=[]
    for d in rows:
        if predicate(d):
            same=current and d['epoch']==current[-1]['epoch'] and 0<d['t']-current[-1]['t']<=1.5
            if not same and current:groups.append(current);current=[]
            current.append(d)
        elif current:
            groups.append(current);current=[]
    if current:groups.append(current)
    return groups


def summary(group):
    return dict(start=group[0]['received_at'],end=group[-1]['received_at'],n=len(group),
                seconds=group[-1]['t']-group[0]['t'],epoch=group[0]['epoch'],
                modes=dict(collections.Counter(d.get('control_mode','unknown') for d in group)),
                move_range=[min(d['output_move_speed'] for d in group),max(d['output_move_speed'] for d in group)],
                steer_range=[min(d['output_steer_speed'] for d in group),max(d['output_steer_speed'] for d in group)],
                forward_range=[min(d['forward'] for d in group),max(d['forward'] for d in group)])


def main():
    rows,counts=load()
    clocked=[d for d in rows if d['epoch'] is not None and 'output_move_speed' in d]
    mode=collections.Counter(d.get('control_mode') for d in clocked)
    straight=runs(clocked,lambda d:usable(d) and d.get('control_mode') in ('manual','cruise_speed')
                  and d['output_move_speed']>0 and abs(d['output_steer_speed'])<=3 and abs(d['gz'])<=3)
    low_yaw=runs(clocked,lambda d:usable(d) and abs(d['output_move_speed'])<=2)
    forward=runs(clocked,lambda d:usable(d) and d['output_move_speed']>=0 and abs(d['gz'])<=5 and abs(d['output_steer_speed'])<=5)
    result=dict(source=str(LOG),source_sha256=hashlib.sha256(LOG.read_bytes()).hexdigest(),counts=counts,
                first=rows[0]['received_at'],last=rows[-1]['received_at'],clocked_modes=dict(mode),
                straight_runs=[summary(s) for s in straight if len(s)>=3],
                low_yaw_runs=[summary(s) for s in low_yaw if len(s)>=8],
                forward_runs=[summary(s) for s in forward if len(s)>=5])
    (ROOT/'full_log_audit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    (ROOT/'canonical_telemetry.jsonl').write_text(''.join(json.dumps(d,ensure_ascii=False)+'\n' for d in rows),encoding='utf-8')
    print('coverage',result['first'],result['last']);print('counts',counts);print('modes',mode)
    print('straight runs')
    for s in result['straight_runs']:
        if s['seconds']>=1:print(s)
    print('yaw runs >=5 seconds')
    for s in result['low_yaw_runs']:
        if s['seconds']>=5:print(s)


if __name__=='__main__':main()
