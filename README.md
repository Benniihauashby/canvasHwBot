# canvasHwBot

A student assignment tracker that fetches deadlines from Canvas, syncs them to Google Calendar, and nags you with voice reminders as due dates approach.

## What It Does

- **Fetches assignments** from the Canvas LMS API (currently using a local Mockoon server for testing)
- **Syncs to Google Calendar** with automatic timezone conversion and built-in reminders
- **Filters out stale deadlines** so only active assignments appear
- **Prevents duplicates** so re-running the tool doesn't clutter your calendar
- **Yells at you** through your laptop speakers via AI text-to-speech as deadlines get close

## Tech Stack

| Component | Technology |
|---|---|
| Fetcher | C++11, libcurl, nlohmann/json |
| Calendar Sync | Python 3, Google Calendar API |
| Voice Reminders | Python 3, ElevenLabs TTS API |
| Scheduling | cron (macOS/Linux) |

## Quick Start

### 1. Build the C++ fetcher

```bash
clang++ -std=c++11 main.cpp canvas_api.cpp -o canvas_bot -lcurl
```

### 2. Set up Python dependencies

```bash
source venv/bin/activate
pip install -r requirements.txt
```

### 3. Configure secrets

Create a `.env` file in the project root:

```
ELEVENLABS_API_KEY=your_key_here
ELEVENLABS_VOICE_ID=your_voice_id_here
```

Place your Google OAuth `credentials.json` in the project root (downloaded from Google Cloud Console).

### 4. Run

```bash
# Start Mockoon on localhost:3000 (for local testing)
# Then:
./canvas_bot
```

The tool will:
1. Fetch courses and assignments
2. Write them to a temp JSON file
3. Spawn `gCalendar.py` to sync with Google Calendar
4. Create a symlink for the voice reminder script

### 5. Enable voice reminders

Edit `reminder_config.json`:

```json
{"mode": "gentle"}
```

Valid modes: `off`, `gentle`, `moderate`, `annoying`.

Add to cron to run every 15 minutes:

```bash
crontab -e
# Add this line:
*/15 * * * * cd /path/to/project && source venv/bin/activate && python voice_reminder.py >> /tmp/voice_reminder.log 2>&1
```

## Project Structure

```
.
├── main.cpp              # C++ entry point: fetch, validate, write temp file
├── canvas_api.h          # Course/Assignment structs and CanvasAPI class
├── canvas_api.cpp        # libcurl HTTP implementation
├── gCalendar.py          # Google Calendar sync + deduplication + filtering
├── voice_reminder.py     # ElevenLabs TTS voice reminders
├── reminder_config.json  # Voice reminder mode selector
├── requirements.txt      # Python dependencies
├── .env                  # API keys (gitignored)
├── credentials.json      # Google OAuth secrets (gitignored)
├── token.json            # Google OAuth tokens (gitignored)
└── DESIGN_DOC.md         # Full architecture and feature documentation
```

## Security

- `.env`, `credentials.json`, and `token.json` are all `.gitignore`d
- No API keys or OAuth tokens are ever committed to version control
- The `.env` pattern is reusable for future Canvas API tokens

## What's Next

- Switch from Mockoon to the real Canvas API (auth code is already written and commented out)
- Update existing calendar events when due dates change
- Add course filtering so users can sync only specific classes
