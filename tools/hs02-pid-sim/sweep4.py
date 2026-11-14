"""PID study for a proportional gain of 40 (4 W per degree): overshoot, heat-up time, idle
stability and behaviour on a joint, over the family of tip models, at a 100 W limit.
Usage: sweep4.py kp/ki/kd/iscale/izone/rate ...   (rate in 0.1 C per 50 ms frame, 0 = no gate)"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hs02sim import *
from sweep3 import Gated, PLANTS, boost_run, first

LIMIT = 100
print("plants:", " ".join(PLANTS))
for spec in sys.argv[1:]:
    kp, ki, kd, isc, iz, rate = (int(x) for x in spec.split("/"))
    mk = lambda: Gated(kp, ki, kd, iscale=isc, limit_w=LIMIT, fine=True, noise=0.7, izone=iz, irate=rate)
    o1, h1, o2, sw, sww, droop, rec = [], [], [], [], [], [], []
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
        back = [x["t"] for x in rows if x["t"] >= 25 and x["sensor"] < 317]
        rec.append((max(back) - 25) if back else 0.0)
    f = lambda v: " ".join("%4.1f" % x for x in v)
    print("%-22s over320 %4.1f (%s) to310 %4.1fs boost %4.1f (%s) idle swing %3.1fC %4.1fW | joint: droop %4.1f (%s) below-3C for %4.1fs"
          % (spec, max(o1), f(o1), max(h1), max(o2), f(o2), max(sw), max(sww), max(droop), f(droop), max(rec)))
    sys.stdout.flush()
