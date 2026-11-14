"""Two-stage PID study: "approach" gains while heating up, "hold" gains once the tip has arrived.
The hold stage starts when the reading is within ARRIVE of the set point and ends when it is
more than LEAVE below it (a new heat-up, boost) or above it.
Usage: sweep5.py kp/kd/kpHold/kdHold/arrive/leave ...   (KI 700, limit scale 60, zone 30, gate 4)"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import io
_out = sys.stdout
sys.stdout = io.StringIO()  # sweep3 prints when imported
_argv = sys.argv
sys.argv = [sys.argv[0]]
from hs02sim import *
from sweep3 import PLANTS, boost_run, first
sys.argv = _argv
sys.stdout = _out


class TwoStage(IronOSPID):
    def __init__(self, kp, kd, kp_hold, kd_hold, arrive, leave, settle=1, limit_w=100, noise=0.7):
        super().__init__(kp, 700, kd, iscale=60, limit_w=limit_w, fine=True, noise=noise, izone=30)
        self.kp_hold, self.kd_hold, self.arrive, self.leave = kp_hold, kd_hold, arrive * 10, leave * 10
        self.hold = False
        self.last_sp = None
        self.settle, self.inband = settle, 0

    def update(self, setpoint, reading_x10, volts):
        v10 = int(volts * 10)
        avail = (v10 * v10 // self.r_x10) * 395 // 400
        max_out = min(avail, self.limit_w * 10, 1000)
        ms, s = 50, 10
        delta = setpoint * s - reading_x10
        if setpoint != self.last_sp:
            self.last_sp, self.hold = setpoint, False
        self.inband = self.inband + 1 if abs(delta) <= self.arrive else 0
        if not self.hold and self.inband >= self.settle:
            self.hold = True
        elif self.hold and abs(delta) > self.leave:
            self.hold = False
        kp_now, kd_now = (self.kp_hold, self.kd_hold) if self.hold else (self.kp, self.kd)
        kp = cdiv(kp_now * delta, s)
        ki = cdiv(self.sum, 100)
        deriv = delta - self.prev[0]
        kd = cdiv(kd_now * deriv, ms * s)
        out = kp + ki + kd
        sat_hi = out >= max_out and delta > 0
        sat_lo = out <= 0 and delta < 0
        in_zone = abs(delta) <= self.izone * s
        closing = -deriv if delta > 0 else deriv
        if not sat_hi and not sat_lo and in_zone and closing <= 4:
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


print("plants:", " ".join(PLANTS))
for spec in sys.argv[1:]:
    a = [int(x) for x in spec.split("/")]
    mk = lambda: TwoStage(*a)
    o1, h1, o2, sw, sww, droop, rec, after = [], [], [], [], [], [], [], []
    for name, kw in PLANTS.items():
        r = boost_run(mk(), Tip(**kw))
        o1.append(max(s for t, s, w in r if t < 25) - 320)
        h1.append(first(r, 310, 0))
        o2.append(max(s for t, s, w in r if t >= 25) - 450)
        idle = [(s, w) for t, s, w in r if 18 <= t < 25]
        sw.append(max(s for s, w in idle) - min(s for s, w in idle))
        sww.append(max(w for s, w in idle) - min(w for s, w in idle))
        rows = run(mk(), Tip(**kw), seconds=50, setpoint=320, touch=(25.0, 35.0), dt=0.001)
        during = [x["sensor"] for x in rows if 25 <= x["t"] < 35]
        droop.append(320 - min(during))
        back = [x["t"] for x in rows if 25 <= x["t"] < 35 and x["sensor"] < 317]
        rec.append((max(back) - 25) if back else 0.0)
        after.append(max(x["sensor"] for x in rows if x["t"] >= 35) - 320)
    f = lambda v: " ".join("%4.1f" % x for x in v)
    print("%-24s over320 %4.1f (%s) to310 %4.1fs boost %4.1f idle swing %3.1fC %4.1fW | joint droop %4.1f (%s) below-3C %4.1fs, after joint +%4.1f"
          % (spec, max(o1), f(o1), max(h1), max(o2), max(sw), max(sww), max(droop), f(droop), max(rec), max(after)))
    sys.stdout.flush()
