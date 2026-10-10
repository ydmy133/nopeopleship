"""Compare conservative speed PI candidates under explicit model uncertainty.

The full-log speed fit fails validation: these are engineering test candidates,
NOT parameters calibrated from an accepted physical speed model.
"""
import itertools
import json
import math
from full_scan import ROOT


def clamp(v,lo,hi):return max(lo,min(hi,v))


class SpeedController:
    def __init__(self,p):
        self.p=p;self.last=None;self.desired=0.;self.integral=0.;self.error=0.;self.reset=True;self.stop=None

    def active(self,now):return self.last is not None and now-self.last<=.5+1e-8

    def limited(self,target,dt,now):
        if not self.active(now):return target
        return clamp(target,self.desired-.25*dt,self.desired+.25*dt)

    def update(self,target,speed,dt,now):
        if target<=1e-6:
            limited=self.limited(0,dt,now)
            if (self.stop is not None and now-self.stop<.3) or (abs(limited)<1e-6 and abs(speed)<=.1):
                self.stop=now;self.last=now;self.integral=self.desired=self.error=0.;self.reset=True;return 0
            self.stop=None;target=limited
        if not self.active(now):
            self.integral=self.error=0.;self.reset=True;self.desired=speed
        self.last=now
        self.desired=self.limited(target,dt,now)
        raw=self.desired-speed
        if self.reset:self.error=raw;self.reset=False
        else:self.error+=dt/(dt+1/(2*math.pi*10))*(raw-self.error)
        self.integral=clamp(self.integral+self.error*self.p['i']*dt,-self.p['imax'],self.p['imax'])
        u=clamp(.875*self.desired+self.p['p']*self.error+self.integral,0,1)
        return math.floor(u*100+.5)


OLD=dict(p=.2,i=.2,imax=1.)


def target(t):
    return 0. if t<5 or t>=65 else .37 if t<25 else .6 if t<45 else .2


def simulate(p,model,controller=None,trace=False):
    c=controller or SpeedController(p)
    dt=model['dt'];v=0.;vs=[v];us=[0.];out=[]
    hold=round(model['delay']/dt);sense=round(.2/dt)
    a=math.exp(-dt/model['tau']);iae=0.;peak=.0;utv=0.;prevu=0.;settled=[]
    for k in range(round(85/dt)):
        t=(k+1)*dt;desired=target(t)
        measured=vs[max(0,len(vs)-1-sense)]+.02*math.sin(t*3.7)
        u=c.update(desired,measured,dt,t)
        us.append(u);applied=us[max(0,len(us)-1-hold)]
        v=a*v+(1-a)*(model['gain']*max(applied/100-model['deadband'],0)+model['bias'])
        vs.append(v);iae+=abs(desired-v)*dt;utv+=abs(u-prevu);prevu=u
        # Exclude normal inertia in the first 3 s following each target change.
        if (8<=t<25) or (28<=t<45) or (48<=t<65):peak=max(peak,v-desired)
        if 60<=t<65:settled.append(abs(desired-v))
        if trace:out.append(dict(t=t,target=desired,speed=v,output=u))
    return dict(iae=iae,peak_overspeed_mps=peak,last_hold_mean_error=sum(settled)/len(settled),output_total_variation=utv),out


def main():
    models=[dict(tau=tau,gain=gain,delay=delay,bias=bias,deadband=db,dt=dt)
            for tau,gain,delay,bias,db,dt in itertools.product((.5,2,4,8),(1.1,2,3.6,4.5),(0,.5),(-.03,.03),(0,.1),(.1,.2))]
    candidates=[OLD]+[dict(p=p,i=i,imax=.5) for p,i in itertools.product((.2,.3,.4,.6),(.05,.08,.1,.15))]
    results=[]
    for p in candidates:
        vals=[simulate(p,m)[0] for m in models]
        result=dict(params=p,scenarios=len(vals),mean_iae=sum(v['iae'] for v in vals)/len(vals),
                    worst_iae=max(v['iae'] for v in vals),worst_overspeed=max(v['peak_overspeed_mps'] for v in vals),
                    worst_hold_error=max(v['last_hold_mean_error'] for v in vals),
                    mean_output_variation=sum(v['output_total_variation'] for v in vals)/len(vals))
        result['score']=result['mean_iae']+result['worst_iae']*.2+result['worst_overspeed']*10
        results.append(result)
    results.sort(key=lambda d:d['score'])
    result=dict(note='Uncertain model benchmark; speed physical fit not accepted.',baseline=next(d for d in results if d['params']==OLD),ranked=results)
    (ROOT/'speed_candidates.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
