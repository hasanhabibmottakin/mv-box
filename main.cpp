#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void writeM3U(const string& filename, const json& items, const string& matchTitle) {
    ofstream file(filename, ios::app);
    if (file.tellp() == 0) {
        file << "#EXTM3U\n"; 
    }
    for (const auto& item : items) {
        string title = item.value("title", "Unknown Title");
        string url = item.value("path", "");
        
        if (!url.empty()) {
            file << "#EXTINF:-1, " << matchTitle << " - " << title << "\n";
            file << url << "\n";
        }
    }
    file.close();
}

void writeJSON(const string& filename, const json& dataArray) {
    ofstream file(filename);
    file << dataArray.dump(4); 
    file.close();
}

int main() {
    const char* api_url_env = getenv("API_URL");
    if (!api_url_env) {
        cerr << "Error: API_URL environment variable is missing!" << endl;
        return 1;
    }
    string url = api_url_env;

    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (!curl) {
        cerr << "Failed to initialize CURL." << endl;
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "CURL Error: " << curl_easy_strerror(res) << endl;
        return 1;
    }

    json response;
    try {
        response = json::parse(readBuffer);
    } catch (json::parse_error& e) {
        cerr << "JSON Parse Error: " << e.what() << endl;
        return 1;
    }

    json matchesJson = json::array();
    json replaysJson = json::array();
    json highlightsJson = json::array();

    ofstream("matches.m3u", ios::trunc).close();
    ofstream("replays.m3u", ios::trunc).close();
    ofstream("highlights.m3u", ios::trunc).close();
    if (response.contains("data") && response["data"].contains("matchList")) {
        for (const auto& match : response["data"]["matchList"]) {
            
            string team1 = match["team1"].value("name", "Team 1");
            string team2 = match["team2"].value("name", "Team 2");
            string matchTitle = team1 + " vs " + team2;

            if (match.contains("playSource") && !match["playSource"].empty()) {
                matchesJson.push_back({
                    {"match", matchTitle},
                    {"sources", match["playSource"]}
                });
                writeM3U("matches.m3u", match["playSource"], matchTitle);
            }

            if (match.contains("replay") && !match["replay"].empty()) {
                replaysJson.push_back({
                    {"match", matchTitle},
                    {"sources", match["replay"]}
                });
                writeM3U("replays.m3u", match["replay"], matchTitle);
            }

            if (match.contains("highlights") && !match["highlights"].empty()) {
                highlightsJson.push_back({
                    {"match", matchTitle},
                    {"sources", match["highlights"]}
                });
                writeM3U("highlights.m3u", match["highlights"], matchTitle);
            }
        }
    }

    writeJSON("matches.json", matchesJson);
    writeJSON("replays.json", replaysJson);
    writeJSON("highlights.json", highlightsJson);

    cout << "Extraction complete! Generated separate JSON and M3U files." << endl;
    return 0;
}
