#ifndef CANVAS_API_H
#define CANVAS_API_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"


// make assignment class
struct Assignment {
    int id;
    std::string name;
    std::string description;
    std::string due_at;
    double points_possible;
    int course_id;
};

// course class
struct Course {
    int id;
    std::string name;
    std::string course_code;
};


class CanvasAPI {
private:
    // stores apis base url
    std::string base_url;
    // stores the api token
    std::string api_token;
    
    //libcurl documentation to work
    // deliver chunks of the reponse
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    // request for http ge. takes the string endpoint
    std::string makeRequest(const std::string& endpoint);
    
public:
    // Constructor for the actual Canvas API
    CanvasAPI(const std::string& domain, const std::string& token);
    
    // For Mockoon testing, localhost
    CanvasAPI(const std::string& base_url);
    
    // dynamic arrays of Courses, specifc course Assignments, and all
    std::vector<Course> getCourses();
    std::vector<Assignment> getAssignments(int course_id);
    std::vector<Assignment> getAllAssignments();
};

#endif