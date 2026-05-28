#include "canvas_api.h"
#include <iostream>
#include <iomanip>

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
    
    return 0;
}