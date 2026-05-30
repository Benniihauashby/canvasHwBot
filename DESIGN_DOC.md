# canvasHwBot — Design Document

## 1. Overview

`canvasHwBot` is a student assignment tracker that bridges the Canvas LMS API and Google Calendar. It fetches course and assignment data, syncs deadlines to a Google Calendar, and uses voice reminders to nag the user as deadlines approach.

## 2. Architecture

```
┌──────────────┐     ┌──────────────┐     ┌──────────────────┐
│  C++ Fetcher │────▶│  Temp JSON   │────▶│  Python Syncer   │
│ (libcurl +   │     │  (/tmp/...)  │     │ (Google Calendar │
│  nlohmann)   │     │              │     │  API client)     │
└──────────────┘     └──────────────┘     └──────────────────┘
       │                                          │
       │                                          ▼
       │                                   ┌──────────────┐
       │                                   │ Google       │
       │                                   │ Calendar     │
       │                                   └──────────────┘
       ▼
┌──────────────┐
│ Stable       │
│ Symlink      │
└──────────────┘
       │
       ▼
┌──────────────┐
│ Voice        │
│ Reminder     │
│ (ElevenLabs) │
└──────────────┘
```

**Design principle:** C++ handles fetching and validation; Python handles sync and notifications. This separation keeps network/JSON logic in one language and API integrations in another.

## 3. Feature 1: C++ to Python Pipeline

**Responsibility:** Fetch assignment data and hand it off to Python.

**Implementation:**
- `CanvasAPI` (C++) uses `libcurl` to make HTTP GET requests.
- `getCourses()` returns a list of `Course` structs.
- `getAllAssignments()` iterates through courses and concatenates all assignments.
- `main.cpp` serializes the assignment array to JSON using `nlohmann::json`.
- The JSON is written to a unique temp file (`/tmp/canvas_assignments_{PID}.json`).
- A stable symlink (`/tmp/canvas_assignments_latest.json`) is created for the voice reminder script.
- `std::system()` spawns `gCalendar.py` with the temp file path as an argument.

**Why a temp file instead of a socket or pipe?**
- A file is stateless and debuggable — you can `cat` it to inspect what C++ produced.
- It decouples the C++ and Python lifecycles. Python can be run manually without recompiling C++.

## 4. Feature 2: Past-Due Filtering

**Responsibility:** Prevent stale deadlines from cluttering the calendar.

**Implementation:**
- `gCalendar.py` parses each assignment's `due_at` field (ISO 8601).
- Dates are normalized to UTC and compared against the **start of today** (midnight UTC).
- Assignments due *today* are still synced; only assignments from *yesterday or earlier* are skipped.
- This keeps the calendar relevant without being overly aggressive.

**Why in Python and not C++?**
- Python's `datetime.fromisoformat()` handles timezone parsing cleanly.
- Keeping the temp file as a complete snapshot means C++ doesn't need to know about calendar logic.

## 5. Feature 3: Deduplication

**Responsibility:** Prevent duplicate calendar events when the tool is run multiple times.

**Implementation:**
- When creating an event, `gCalendar.py` embeds the Canvas assignment ID in the event's `extendedProperties.private` metadata.
- Before inserting a new event, it queries Google Calendar for events with `privateExtendedProperty=canvasAssignmentId={id}`.
- If a match is found, the assignment is skipped with a console message.
- If the user manually deletes the event from Google Calendar, the next run will recreate it (the query returns zero results).

**Why `extendedProperties` instead of title matching?**
- Professors sometimes rename assignments. Titles change; IDs don't.
- Searching by title requires fetching many events and doing string comparison, which is slower and less reliable.

## 6. Feature 4: Voice Reminders

**Responsibility:** Nag the user through their laptop speakers as deadlines approach.

**Implementation:**
- `voice_reminder.py` is a standalone script that reads the stable symlink.
- It calculates hours-until-deadline for each assignment.
- Assignments are grouped into buckets: `>24h` (silent), `4–24h`, `1–4h`, `<1h`.
- A `reminder_state.json` file tracks the last reminder time and bucket for each assignment ID.
- Reminder modes:
  - `off` — no reminders
  - `gentle` — remind once per bucket transition
  - `moderate` — gentle + every 15 minutes inside the current bucket
  - `annoying` — gentle + every 5 minutes inside the current bucket
- Text is sent to the ElevenLabs API (`eleven_flash_v2_5` model) and played via `afplay`.
- The script is scheduled via cron (`*/15 * * * *`).

**Why a separate script instead of bundling into `gCalendar.py`?**
- `gCalendar.py` only runs when the user manually executes `./canvas_bot`.
- Voice reminders need to fire throughout the day on a fixed schedule.
- Separation of concerns: calendar sync code stays clean.

## 7. Security

| Secret | Storage | GitHub Safe? |
|---|---|---|
| Google OAuth client secrets | `credentials.json` | ✅ `.gitignore`d |
| Google OAuth tokens | `token.json` | ✅ `.gitignore`d |
| ElevenLabs API key | `.env` | ✅ `.gitignore`d |

**Principle:** No credential file is ever committed. The `.env` pattern will be reused for the Canvas API bearer token when switching from Mockoon to production.

## 8. Future Work

- **Real Canvas API integration:** The `CanvasAPI` constructor for real Canvas (`https://<domain>/api/v1`) and the Bearer token auth block in `makeRequest()` are already written and commented out. Switching requires only a URL change and an environment variable for the token.
- **Update existing events:** Currently, if a due date changes, the old event is left untouched and a new one is not created (because deduplication skips it). Replacing the skip with `events().update()` would handle date changes.
- **Course filtering:** A command-line flag to sync only specific `course_id`s would let users ignore electives or old semesters.
