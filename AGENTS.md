# canvasHwBot — Agent Guide

This document is a concise reference for AI coding agents working on this project.

---

## Project Overview

`canvasHwBot` is a student-built assignment-tracking tool that bridges the Canvas LMS API and Google Calendar. It fetches course and assignment data from Canvas and can sync assignment deadlines to a Google Calendar.

The project currently consists of two **separate, not-yet-integrated** components:

1. **C++ Canvas client** (`canvas_api.cpp`, `canvas_api.h`, `main.cpp`) — fetches courses and assignments via HTTP.
2. **Python Google Calendar sync** (`gCalendar.py`) — creates calendar events from assignment data.

Both components currently point at a **Mockoon mock server** running on `localhost:3000` for local testing.

---

## Technology Stack

- **C++** (C++11 or later)
  - `libcurl` — HTTP client
  - `nlohmann/json` (v3.12.0, header-only, bundled in `nlohmann/json.hpp`) — JSON parsing
  - Compiler: Apple Clang (tested on macOS ARM64)
- **Python** 3.11
  - `google-api-python-client`, `google-auth-oauthlib`, `requests` — Google Calendar API & HTTP
- **OS / Platform**: macOS (ARM64)
- **IDE**: VS Code (configs in `.vscode/`)

---

## Project Structure

```
.
├── main.cpp              # C++ entry point. Currently hard-codes Mockoon URL.
├── canvas_api.h          # C++ declarations: CanvasAPI class, Course & Assignment structs
├── canvas_api.cpp        # C++ implementation: libcurl HTTP GET + JSON parsing
├── canvas_bot            # Compiled C++ executable (Mach-O 64-bit ARM64)
├── gCalendar.py          # Python script: OAuth to Google Calendar, fetches mock data, creates events
├── nlohmann/json.hpp     # Single-header JSON library (do not modify)
├── DESIGN_DOC.md         # Currently empty (placeholder)
├── .vscode/
│   ├── tasks.json        # Generic VS Code clang build task (only compiles active file)
│   └── launch.json       # Empty
├── venv/                 # Python virtual environment (committed to repo)
└── .gitattributes        # LF normalization
```

There is **no Makefile, CMakeLists.txt, or build script** for the C++ code.

---

## Build & Run Commands

### C++ Component

Because there is no formal build system, compile manually. From the project root:

```bash
clang++ -std=c++11 main.cpp canvas_api.cpp -o canvas_bot -lcurl
```

Then run:

```bash
./canvas_bot
```

Requirements:
- `libcurl` must be installed and linkable (macOS usually ships with `/usr/lib/libcurl.4.dylib`).
- `curl-config` is present at `/usr/bin/curl-config` on the reference machine.

### Python Component

Activate the virtual environment (already populated):

```bash
source venv/bin/activate
python gCalendar.py
```

If you ever need to recreate the environment, the key packages are:
- `google-api-python-client`
- `google-auth-oauthlib`
- `requests`

There is **no `requirements.txt`** in the repo at this time.

### Mockoon Dependency

Both components expect a mock API at `http://localhost:3000`.
- C++: `main.cpp` constructs `CanvasAPI("http://localhost:3000")`.
- Python: `gCalendar.py` calls `http://localhost:3000/assignments`.

You must have Mockoon (or equivalent) running with the appropriate routes before testing.

---

## Code Organization

### C++ (`canvas_api.h` / `canvas_api.cpp`)

- **`struct Course`** — `id`, `name`, `course_code`
- **`struct Assignment`** — `id`, `name`, `description`, `due_at`, `points_possible`, `course_id`
- **`class CanvasAPI`**
  - Two constructors:
    1. `CanvasAPI(domain, token)` — for real Canvas API (`https://<domain>/api/v1`)
    2. `CanvasAPI(base_url)` — for Mockoon/local testing
  - `getCourses()` → `std::vector<Course>`
  - `getAssignments(int course_id)` → `std::vector<Assignment>`
  - `getAllAssignments()` → fetches all courses, then all assignments per course, and concatenates them.
  - Private `makeRequest(endpoint)` uses `libcurl` with a static `WriteCallback`.

**Note:** The real Canvas API token authorization code inside `makeRequest` is **commented out**. To use a live Canvas instance, uncomment the `Authorization: Bearer ...` block and pass a valid token.

### Python (`gCalendar.py`)

- **`get_calendar_service()`** — OAuth2 flow using `credentials.json` and caching tokens in `token.json`.
- **`fetch_assignments_from_mockoon()`** — plain `requests.get` to `localhost:3000/assignments`.
- **`add_assignment_to_calendar(service, assignment)`** — inserts a Google Calendar event with 1-day and 1-hour popup reminders.
- **`main()`** — ties the above together.

---

## Code Style Guidelines

- The codebase is informal and educational. Comments are conversational and explanatory.
- C++ uses modern basics: `auto`, range-based `for`, `const auto&`, and `try/catch` for JSON exceptions.
- Python follows procedural scripting style; no classes or modules are used.
- Spelling/grammar in existing comments is inconsistent — do not “fix” comments for style unless they become misleading.

---

## Testing Instructions

- **There are no formal unit tests, no test framework, and no CI configuration.**
- Manual end-to-end testing is done against Mockoon on `localhost:3000`.
- Expected mock endpoints:
  - `GET /api/v1/users/self/courses` — returns JSON array of course objects
  - `GET /api/v1/courses/{id}/assignments` — returns JSON array of assignment objects
  - `GET /assignments` — returns JSON array consumed by `gCalendar.py` (schema: `{ title, description, due_date }`)

If you add tests, place them in a new `tests/` directory and update this file.

---

## Security Considerations

1. **Credentials files must stay out of version control.**
   - `credentials.json` (Google OAuth client secrets)
   - `token.json` (Google OAuth access/refresh tokens)
   - Canvas API bearer tokens (passed as `std::string` in C++)

   The repo currently does **not** have a `.gitignore` file. If you add credential files locally, create a `.gitignore` immediately to prevent accidental commits.

2. **OAuth scope** in `gCalendar.py` is `https://www.googleapis.com/auth/calendar` (full read/write). This is necessary for event insertion.

3. **HTTP vs HTTPS** — The C++ `CanvasAPI` constructor for real Canvas hard-codes `https://`. The Mockoon constructor accepts plain HTTP. Do not switch the real-API path to HTTP.

4. **Token storage** — `token.json` is written to the project root with standard file permissions. On shared machines, consider restricting file permissions (`chmod 600 token.json`).

---

## Known Gaps & Quick Wins

- `DESIGN_DOC.md` is empty.
- `.vscode/launch.json` is empty — no debug configuration is set up.
- VS Code `tasks.json` only compiles the active file; it will fail for the multi-file C++ project. Update it to build `main.cpp` + `canvas_api.cpp` together.
- No `.gitignore` — `.DS_Store`, `venv/`, `token.json`, and `credentials.json` should be ignored.
- No `requirements.txt` for Python.
- The C++ and Python components are not wired together (e.g., the C++ program does not invoke the Python script, nor do they share a data file or socket).
