import datetime
import os.path
import requests

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError

# We need full read/write access to modify calendars
SCOPES = ["https://www.googleapis.com/auth/calendar"]

def get_calendar_service():
    """Handles authentication and returns the Google Calendar API service object."""
    creds = None
    # token.json stores the user's access and refresh tokens
    if os.path.exists("token.json"):
        creds = Credentials.from_authorized_user_file("token.json", SCOPES)
        
    # If there are no valid credentials, make the user log in
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file("credentials.json", SCOPES)
            creds = flow.run_local_server(port=0)
        # Save credentials for the next run
        with open("token.json", "w") as token:
            token.write(creds.to_json())

    return build("calendar", "v3", credentials=creds)

def fetch_assignments_from_mockoon():
    """Fetches mock assignment data from your local Mockoon environment."""
    mockoon_url = "http://localhost:3000/assignments" # Replace with your Mockoon port/route
    try:
        response = requests.get(mockoon_url)
        response.raise_for_status()
        return response.json() # Assumes Mockoon returns a JSON array
    except requests.exceptions.RequestException as e:
        print(f"Error fetching from Mockoon: {e}")
        return []

def add_assignment_to_calendar(service, assignment):
    """Maps assignment data to a Google Calendar event structure and uploads it."""
    # Google Calendar expects dates in ISO 8601 format (YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS)
    # If assignments are all-day, use 'date'. If they have a specific time, use 'dateTime'.
    event = {
        'summary': assignment.get('title', 'New Assignment'),
        'description': assignment.get('description', 'No description provided.'),
        'start': {
            'dateTime': assignment.get('due_date'), # e.g., '2026-06-01T23:59:00Z'
            'timeZone': 'UTC',
        },
        'end': {
            'dateTime': assignment.get('due_date'), # For deadlines, start time usually equals end time
            'timeZone': 'UTC',
        },
        'reminders': {
            'useDefault': False,
            'overrides': [
                {'method': 'popup', 'minutes': 24 * 60}, # 1 day before
                {'method': 'popup', 'minutes': 60},      # 1 hour before
            ],
        },
    }

    try:
        # "primary" defaults to your main Google Calendar account
        event_result = service.events().insert(calendarId='primary', body=event).execute()
        print(f"Successfully added: {event_result.get('summary')} (Link: {event_result.get('htmlLink')})")
    except HttpError as error:
        print(f"An error occurred while adding event: {error}")

def main():
    service = get_calendar_service()
    assignments = fetch_assignments_from_mockoon()
    
    print(f"Found {len(assignments)} assignments to sync...")
    for assignment in assignments:
        add_assignment_to_calendar(service, assignment)

if __name__ == "__main__":
    main()