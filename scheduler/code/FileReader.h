//
// Created by Padraigh on 05/01/2021.
//

#ifndef SCHEDULER_READ_FILES_H
#define SCHEDULER_READ_FILES_H
#include "string"
#include "vector"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class FileReader{
public:
    vector<vector<string>> readCSV(string fileName);
    json readJson(string fileName);
    bool validatePath(string fileName);
private:
    static inline char separator();
};

#endif //SCHEDULER_READ_FILES_H
