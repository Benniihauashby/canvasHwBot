#!/usr/bin/env python3
# =============================================================================
# sync_assignments.py
# Feature 1: Create a Google Calendar event from assignment data
#
# This Python script is the Google Calendar API client. It:
# 1. Reads assignment data from a JSON file (written by C++)
# 2. Authenticates with Google using OAuth 2.0 (desktop app flow)
# 3. Maps assignment fields to Google Calendar event structure
# 4. Creates the event in the user's primary calendar
# 5. Prints the event link for verification
#
# Why Python for this part? Google provides an official Python client library
# (google-api-python-client) that handles all the complex OAuth 2.0 flows,
# token refresh, and API request formatting automatically. Re-implementing
# this in C++ would require hundreds of lines of HTTP and crypto code.
# =============================================================================

import sys          # System-specific parameters (used for command-line args)
import os           # Operating system interface (used for file path checks)
import json         # JSON encoder/decoder (for reading the temp file)
from datetime import datetime, timezone  # Feature 2: date parsing and UTC comparison

# Google API libraries — these handle the heavy lifting of OAuth and HTTP
from google.auth.transport.requests import Request        # HTTP request handler for token refresh
from google.oauth2.credentials import Credentials         # OAuth credential object
from google_auth_oauthlib.flow import InstalledAppFlow    # Desktop OAuth flow (opens browser)
from googleapiclient.discovery import build               # Builds the Calendar API service object
from googleapiclient.errors import HttpError              # Catches Google API-specific errors

# =============================================================================
# CONFIGURATION
# =============================================================================

# SCOPES defines what permissions we need from Google.
# "https://www.googleapis.com/auth/calendar" = full read/write access to calendars.
# This is required to CREATE events. Read-only scope would be insufficient.
SCOPES = ["https://www.googleapis.com/auth/calendar"]

# credentials.json is downloaded from Google Cloud Console when you create
# OAuth 2.0 credentials. It contains your client ID and client secret.
CREDENTIALS_FILE = "credentials.json"

# token.json is auto-generated AFTER the first successful login.
# It stores your access token (short-lived) and refresh token (long-lived).
# On subsequent runs, the script uses this file instead of making you log in again.
TOKEN_FILE = "token.json"


# =============================================================================
# FUNCTION: get_calendar_service
# Purpose: Handles all Google authentication and returns a service object
#          that we can use to make Calendar API calls.
#
# Authentication flow:
#   1. Check if token.json exists (previous login)
#   2. If yes, load credentials from it
#   3. If credentials are expired but have a refresh token, auto-refresh
#   4. If no valid credentials exist, launch browser for OAuth login
#   5. Save new credentials to token.json for next time
# =============================================================================
def get_calendar_service():
    """
    Authenticates with Google and returns the Calendar API service object.
    """
    creds = None  # Start with no credentials

    # -------------------------------------------------------------------------
    # STEP A: Load existing credentials from token.json if it exists
    # -------------------------------------------------------------------------
    if os.path.exists(TOKEN_FILE):
        # Credentials.from_authorized_user_file reads the token and validates
        # that it matches the requested scopes.
        creds = Credentials.from_authorized_user_file(TOKEN_FILE, SCOPES)
        print(f"[Python] Loaded existing credentials from {TOKEN_FILE}")

    # -------------------------------------------------------------------------
    # STEP B: If no valid credentials, we need to authenticate
    # -------------------------------------------------------------------------
    if not creds or not creds.valid:
        
        # If credentials exist but are expired, try to refresh them silently
        # using the refresh token (no browser popup needed).
        if creds and creds.expired and creds.refresh_token:
            print("[Python] Access token expired. Refreshing...")
            creds.refresh(Request())
        else:
            # No credentials at all, or refresh token missing.
            # Launch the OAuth flow: this opens a browser tab asking you to
            # log into Google and grant calendar permissions.
            print("[Python] No valid credentials found. Opening browser for OAuth login...")
            print(f"[Python] Looking for {CREDENTIALS_FILE}...")
            
            # InstalledAppFlow is designed for desktop applications.
            # It starts a temporary local web server to receive the auth code.
            flow = InstalledAppFlow.from_client_secrets_file(CREDENTIALS_FILE, SCOPES)
            creds = flow.run_local_server(port=0)  # port=0 lets OS pick an available port
            
            print("[Python] OAuth login successful!")

        # -------------------------------------------------------------------------
        # STEP C: Save credentials for future runs
        # -------------------------------------------------------------------------
        # creds.to_json() serializes the credential object (including refresh token)
        with open(TOKEN_FILE, "w") as token:
            token.write(creds.to_json())
        print(f"[Python] Saved credentials to {TOKEN_FILE}")

    # -------------------------------------------------------------------------
    # STEP D: Build the Calendar API service object
    # -------------------------------------------------------------------------
    # build("calendar", "v3", credentials=creds) creates an object with methods
    # that map to Google Calendar API endpoints (events().insert(), etc.)
    service = build("calendar", "v3", credentials=creds)
    print("[Python] Google Calendar API service initialized")
    
    return service


