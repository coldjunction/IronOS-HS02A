import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hs02sim import *
PLANTS = {'lag0.6': dict(sensor_tau=0.6), 'lag0.8': dict(sensor_tau=0.8), 'lag1.0': dict(sensor_tau=1.0), 'lag0.6+light': dict(sensor_tau=0.6, c_heater=0.1),
          'lag0.8+loose': dict(sensor_tau=0.8, r_heater_tip=2.5), 'lag0.8+heavy': dict(sensor_tau=0.8, c_tip=1.2), 'nominal': {}}
T = (25.0, 35.0)
def ev(mk, lim, sp=320):
    out = []
    for name, kw in PLANTS.items():
        rows = run(mk(lim), Tip(**kw), seconds=50, setpoint=sp, touch=T, dt=0.001)
        m = metrics(rows, setpoint=sp, touch=T)
        out.append((m['peak'] - sp, m['heatup'], m['idle_swing_c'], m['idle_err'], m['end_err'], m['after_peak'] - sp, m['idle_swing_w']))
    return out
def line(label, mk):
    for lim in (100, 60):
        r = ev(mk, lim)
        print(f"{label:30s} {lim:3d}W over max {max(x[0] for x in r):5.1f} (per plant {' '.join(f'{x[0]:4.1f}' for x in r)}) heat {max(x[1] for x in r):4.1f}s swingC {max(x[2] for x in r):4.1f} swingW {max(x[6] for x in r):4.1f} idleErr {max(abs(x[3]) for x in r):3.1f} jointEnd {min(x[4] for x in r):5.1f} after {max(x[5] for x in r):4.1f}")
    sys.stdout.flush()
print('plants:', ' '.join(PLANTS))
specs = sys.argv[1:]
for sp in specs:
    kp, ki, kd, isc, fine, iz, df = (int(x) for x in (sp.split('/') + ['1'])[:7])
    line(sp, lambda lim, a=(kp, ki, kd, isc, fine, iz, df): IronOSPID(a[0], a[1], a[2], iscale=a[3], limit_w=lim, fine=bool(a[4]), noise=0.7, izone=a[5], d_filter=a[6]))
