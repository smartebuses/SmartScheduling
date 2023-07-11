//
// Created by Padraigh on 05/01/2021.
//

#include "FileReader.h"
#include "string"
#include "vector"
#include "iostream"
#include "fstream"
#include "stdio.h"
#include <nlohmann/json.hpp>
#include "sstream"

using namespace std;
using json = nlohmann::json;

vector<vector<string>> FileReader::readCSV(string fileName){
        validatePath(fileName);
        vector<vector<string>> returnData;
        ifstream readFile(fileName.c_str());
        string line;
        bool columnRow = true;
        while(getline(readFile, line)){
            if(columnRow){
                columnRow=false;
                continue;
            }
            stringstream stringStream(line);
            string data;
            vector<string> row;
            while(getline(stringStream, data, ',')){
                row.push_back(data);
            }
            returnData.push_back(row);
        }
        readFile.close();
        return returnData;
    }

json FileReader::readJson(string fileName) {
    bool exists = validatePath(fileName);
    if(!exists){
        cout << fileName << " does not exist" << endl;
        exit(-1);
    }
    vector <string> data;
    ifstream inputFileStream(fileName);
    cout << fileName << endl;
    json jsonData = json::parse(inputFileStream);

    inputFileStream.close();
    return jsonData;
}

bool FileReader::validatePath(string fileName){
    FILE *file;

    if(file = fopen(fileName.c_str(), "r")){
        fclose(file);
        return true;
    }
    else{
        cout << "File" << fileName << " does not exist";
        exit(-1);
    }

}

inline char  FileReader::separator(){
#ifdef  _WIN32
    return '\\';
#else
    return '/';
#endif
}