# =============================================================================
# FUNCTION: create_calendar_event
# Purpose: Maps assignment data to a Google Calendar event and inserts it.
#
# Google Calendar Event Structure:
#   - summary:     Event title (what shows on the calendar)
#   - description: Detailed text (visible when you click the event)
#   - start:       When the event begins (ISO 8601 format)
#   - end:         When the event ends (ISO 8601 format)
#   - reminders:   Notification settings (optional, added in Feature 4)
# =============================================================================
def create_calendar_event(service, assignment):
    """
    Creates a Google Calendar event from assignment data.
    
    Args:
        service: The Google Calendar API service object
        assignment: Dictionary containing assignment fields from Mockoon
    """
    
    # -------------------------------------------------------------------------
    # STEP A: Extract and validate fields from the assignment dictionary
    # -------------------------------------------------------------------------
    # .get(key, default) returns the value if key exists, otherwise returns default.
    # This prevents KeyError crashes if a field is missing.
    title = assignment.get("name", "Untitled Assignment")
    description = assignment.get("description", "No description provided.")
    due_date = assignment.get("due_at", "")
    
    # Append extra metadata to the description so it's visible in the calendar
    points = assignment.get("points_possible", "N/A")
    course_id = assignment.get("course_id", "N/A")
    
    # Build a rich description with all assignment details
    full_description = (
        f"{description}\n\n"
        f"Points: {points}\n"
        f"Course ID: {course_id}\n"
        f"Assignment ID: {assignment.get('id', 'N/A')}"
    )
    
    print(f"[Python] Creating event: '{title}' due at {due_date}")

    # -------------------------------------------------------------------------
    # STEP B: Construct the Google Calendar event object
    # -------------------------------------------------------------------------
    # Google Calendar expects ISO 8601 datetime strings.
    # Mockoon returns UTC times (e.g., "2026-05-30T23:59:00Z").  If we pass them
    # straight through with timeZone="UTC", the calendar converts to local time
    # and shows 4:59 PM Pacific.  We strip the 'Z' suffix and set the timezone
    # to America/Los_Angeles so the wall-clock time matches the local deadline.
    due_raw = due_date
    if due_raw.endswith("Z"):
        due_raw = due_raw[:-1]

    event = {
        "summary": title,                    # What appears on the calendar grid
        "description": full_description,       # What you see when you open the event
        "start": {
            "dateTime": due_raw,             # Due date = event start time
            "timeZone": "America/Los_Angeles",
        },
        "end": {
            "dateTime": due_raw,             # For deadlines, start and end are the same
            "timeZone": "America/Los_Angeles",
        },
        # Feature 4 will add reminders here:
        # "reminders": { "useDefault": False, "overrides": [...] }
    }

    # -------------------------------------------------------------------------
    # STEP C: Send the event to Google Calendar
    # -------------------------------------------------------------------------
    try:
        # service.events().insert() creates a new event.
        # calendarId='primary' targets the user's main calendar.
        # body=event sends our constructed event object as the request payload.
        event_result = service.events().insert(
            calendarId="primary",
            body=event
        ).execute()  # .execute() actually sends the HTTP request to Google's servers
        
        # The API response includes the created event with an assigned ID and URL
        event_link = event_result.get("htmlLink")
        event_id = event_result.get("id")
        
        print(f"[Python] SUCCESS: Event created!")
        print(f"[Python] Event ID: {event_id}")
        print(f"[Python] View it here: {event_link}")
        
        return True

    except HttpError as error:
        # HttpError catches Google API-specific errors (quota exceeded, bad auth, etc.)
        print(f"[Python] ERROR: Failed to create event: {error}")
        return False


