#include "canvas_api.h"
#include <curl/curl.h>
#include <iostream>

// libcurl calls this func everytime it receive a chink of data form the server
// pointer to the raw data recieved
size_t CanvasAPI::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Constructor for the Real canvas API
CanvasAPI::CanvasAPI(const std::string& domain, const std::string& token) 
    : base_url("https://" + domain + "/api/v1"), api_token(token) {}

// constructor for Mockoon
CanvasAPI::CanvasAPI(const std::string& url) 
    : base_url(url + "/api/v1"), api_token("") {}

// private helper to make http GET reueqest to specific endpoint
std::string CanvasAPI::makeRequest(const std::string& endpoint) {
    // Init new curl session
    CURL* curl = curl_easy_init();
    // string to read response
    std::string readBuffer;
    
    // if curl goes through, run the body
    if(curl) {
        // build the url with specific endpoint
        std::string url = base_url + endpoint;
        // choose specific url
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // do writebackcall when data is received
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        // pass readbuffer as parameter so writecallback can fill it 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // for real api token
        // if(!api_token.empty()) {
        //     std::string auth = "Authorization: Bearer " + api_token;
        //     struct curl_slist* headers = nullptr;
        //     headers = curl_slist_append(headers, auth.c_str());
        //     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // }
        
        CURLcode res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
        }
        
        // free up curl resources
        curl_easy_cleanup(curl);
    }
    // return response string
    return readBuffer;
}

// Grab all courses from current use
std::vector<Course> CanvasAPI::getCourses() {
    // make empty vector to hold
    std::vector<Course> courses;
    // call canvas api endpoint to list user course
    std::string response = makeRequest("/users/self/courses");
    
    // use try - catch to help exceptions (idk about this one)
    try {
        // parse json string into json object
        nlohmann::json jsonArray = nlohmann::json::parse(response);
        // loop through each array(each course)
        for(const auto& item : jsonArray) {
            Course c;
            c.id = item.value("id", 0);
            c.name = item.value("name", "Unknown");
            c.course_code = item.value("course_code", "N/A");
            courses.push_back(c);
        }
    } catch(const std::exception& e) {
        // else print error code
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    // return vector of courses
    return courses;
}

// Fech specific course assignement
std::vector<Assignment> CanvasAPI::getAssignments(int course_id) {
    // Create empty vector to hold
    std::vector<Assignment> assignments;
    // use specifc endpoint of API to grab data
    // EXAMPLE: /courses/101/assignments
    std::string endpoint = "/courses/" + std::to_string(course_id) + "/assignments";
    // Actually fetch the data
    std::string response = makeRequest(endpoint);
    
    // use try catch again
    try {
        // Parse json reponse
        nlohmann::json jsonArray = nlohmann::json::parse(response);
        // iterate through each assignemnt in array
        for(const auto& item : jsonArray) {
            
            Assignment a;
            a.id = item.value("id", 0);
            a.name = item.value("name", "Unknown");
            a.description = item.value("description", "");
            a.due_at = item.value("due_at", "");
            a.points_possible = item.value("points_possible", 0.0);
            a.course_id = item.value("course_id", course_id);
            assignments.push_back(a);
        }
    } catch(const std::exception& e) {
        // error message
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return assignments;
}

// Fetch ALL assignments from api call, combine courses, and assignments
std::vector<Assignment> CanvasAPI::getAllAssignments() {
    // make vector to hold all
    std::vector<Assignment> all;
    // Grab all courses first
    auto courses = getCourses();


    
    // Iterate through each course
    for(const auto& course : courses) {
        // grab all assignemnts
        auto assignments = getAssignments(course.id);
        // Append this course assignemnt to the main list
        // insert takes an iterator position and a range to copy from
        all.insert(all.end(), assignments.begin(), assignments.end());
    }
    
    // return the combined list of all assignemnts
    return all;
}