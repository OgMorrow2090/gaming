#!/usr/bin/env python3
"""Switch LG TV to bazzite HDMI input when the Steam Controller wakes from sleep.

Uses WebOS SSAP API over the local network instead of CEC.
Silence-detection on hidraw: sensor/IMU reports stop when the controller
sleeps and resume when it wakes. Only triggers after data has been seen
at least once (eliminates false triggers on boot/gamescope start).
"""
import asyncio, os, select, subprocess, time, syslog
from aiowebostv import WebOsClient
from wakeonlan import send_magic_packet

TV_IP = "172.16.100.44"
TV_MAC = "6c:15:db:8d:a5:a6"
TV_CLIENT_KEY = "2c37f5beb73ad7247415b32263c9501f"
TV_INPUT = "HDMI_2"

HIDRAW = "/dev/hidraw0"
# Silence threshold was 30s for the original CEC version, which assumed the
# controller would be fully asleep (no data at all) before a wake event.
# In practice the Steam Controller's IMU rate-limits to ~2-3 reports/sec when
# held but stationary, so 30s of NO data only happens if the controller is
# physically set down and inactive — useless for "press Steam button while
# holding it" which is the actual workflow. Drop to 3s so a brief stillness
# (typical between user actions) is enough to arm the gate, while continuous
# active use (still-moving IMU) still suppresses false fires. COOLDOWN (60s)
# prevents repeat fires within a window.
SILENCE_THRESHOLD = 3
COOLDOWN = 60

# Restart steamwebhelper only if the oldest one has been alive longer than
# this. The CEF black-screen overlay only appears after the Steam helper has
# accumulated render state over a long session ("when bazzite used a while"),
# so killing it after every Steam-button wake is overkill — does nothing
# useful on short sessions and disturbs the BPM UI for ~2 seconds while Steam
# auto-respawns it. 3 hours is the threshold where the overlay starts showing
# in practice; tune here if it needs to be tighter or looser. Set to 0 to
# always restart (legacy behaviour); set very high to never restart.
#
# Real framebuffer-pixel "is the screen black" detection would be preferable,
# but gamescope holds DRM master and locks out kmsgrab (even via sudo) and no
# screenshot tools (grim / wf-recorder / gpu-screen-recorder) are installed
# in the bazzite session — so we use this process-uptime heuristic instead,
# which correlates well with the underlying CEF state-bloat cause.
SWH_RESTART_THRESHOLD = 3 * 60 * 60   # 3 hours, in seconds


def steamwebhelper_uptime_secs():
    """Return the oldest steamwebhelper process's age in seconds, or 0 if
    none found. Reads from `ps -eo etimes,comm` — etimes is elapsed-seconds
    since the process started, the most stable signal across reboots."""
    try:
        out = subprocess.run(
            ["ps", "-eo", "etimes=,comm="],
            capture_output=True, text=True, timeout=3,
        ).stdout
    except Exception:
        return 0
    max_age = 0
    for line in out.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) >= 2 and parts[1] == "steamwebhelper":
            try:
                age = int(parts[0])
                if age > max_age:
                    max_age = age
            except ValueError:
                pass
    return max_age

async def switch_tv():
    send_magic_packet(TV_MAC)
    for attempt in range(4):
        wait = [3, 5, 8, 12][attempt]
        await asyncio.sleep(wait)
        try:
            client = WebOsClient(TV_IP, TV_CLIENT_KEY)
            await asyncio.wait_for(client.connect(), timeout=10)
            await client.set_input(TV_INPUT)
            await client.disconnect()
            syslog.syslog(syslog.LOG_INFO,
                          f"Steam Controller wake — switched TV to {TV_INPUT} via WebOS (attempt {attempt + 1})")
            await asyncio.sleep(3)

            # Conditional CEF-overlay clear: only restart steamwebhelper if it
            # has been alive long enough that the black-screen overlay is
            # plausibly accumulating. Short sessions skip it.
            age = steamwebhelper_uptime_secs()
            age_h, age_m = age // 3600, (age % 3600) // 60
            thr_h = SWH_RESTART_THRESHOLD // 3600
            if age >= SWH_RESTART_THRESHOLD:
                subprocess.run(["pkill", "steamwebhelper"],
                               capture_output=True, timeout=5)
                syslog.syslog(syslog.LOG_INFO,
                    f"steamwebhelper alive {age_h}h{age_m}m "
                    f"(>= {thr_h}h threshold) — restarted to clear "
                    f"accumulated CEF overlay state")
            elif age > 0:
                syslog.syslog(syslog.LOG_INFO,
                    f"steamwebhelper alive {age_h}h{age_m}m "
                    f"(< {thr_h}h threshold) — skipping restart")
            else:
                syslog.syslog(syslog.LOG_INFO,
                    "no steamwebhelper process found — nothing to restart")
            return
        except Exception as e:
            syslog.syslog(syslog.LOG_WARNING,
                          f"WebOS attempt {attempt + 1}/4 failed: {e}")
            if attempt < 3:
                send_magic_packet(TV_MAC)
    syslog.syslog(syslog.LOG_ERR, "WebOS switch failed after 4 attempts")

last_data = time.monotonic()
last_trigger = 0.0
# Init both True so the very first data after a fresh boot fires the switch.
# The original gate required data → silence → data, but on a fresh boot with a
# sleeping controller there is no "first data" to arm silence detection — so
# the first wake was always swallowed. last_trigger stays 0.0 until that first
# fire, so COOLDOWN is satisfied trivially on the initial event.
ever_seen_data = True
was_silent = True

while True:
    try:
        fd = os.open(HIDRAW, os.O_RDONLY)
        try:
            while True:
                ready, _, _ = select.select([fd], [], [], 1.0)
                now = time.monotonic()

                if ready:
                    os.read(fd, 64)
                    last_data = now

                    if (was_silent
                            and ever_seen_data
                            and now - last_trigger > COOLDOWN):
                        asyncio.run(switch_tv())
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
