"""HS-02A closed-loop simulator: stock control law vs the IronOS port's PID.

Tip and joint models are the ones the IronOS port's author uses in
tools/hs02-stock-emu/control_sim.py (assumed, not measured). The stock law is the one
decoded from APP_HS_02A_V2.2.0 (function 0x08009fb0). The PID is a line-for-line port of
Core/Threads/PIDThread.cpp (integer maths, conditional anti-windup).
"""
import random

CURVE = ((0, 0), (223, 140), (393, 240), (572, 340), (770, 440))  # ADC count, curve C


def count_for(tip_c):
    c = max(tip_c - 25.0, 0.0)
    for (c0, t0), (c1, t1) in zip(CURVE, CURVE[1:]):
        if c <= t1:
            break
    return c0 + (c - t0) * (c1 - c0) / (t1 - t0)


def temp_for(count):
    for (c0, t0), (c1, t1) in zip(CURVE, CURVE[1:]):
        if count <= c1:
            break
    return 25.0 + t0 + (count - c0) * (t1 - t0) / (c1 - c0)


def cdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


class Tip:
    def __init__(self, c_heater=0.2, c_tip=0.7, r_heater_tip=1.5, r_tip_air=36.0, ohms=3.0,
                 sensor_tau=0.0, ambient=25.0):
        self.c_heater, self.c_tip = c_heater, c_tip
        self.r_heater_tip, self.r_tip_air = r_heater_tip, r_tip_air
        self.ohms, self.ambient, self.sensor_tau = ohms, ambient, sensor_tau
        self.heater = self.body = self.sensor = ambient
        self.work = None
        self.touching = False
        self.load = 0.0
        self.drawn = 0.0

    def step(self, dt, volts, duty):
        power = volts * volts / self.ohms * duty
        flow = (self.heater - self.body) / self.r_heater_tip
        self.drawn = self.load
        if self.work:
            self.drawn += self.work.step(dt, self.body if self.touching else None)
        loss = (self.body - self.ambient) / self.r_tip_air + self.drawn
        self.heater += dt * (power - flow) / self.c_heater
        self.body += dt * (flow - loss) / self.c_tip
        if self.sensor_tau > 0:
            self.sensor += dt * (self.heater - self.sensor) / self.sensor_tau
        else:
            self.sensor = self.heater
        return power


class Workpiece:
    def __init__(self, c=1.2, r_contact=3.0, r_air=20.0, solder_j=10.0, melt_c=183.0, ambient=25.0):
        self.c, self.r_contact, self.r_air = c, r_contact, r_air
        self.solder_j, self.melt_c, self.ambient = solder_j, melt_c, ambient
        self.temp = ambient
        self.melted_j = 0.0

    def step(self, dt, tip_c):
        taken = (tip_c - self.temp) / self.r_contact if tip_c is not None else 0.0
        energy = (taken - (self.temp - self.ambient) / self.r_air) * dt
        melting = energy > 0 and self.temp >= self.melt_c and self.melted_j < self.solder_j
        freezing = energy < 0 and self.temp <= self.melt_c and self.melted_j > 0
        if melting or freezing:
            self.melted_j = min(max(self.melted_j + energy, 0.0), self.solder_j)
        else:
            self.temp += energy / self.c
        return taken


