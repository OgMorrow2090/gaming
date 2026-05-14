#!/usr/bin/env python3
"""CEC wake TV when the Steam Controller wakes from sleep.

Detects the transition from silence (controller sleeping) to data flowing
(controller awake) on hidraw. Steam exclusively grabs button data, so we
can only observe sensor/IMU reports (0x7b) — but those stop when the
controller sleeps and resume when it wakes.

Only triggers after data has been seen at least once — eliminates false
triggers on boot/gamescope start where no prior data baseline exists.

Does NOT trigger on:
- Gamescope restarts (data keeps flowing — no silence gap)
- System boot (no prior data seen)
- Idle sensor noise (already awake — no silence gap)
"""
import os, select, subprocess, time, syslog

HIDRAW = "/dev/hidraw0"
SILENCE_THRESHOLD = 30   # seconds without data = controller asleep
COOLDOWN = 60             # min seconds between CEC sends

def send_cec():
    try:
        subprocess.run(["cec-ctl", "-d", "/dev/cec0", "--playback",
                         "--image-view-on", "-t", "0", "-s"],
                       capture_output=True, timeout=5)
        time.sleep(0.5)
        subprocess.run(["cec-ctl", "-d", "/dev/cec0", "--playback",
                         "--active-source", "phys-addr=1.0.0.0", "-s"],
                       capture_output=True, timeout=5)
    except Exception:
        pass
    syslog.syslog(syslog.LOG_INFO,
                  "Steam Controller wake detected — sent CEC wake + active source")

last_data = time.monotonic()
last_trigger = 0.0
ever_seen_data = False
was_silent = False

while True:
    try:
        fd = os.open(HIDRAW, os.O_RDONLY)
        try:
            while True:
                ready, _, _ = select.select([fd], [], [], 1.0)
                now = time.monotonic()

                if ready:
                    data = os.read(fd, 64)
                    last_data = now

                    if (was_silent
                            and ever_seen_data
                            and now - last_trigger > COOLDOWN):
                        send_cec()
                        last_trigger = now
                    was_silent = False
                    ever_seen_data = True
                else:
                    if ever_seen_data and now - last_data > SILENCE_THRESHOLD:
                        was_silent = True
        except OSError:
            pass
        finally:
            os.close(fd)
    except OSError:
        if ever_seen_data:
            was_silent = True
    time.sleep(2)
