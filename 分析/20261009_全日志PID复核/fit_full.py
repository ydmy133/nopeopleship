"""Fit yaw and planar surge models; hold out whole maneuvers, not random rows."""
import bisect
import json
import math
import sys
from pathlib import Path
from full_scan import ROOT, load, usable

sys.path.insert(0,str(ROOT.parent/'20261008_稳泊转向诊断'))
from tune_yaw import features as yaw_features, fit as yaw_fit, score as yaw_score


def select(rows,start,end):
    result=[d for d in rows if d['epoch'] is not None and start<=d['received_at'][11:23]<end
            and usable(d) and 'output_move_speed' in d]
    result.sort(key=lambda d:(d['epoch'],d['uptime_ms']))
    return result


def velocity(d):
    cog=math.radians(d['gnss_yaw'])
    return d['speed']*math.cos(cog),d['speed']*math.sin(cog)


def surge_features(rows,tau,delay,deadband):
    ts=[d['t'] for d in rows]
    inputs=[]
    for d in rows:
        u=max(d['output_move_speed']-deadband,0)
        h=math.radians(d['heading']);inputs.append((u*math.cos(h),u*math.sin(h)))
    base_n,base_e=velocity(rows[0]);z_n=z_e=b=0.;out=[]
    for i in range(1,len(rows)):
        dt=ts[i]-ts[i-1]
        vn,ve=velocity(rows[i])
        if dt<=0 or dt>1.5 or rows[i]['epoch']!=rows[i-1]['epoch']:
            base_n,base_e=vn,ve;z_n=z_e=b=0.;continue
        cuts=[ts[i-1],ts[i]]
        lo=bisect.bisect_right(ts,ts[i-1]-delay);hi=bisect.bisect_left(ts,ts[i]-delay)
        cuts[1:1]=[ts[j]+delay for j in range(lo,hi)]
        for left,right in zip(cuts,cuts[1:]):
            idx=max(0,bisect.bisect_right(ts,(left+right)/2-delay)-1)
            a=math.exp(-(right-left)/tau)
            z_n=a*z_n+(1-a)*inputs[idx][0];z_e=a*z_e+(1-a)*inputs[idx][1]
            b=a*b+1-a;base_n*=a;base_e*=a
        out.append((z_n,z_e,b,vn-base_n,ve-base_e,vn,ve,base_n,base_e))
    return out


def surge_fit(rows,tau,delay,deadband):
    f=surge_features(rows,tau,delay,deadband)
    bb=sum(v[2]**2 for v in f)
    if bb<1e-6:return None
    zn_b=sum(v[0]*v[2] for v in f);ze_b=sum(v[1]*v[2] for v in f)
    yn_b=sum(v[3]*v[2] for v in f);ye_b=sum(v[4]*v[2] for v in f)
    zz=sum(v[0]**2+v[1]**2 for v in f)-(zn_b**2+ze_b**2)/bb
    zy=sum(v[0]*v[3]+v[1]*v[4] for v in f)-(zn_b*yn_b+ze_b*ye_b)/bb
    if zz<=1e-9:return None
    gain=zy/zz
    if gain<=0:return None
    bn=(yn_b-gain*zn_b)/bb;be=(ye_b-gain*ze_b)/bb
    rmse=math.sqrt(sum((gain*zn+bn*b-yn)**2+(gain*ze+be*b-ye)**2 for zn,ze,b,yn,ye,*rest in f)/(2*len(f)))
    return dict(tau=tau,delay=delay,deadband=deadband,gain=gain,bias_n=bn,bias_e=be,rmse=rmse,n=len(f))


def surge_score(rows,m):
    f=surge_features(rows,m['tau'],m['delay'],m['deadband'])
    residual=sum((m['gain']*zn+m['bias_n']*b-yn)**2+(m['gain']*ze+m['bias_e']*b-ye)**2 for zn,ze,b,yn,ye,*rest in f)
    nmean=sum(v[5] for v in f)/len(f);emean=sum(v[6] for v in f)/len(f)
    variance=sum((v[5]-nmean)**2+(v[6]-emean)**2 for v in f)
    return dict(n=len(f),rmse=math.sqrt(residual/(2*len(f))),r2=1-residual/variance if variance else None)


def main():
    rows,counts=load()
    # The whole log is audited. The clocked epoch with observable movement is
    # identified from the data; bench/no-response and legacy schemas stay separate.
    train=select(rows,'16:39:00','16:45:05')
    valid=select(rows,'16:49:09','17:00:11')
    speed=[]
    for tau in (.3,.5,1,2,3,4,6,8,12,20):
        for delay in (0,.2,.5,1):
            for deadband in (0,5,10,15):
                fit=surge_fit(train,tau,delay,deadband)
                if fit:speed.append(fit)
    speed.sort(key=lambda m:m['rmse'])
    for m in speed[:8]:m['holdout']=surge_score(valid,m)
    # Large-angle steering excitation across all observable low-speed maneuvers.
    yaw_windows=[('16:38:00','16:42:00'),('16:45:05','16:49:09'),('16:49:09','16:53:17'),('16:57:11','16:59:11')]
    yaw_sets=[select(rows,a,b) for a,b in yaw_windows]
    yaw=[]
    for tau in (1,2,3,4,5,6,8):
        for delay in (0,.1,.3,.5,1):
            for deadband in (0,1,2,3):
                sets=[]
                for s in yaw_sets[:3]:
                    # Preserve inputs during propulsion: yaw model is an average
                    # effective low-speed model, not independent motor mechanics.
                    f=yaw_features(s,tau,delay,deadband);sets.extend(f)
                zz=sum(v[0]**2 for v in sets);bb=sum(v[1]**2 for v in sets);zb=sum(v[0]*v[1] for v in sets)
                zy=sum(v[0]*v[2] for v in sets);by=sum(v[1]*v[2] for v in sets);det=zz*bb-zb*zb
                if det<=0:continue
                gain=(zy*bb-by*zb)/det;bias=(by*zz-zy*zb)/det
                if gain<=0:continue
                m=dict(tau=tau,delay=delay,deadband=deadband,gain=gain,bias=bias,n=len(sets))
                m['rmse']=math.sqrt(sum((gain*z+bias*b-y)**2 for z,b,y,*rest in sets)/len(sets))
                yaw.append(m)
    yaw.sort(key=lambda m:m['rmse'])
    for m in yaw[:8]:m['holdout']=yaw_score(yaw_sets[3],m)
    result=dict(speed_train_rows=len(train),speed_validation_rows=len(valid),speed_top_fits=speed[:8],
                yaw_training_windows=yaw_windows[:3],yaw_holdout_window=yaw_windows[3],yaw_top_fits=yaw[:8])
    (ROOT/'full_model_fit.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