class IronOSPID:
    """Core/Threads/PIDThread.cpp on Core/BSP/Fnirsi: 20 Hz frame, 8-frame ADC average.

    fine=False: as shipped, the PID sees whole degrees C.
    fine=True : proposed change, the PID sees the 0.1 C reading it already has.
    d_filter  : proposed change, derivative taken over this many frames (1 = as shipped).
    """
    FRAME = 0.05
    HEAT = 395 / 400

    def __init__(self, kp, ki, kd, iscale=30, limit_w=60, r_x10=25, fine=False, d_filter=1,
                 noise=0.0, seed=1, izone=0):
        self.izone = izone
        self.kp, self.ki, self.kd, self.iscale = kp, ki, kd, iscale
        self.limit_w, self.r_x10, self.fine, self.d_filter = limit_w, r_x10, fine, d_filter
        self.noise = noise
        self.rng = random.Random(seed)
        self.prev = [0] * d_filter
        self.sum = 0
        self.hist = [None] * 8
        self.slot = 0
        self.duty = 0.0
        self.req_w = 0.0

    def sample(self, sensor_c):
        # four 12-bit conversions summed (x2 in firmware: "x8 oversampling")
        true = count_for(sensor_c)
        total = sum(min(4095, max(0, round(true + self.rng.gauss(0, self.noise)))) for _ in range(4))
        if self.hist[0] is None:
            self.hist = [total] * 8
        self.hist[self.slot] = total
        self.slot = (self.slot + 1) % 8
        avg_count = sum(self.hist) / 8 / 4
        return int(temp_for(avg_count) * 10)  # degrees C x10, as getTipInCx10()

    def update(self, setpoint, reading_x10, volts):
        v10 = int(volts * 10)
        avail = (v10 * v10 // self.r_x10) * 395 // 400
        max_out = min(avail, self.limit_w * 10, 1000)
        ms = 50
        s = 10 if self.fine else 1
        reading = reading_x10 if self.fine else reading_x10 // 10
        delta = setpoint * s - reading
        kp = cdiv(self.kp * delta, s)
        ki = cdiv(self.sum, 100)
        deriv = delta - self.prev[0]
        kd = cdiv(self.kd * deriv, ms * s * self.d_filter)
        out = kp + ki + kd
        sat_hi = out >= max_out and delta > 0
        sat_lo = out <= 0 and delta < 0
        in_zone = self.izone == 0 or abs(delta) <= self.izone * s
        if not sat_hi and not sat_lo and in_zone:
            self.sum += cdiv(cdiv(delta * self.ki, s) * ms, 1000)
        lim = self.iscale * max_out
        self.sum = max(-lim, min(lim, self.sum))
        out = max(0, min(max_out, out))
        self.prev = self.prev[1:] + [delta]
        if setpoint < 10:
            out = 0
        pwm = min(395, 395 * out // avail) if out > 0 else 0
        self.duty = (pwm * 127 // 394) / 128
        self.req_w = out / 10


class Stock:
    """Decoded from APP_HS_02A_V2.2.0: 10 ms period, 40-sample average, P-only law."""
    FRAME = 0.01
    HEAT = 0.98

    def __init__(self, power=65, noise=0.0, seed=1):
        self.power = power
        self.noise = noise
        self.rng = random.Random(seed)
        self.hist = None
        self.slot = 0
        self.e1 = self.e2 = 0.0
        self.soft = 0
        self.duty = 0.0
        self.req_w = 0.0

    def sample(self, sensor_c):
        count = min(4095, max(0, round(count_for(sensor_c) + self.rng.gauss(0, self.noise))))
        t = temp_for(count)
        if self.hist is None:
            self.hist = [t] * 40
        self.hist[self.slot] = t
        self.slot = (self.slot + 1) % 40
        return sum(self.hist) / 40

    def update(self, setpoint, avg_c, volts):
        tier = 20 if volts > 16 else 15 if volts > 13 else 12 if volts > 10 else 9 if volts > 6 else 5
        e = setpoint - avg_c
        if e > 100:
            e, out = 100.0, 100.0
        elif e < -20:
            e, out = -20.0, 0.0
        else:
            out = 2.38 * (e - self.e1) + 0.81 * e + 0.28 * (e - 2 * self.e1 + self.e2)
            out *= {9: 4.5, 12: 3.0, 15: 2.0}.get(tier, 1.0)
        self.e2, self.e1 = self.e1, e
        if tier == 20:
            cap = self.power * (275 if self.power <= 65 else 340) / 400
        else:
            cap = self.power * 325 / tier / tier
        cap = min(int(cap), 100)
        if self.soft < 40:
            ceiling = (15, 22, 30, 37)[self.soft // 10]
            out = min(out, ceiling)
            self.soft += 1
        out = max(0.0, min(out, cap))
        self.duty = int(out) / 101
        self.req_w = 0.0


def run(ctrl, tip=None, seconds=30.0, setpoint=350, volts=20.0, touch=(12.0, 22.0), load=None,
        dt=0.0005):
    tip = tip or Tip()
    if touch:
        tip.work = Workpiece()
    rows = []
    frames = round(seconds / ctrl.FRAME)
    steps = max(1, round(ctrl.FRAME / dt))
    h = ctrl.FRAME / steps
    heat_steps = round(steps * ctrl.HEAT)
    for i in range(frames):
        t = i * ctrl.FRAME
        tip.touching = bool(touch) and touch[0] <= t < touch[1]
        tip.load = load[2] if load and load[0] <= t < load[1] else 0.0
        energy = 0.0
        for k in range(steps):
            energy += tip.step(h, volts, ctrl.duty if k < heat_steps else 0.0) * h
        reading = ctrl.sample(tip.sensor)
        ctrl.update(setpoint, reading, volts)
        rows.append(dict(t=t, sensor=tip.heater, body=tip.body, watts=energy / ctrl.FRAME,
                         work=tip.work.temp if tip.work else 25.0, drawn=tip.drawn))
    return rows


def metrics(rows, setpoint=350, touch=(12.0, 22.0)):
    def first(key, level, start=0.0):
        for r in rows:
            if r["t"] >= start and r[key] >= level:
                return r["t"] - start
        return float("nan")

    def sel(a, b):
        return [r for r in rows if a <= r["t"] < b]

    def mean(key, a, b):
        v = [r[key] for r in sel(a, b)]
        return sum(v) / len(v)

    t0 = touch[0] if touch else rows[-1]["t"]
    idle = sel(t0 - 3, t0)
    m = dict(
        heatup=first("sensor", setpoint - 10),
        peak=max(r["sensor"] for r in sel(0, t0)),
        idle_err=mean("sensor", t0 - 3, t0) - setpoint,
        idle_body=mean("body", t0 - 3, t0),
        idle_swing_c=max(r["sensor"] for r in idle) - min(r["sensor"] for r in idle),
        idle_swing_w=max(r["watts"] for r in idle) - min(r["watts"] for r in idle),
    )
    if touch:
        a, b = touch
        m.update(
            melt=first("work", 183, a),
            flow=first("work", 220, a),
            min_body=min(r["body"] for r in sel(a, b)),
            w3=mean("watts", a, a + 3),
            end_err=mean("sensor", b - 1, b) - setpoint,
            end_body=mean("body", b - 1, b),
            after_peak=max(r["sensor"] for r in rows if r["t"] >= b),
            final_err=mean("sensor", rows[-1]["t"] - 2, rows[-1]["t"] + 1) - setpoint,
        )
    return m


COLS = ("heatup", "peak", "idle_err", "idle_swing_c", "idle_swing_w", "flow", "min_body", "w3",
        "end_err", "after_peak", "final_err")


def show(label, m):
    print(f"{label:34s}" + " ".join(f"{m.get(c, float('nan')):8.1f}" for c in COLS))


def header():
    print(" " * 34 + " ".join(f"{c[:8]:>8s}" for c in COLS))