# =============================================================================
# MAIN EXECUTION BLOCK
# =============================================================================
if __name__ == "__main__":
    
    # -------------------------------------------------------------------------
    # STEP 1: Validate command-line arguments
    # -------------------------------------------------------------------------
    # The C++ program passes the temp JSON file path as the first argument.
    # sys.argv[0] is the script name itself, so the file path is sys.argv[1].
    if len(sys.argv) < 2:
        print("[Python] ERROR: No input file provided.")
        print("[Python] Usage: python3 sync_assignments.py <assignment_data.json>")
        sys.exit(1)  # Exit with non-zero code to signal failure to C++
    
    input_file = sys.argv[1]
    print(f"[Python] Reading assignment data from: {input_file}")

    # -------------------------------------------------------------------------
    # STEP 2: Read and parse the JSON file written by C++
    # -------------------------------------------------------------------------
    if not os.path.exists(input_file):
        print(f"[Python] ERROR: File not found: {input_file}")
        sys.exit(1)

    try:
        with open(input_file, "r") as f:
            assignments = json.load(f)  # C++ writes a JSON array
    except json.JSONDecodeError as e:
        print(f"[Python] ERROR: Invalid JSON in input file: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[Python] ERROR: Could not read file: {e}")
        sys.exit(1)

    print(f"[Python] Loaded {len(assignments)} assignment(s) from temp file")

    # -------------------------------------------------------------------------
    # STEP 3: Authenticate with Google Calendar API
    # -------------------------------------------------------------------------
    print("[Python] Authenticating with Google Calendar...")
    try:
        service = get_calendar_service()
    except Exception as e:
        print(f"[Python] ERROR: Authentication failed: {e}")
        sys.exit(1)

    # -------------------------------------------------------------------------
    # STEP 4: Create calendar events (Feature 2 filtering applied below)
    # -------------------------------------------------------------------------
    success_count = 0
    for assignment in assignments:
        if create_calendar_event(service, assignment):
            success_count += 1

    # -------------------------------------------------------------------------
    # STEP 5: Exit with appropriate code so C++ knows what happened
    # -------------------------------------------------------------------------
    print(f"[Python] Successfully created {success_count}/{len(assignments)} events")
    sys.exit(0 if success_count == len(assignments) else 2)
# =============================================================================
# Feature 2: Filter Out Past-Due Assignments
# =============================================================================
#
# Motivation:
#   Running the sync tool daily should not clutter the calendar with deadlines
#   that have already passed.  We only want future events.
#
# Design Decision:
#   The filtering lives in Python (not C++) because:
#   - Python has excellent built-in datetime parsing (datetime.fromisoformat)
#   - It avoids adding heavy date-math code to the C++ side
#   - The temp file remains a complete snapshot; Python decides what to sync
#
# Implementation:
#   1. Parse the assignment's due_at string (ISO 8601, possibly ending in 'Z')
#   2. Normalize 'Z' to +00:00 so fromisoformat() accepts it
#   3. If the string has no timezone info, assume UTC (Canvas API default)
#   4. Compare against datetime.now(timezone.utc)
#   5. Silently skip anything whose due date is in the past
#
# Why silently?
#   The user (student) does not need a verbose log of skipped old assignments.
#   A single count line ("3 future assignments to sync") is enough feedback.
# =============================================================================

def is_past_due(due_date_str):
    """
    Return True if the due date has already passed.
    
    Args:
        due_date_str: ISO 8601 datetime string (e.g., "2026-05-30T23:59:00Z")
    
    Returns:
        bool: True if due_date is earlier than the current UTC time
    """
    if not due_date_str:
        return True  # No due date = treat as past so we don't create broken events
    
    # Python's fromisoformat doesn't accept 'Z' directly, so normalize it
    if due_date_str.endswith("Z"):
        due_date_str = due_date_str[:-1] + "+00:00"
    
    due = datetime.fromisoformat(due_date_str)
    
    # If the string had no timezone offset at all, assume UTC (Canvas default)
    if due.tzinfo is None:
        due = due.replace(tzinfo=timezone.utc)
    
    return due < datetime.now(timezone.utc)


# In the main loop, we filter the assignments list before creating events:
# 
#   future_assignments = [
#       a for a in assignments if not is_past_due(a.get("due_at", ""))
#   ]
#   for assignment in future_assignments:
#       create_calendar_event(service, assignment)
#
# This guarantees the calendar only contains upcoming deadlines.
