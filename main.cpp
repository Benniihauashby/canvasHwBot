#include "canvas_api.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

int main() {
    // Connect to Mockoon (change to real Canvas URL later)
    CanvasAPI canvas("http://localhost:3000");


    std::cout << "=== Fetching Courses ===" << std::endl;
    // use auto to let compiler know the type is (vector<Couse>)
    // getCourses() to fetch all from API
    auto courses = canvas.getCourses();
    
    // print out all courses available
    // const auto& to just read it, not copy
    for(const auto& course : courses) {
        std::cout << "Course: " << course.name 
                  << " (" << course.course_code << ")" << std::endl;
    }
    
    // Print all assignemnts
    std::cout << "\n--- Fetching All Assignments ---" << std::endl;
    auto assignments = canvas.getAllAssignments();
    
    // iterate for each assignment
    for(const auto& assignment : assignments) {
        std::cout << "Assignment: " << assignment.name << std::endl;
        std::cout << "  Due: " << assignment.due_at << std::endl;
        std::cout << "  Points: " << assignment.points_possible << std::endl;
        std::cout << "  Course ID: " << assignment.course_id << std::endl;
        std::cout << std::endl;
    }

    // ------------------------------------------------------------------
    // Feature 1: Write validated assignment data to a temporary JSON file
    // ------------------------------------------------------------------
    // We use the same nlohmann::json library (already bundled) that the
    // CanvasAPI class uses for parsing.  This keeps the C++ side as the
    // single source of truth for fetching and validating data.  The
    // resulting temp file is then handed off to the Python Google Calendar
    // sync script so Python does not need to re-fetch from the network.
    // ------------------------------------------------------------------

    // Build a JSON array where each element is an object representing one assignment.
    nlohmann::json j = nlohmann::json::array();
    for (const auto& a : assignments) {
        nlohmann::json obj;
        obj["id"]              = a.id;
        obj["name"]            = a.name;
        obj["description"]     = a.description;
        obj["due_at"]          = a.due_at;
        obj["points_possible"] = a.points_possible;
        obj["course_id"]       = a.course_id;
        j.push_back(obj);
    }

    // Generate a unique temp file path using the process ID so parallel
    // runs (or rapid re-runs) do not stomp on the same file.
    std::ostringstream tempFileName;
    tempFileName << "/tmp/canvas_assignments_" << getpid() << ".json";
    std::string tempPath = tempFileName.str();

    // Open the temp file for writing.  std::ofstream uses RAII so the
    // underlying file handle is closed automatically when it goes out of scope.
    std::ofstream outFile(tempPath);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open temp file: " << tempPath << std::endl;
        return 1;  // Bail out early so we do not spawn Python with missing data.
    }

    // std::setw(4) pretty-prints the JSON with 4-space indentation,
    // making it easier to inspect manually while debugging.
    outFile << std::setw(4) << j << std::endl;
    outFile.close();

    std::cout << "\nWrote " << assignments.size()
              << " assignment(s) to temp file: " << tempPath << std::endl;

    // ------------------------------------------------------------------
    // Feature 1 (continued): Spawn the Python Google Calendar sync script
    // ------------------------------------------------------------------
    // std::system() invokes the command through the default shell (/bin/sh).
    // It blocks execution until the Python process finishes, which is fine
    // for a sequential tool like this.  We pass the temp file path as the
    // first command-line argument so gCalendar.py knows where to read.
    // ------------------------------------------------------------------

    std::ostringstream pyCmd;
    // Activate the local venv first so google-api-python-client is on PATH,
    // then run the script with the temp JSON file as an argument.
    pyCmd << "source venv/bin/activate && python gCalendar.py \""
          << tempPath << "\"";

    std::cout << "Spawning Python sync: " << pyCmd.str() << std::endl;
    int pyExit = std::system(pyCmd.str().c_str());

    // std::system() returns the child exit status encoded by the shell.
    // A non-zero value usually means the Python script raised an error.
    if (pyExit != 0) {
        std::cerr << "Warning: Python script exited with code " << pyExit << std::endl;
    }

    // Optional cleanup: remove the temp file now that Python has consumed it.
    // Uncomment the next line if you want automatic deletion instead of leaving
    // the file in /tmp for manual inspection.
    // std::remove(tempPath.c_str());

    return 0;
}
