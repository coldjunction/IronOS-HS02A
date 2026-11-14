"""Build 35 PID study: integrate only while the error is closing slowly ("rate gate").
Usage: sweep3.py kp/ki/kd/iscale/izone/rate ...   (rate in 0.1 C per 50 ms frame, 0 = no gate)"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hs02sim import *


class Gated(IronOSPID):
    def __init__(self, *a, irate=0, **kw):
        super().__init__(*a, **kw)
        self.irate = irate

    def update(self, setpoint, reading_x10, volts):
        v10 = int(volts * 10)
        avail = (v10 * v10 // self.r_x10) * 395 // 400
        max_out = min(avail, self.limit_w * 10, 1000)
        ms, s = 50, 10
        delta = setpoint * s - reading_x10
        kp = cdiv(self.kp * delta, s)
        ki = cdiv(self.sum, 100)
        deriv = delta - self.prev[0]
        kd = cdiv(self.kd * deriv, ms * s)
        out = kp + ki + kd
        sat_hi = out >= max_out and delta > 0
        sat_lo = out <= 0 and delta < 0
        in_zone = self.izone == 0 or abs(delta) <= self.izone * s
        closing = -deriv if delta > 0 else deriv
        slow = self.irate == 0 or closing <= self.irate
        if not sat_hi and not sat_lo and in_zone and slow:
            self.sum += cdiv(cdiv(delta * self.ki, s) * ms, 1000)
        lim = self.iscale * max_out
        self.sum = max(-lim, min(lim, self.sum))
        out = max(0, min(max_out, out))
        self.prev = [delta]
        if setpoint < 10:
            out = 0
        pwm = min(395, 395 * out // avail) if out > 0 else 0
        self.duty = (pwm * 127 // 394) / 128
        self.req_w = out / 10


PLANTS = {'lag0.6': dict(sensor_tau=0.6), 'lag0.8': dict(sensor_tau=0.8), 'lag1.0': dict(sensor_tau=1.0), 'light': dict(sensor_tau=0.6, c_heater=0.1),
          'loose': dict(sensor_tau=0.8, r_heater_tip=2.5), 'heavy': dict(sensor_tau=0.8, c_tip=1.2), 'nominal': {}}


def boost_run(ctrl, tip, dt=0.001):
    """25 s at 320, then 15 s at 450. Returns rows of (t, sensor, watts)."""
    rows = []
    steps = max(1, round(ctrl.FRAME / dt))
    h = ctrl.FRAME / steps
    heat_steps = round(steps * ctrl.HEAT)
    for i in range(round(40 / ctrl.FRAME)):
        t = i * ctrl.FRAME
        sp = 320 if t < 25 else 450
        e = 0.0
        for k in range(steps):
            e += tip.step(h, 20.0, ctrl.duty if k < heat_steps else 0.0) * h
        ctrl.update(sp, ctrl.sample(tip.sensor), 20.0)
        rows.append((t, tip.heater, e / ctrl.FRAME))
    return rows


def first(rows, level, a):
    for t, s, w in rows:
        if t >= a and s >= level:
            return t - a
    return float('nan')


print('plants:', ' '.join(PLANTS))
for spec in sys.argv[1:]:
    kp, ki, kd, isc, iz, rate = (int(x) for x in spec.split('/'))
    mk = lambda lim: Gated(kp, ki, kd, iscale=isc, limit_w=lim, fine=True, noise=0.7, izone=iz, irate=rate)
    for lim in (100, 60):
        o1, h1, o2, h2, sw, je = [], [], [], [], [], []
        for name, kw in PLANTS.items():
            r = boost_run(mk(lim), Tip(**kw))
            o1.append(max(s for t, s, w in r if t < 25) - 320)
            h1.append(first(r, 310, 0))
            o2.append(max(s for t, s, w in r if t >= 25) - 450)
            h2.append(first(r, 440, 25))
            idle = [s for t, s, w in r if 20 <= t < 25]
            sw.append(max(idle) - min(idle))
            rows = run(mk(lim), Tip(**kw), seconds=50, setpoint=320, touch=(25.0, 35.0), dt=0.001)
            m = metrics(rows, setpoint=320, touch=(25.0, 35.0))
            je.append(m['end_err'])
        f = lambda v: ' '.join('%4.1f' % x for x in v)
        print('%-24s %3dW  over320 max %4.1f (%s)  to310 %4.1fs  boost over max %4.1f (%s) to440 %4.1fs  swing %3.1f  jointEnd %5.1f'
              % (spec, lim, max(o1), f(o1), max(h1), max(o2), f(o2), max(h2), max(sw), min(je)))
        sys.stdout.flush()
