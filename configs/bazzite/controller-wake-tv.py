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
SILENCE_THRESHOLD = 30
COOLDOWN = 60

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
            subprocess.run(["pkill", "steamwebhelper"],
                           capture_output=True, timeout=5)
            syslog.syslog(syslog.LOG_INFO, "Killed steamwebhelper to clear CEF overlay (Steam auto-restarts it)")
            return
        except Exception as e:
            syslog.syslog(syslog.LOG_WARNING,
                          f"WebOS attempt {attempt + 1}/4 failed: {e}")
            if attempt < 3:
                send_magic_packet(TV_MAC)
    syslog.syslog(syslog.LOG_ERR, "WebOS switch failed after 4 attempts")

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
