#!/usr/bin/env python3
"""
voice_reminder.py
Feature 4: Annoying voice reminders for upcoming deadlines

Reads the latest assignment snapshot, checks which deadlines are approaching,
and uses ElevenLabs text-to-speech to yell at you through your laptop speakers.

Modes (set in reminder_config.json):
  off      - do nothing
  gentle   - remind once per threshold window (24h, 4h, 1h)
  moderate - remind every 15 min when within 24 hours of deadline
  annoying - remind every 5 min when within 24 hours of deadline

Scheduling:
  Add to cron to run every 15 minutes:
    */15 * * * * cd /path/to/project && source venv/bin/activate && python voice_reminder.py >> /tmp/voice_reminder.log 2>&1
"""

import json
import os
import sys
from datetime import datetime, timezone

import requests
from dotenv import load_dotenv

# Load API keys from .env (never commit .env to Git!)
load_dotenv()

ELEVENLABS_API_KEY = os.getenv("ELEVENLABS_API_KEY")
ELEVENLABS_VOICE_ID = os.getenv("ELEVENLABS_VOICE_ID", "Adam")
ASSIGNMENTS_FILE = "/tmp/canvas_assignments_latest.json"
CONFIG_FILE = "reminder_config.json"
STATE_FILE = "reminder_state.json"


# ---------------------------------------------------------------------------
# Reminder buckets: assignments are grouped by how close their deadline is.
# None  = > 24 hours away (silent)
# 24h   = 4–24 hours away
# 4h    = 1–4 hours away
# 1h    = < 1 hour away
# ---------------------------------------------------------------------------
def get_bucket(hours_left):
    if hours_left > 24:
        return None
    elif hours_left > 4:
        return "24h"
    elif hours_left > 1:
        return "4h"
    else:
        return "1h"


def load_config():
    """Read the user's chosen nag mode from reminder_config.json."""
    if not os.path.exists(CONFIG_FILE):
        return {"mode": "off"}
    with open(CONFIG_FILE, "r") as f:
        return json.load(f)


def load_state():
    """Load reminder history so we don't repeat the same nag every run."""
    if not os.path.exists(STATE_FILE):
        return {}
    with open(STATE_FILE, "r") as f:
        return json.load(f)


def save_state(state):
    """Persist reminder history to disk."""
    with open(STATE_FILE, "w") as f:
        json.dump(state, f, indent=2)


def should_remind(assignment_id, hours_left, state, mode):
    """
    Decide whether it's time to nag about this assignment.

    Rules:
      - off      → never
      - gentle   → only when entering a new bucket (24h → 4h → 1h)
      - moderate → same as gentle, plus every 15 min inside the same bucket
      - annoying → same as gentle, plus every 5 min inside the same bucket
    """
    if mode == "off":
        return False

    bucket = get_bucket(hours_left)
    if bucket is None:
        return False

    now = datetime.now(timezone.utc)
    assignment_state = state.get(str(assignment_id), {})
    last_bucket = assignment_state.get("last_bucket")
    last_time_str = assignment_state.get("last_time")

    last_time = datetime.fromisoformat(last_time_str) if last_time_str else None

    # Entering a new bucket always triggers a reminder
    if bucket != last_bucket:
        return True

    # Same bucket: check interval based on mode
    if mode == "gentle":
        return False  # already reminded for this bucket

    if last_time is None:
        return True

    elapsed_mins = (now - last_time).total_seconds() / 60

    if mode == "moderate" and elapsed_mins >= 15:
        return True
    if mode == "annoying" and elapsed_mins >= 5:
        return True

    return False


def build_message(title, hours_left):
    """Craft a human-friendly nag message based on time remaining."""
    if hours_left >= 2:
        return f"Hey. Your {title} is due in {int(hours_left)} hours. Get to work."
    elif hours_left >= 1:
        return f"Hey. Your {title} is due in 1 hour. Wrap it up."
    elif hours_left > 0:
        minutes_left = int(hours_left * 60)
        return f"Hey. Your {title} is due in {minutes_left} minutes. Hurry up."
    else:
        return f"Hey. Your {title} is due right now. Submit it."


def speak(text):
    """
    Call ElevenLabs API to convert text to speech, then play it with afplay.
    Returns True on success, False on failure.
    """
    if not ELEVENLABS_API_KEY:
        print("[Voice] Error: ELEVENLABS_API_KEY not set in .env")
        return False

    url = f"https://api.elevenlabs.io/v1/text-to-speech/{ELEVENLABS_VOICE_ID}"
    headers = {
        "xi-api-key": ELEVENLABS_API_KEY,
        "Content-Type": "application/json",
    }
    body = {
        "text": text,
        "model_id": "eleven_flash_v2_5",
    }

    try:
        resp = requests.post(url, json=body, headers=headers, timeout=30)
    except requests.RequestException as e:
        print(f"[Voice] Network error calling ElevenLabs: {e}")
        return False

    if resp.status_code != 200:
        print(f"[Voice] ElevenLabs API error {resp.status_code}: {resp.text}")
        return False

    mp3_path = "/tmp/reminder.mp3"
    with open(mp3_path, "wb") as f:
        f.write(resp.content)

    # macOS built-in audio player
    exit_code = os.system(f"afplay {mp3_path}")
    if exit_code != 0:
        print("[Voice] afplay failed — could not play audio")
        return False

    return True


def main():
    config = load_config()
    mode = config.get("mode", "off")

    if mode == "off":
        print("[Voice] Reminders are off. Set mode in reminder_config.json to enable.")
        sys.exit(0)

    if not os.path.exists(ASSIGNMENTS_FILE):
        print(f"[Voice] No assignment snapshot found at {ASSIGNMENTS_FILE}")
        sys.exit(0)

    with open(ASSIGNMENTS_FILE, "r") as f:
        assignments = json.load(f)

    state = load_state()
    now = datetime.now(timezone.utc)
    reminded_count = 0

    for item in assignments:
        due_str = item.get("due_at", "")
        if not due_str:
            continue

        # Parse due date (same logic as gCalendar.py)
        if due_str.endswith("Z"):
            due_str = due_str[:-1] + "+00:00"
        try:
            due = datetime.fromisoformat(due_str)
        except ValueError:
            continue
        if due.tzinfo is None:
            due = due.replace(tzinfo=timezone.utc)

        # Only skip assignments from yesterday or earlier.
        # Assignments due today (even in a few hours) are still fair game.
        start_of_today = now.replace(hour=0, minute=0, second=0, microsecond=0)
        if due < start_of_today:
            continue

        hours_left = (due - now).total_seconds() / 3600
        assignment_id = item.get("id")

        if should_remind(assignment_id, hours_left, state, mode):
            title = item.get("name", "Assignment")
            message = build_message(title, hours_left)
            print(f"[Voice] {message}")

            if speak(message):
                reminded_count += 1
                # Update state so we don't repeat immediately
                state[str(assignment_id)] = {
                    "last_bucket": get_bucket(hours_left),
                    "last_time": now.isoformat(),
                }

    save_state(state)
    print(f"[Voice] Done. Reminded about {reminded_count} assignment(s).")


if __name__ == "__main__":
    main()
