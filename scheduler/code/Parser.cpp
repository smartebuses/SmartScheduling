//
// Created by padraigh on 02/03/2022.
//

#include <iostream>

#include "fstream"
#include "Parser.h"
#include "Utils.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/variant.hpp>

using namespace std;

vector<vector<string>> Parser::parseStopsFile(string stopsFile){
    return Parser::myFileReader.readCSV(stopsFile);
}

vector<int> Parser::parseChargingStationsFile(string chargingStationFile){
    bool exists = Parser::myFileReader.validatePath(chargingStationFile);
    if(!exists){
        exit(-1);
    }
    vector<int> chargingStations;
    ifstream file(chargingStationFile);
    string line;

    while(getline(file, line)){
        for(char i: line){
            if(isdigit(i)){
                chargingStations.push_back((int)(i)-48);
            }
        }
    }
    file.close();
    return chargingStations;
}

primitiveVariables Parser::parseSolutionFile(string solutionFile){
    Parser::myFileReader.validatePath(solutionFile);
    primitiveVariables loadedVars;
    {
        ifstream ifs(solutionFile);
        boost::archive::text_iarchive ia(ifs);
        ia >> loadedVars;
    }
    return loadedVars;
}

map<string, string> Parser::parseArguments(int argc, char *argv[]){
    map<string, string> parsedArguments;
    string argumentDelimiter = "--";
    for(int i=1; i<argc;i++){
        string argument = argv[i];
        if(i%2==1){
            string argumentName = argument.substr(argumentDelimiter.size(), argument.size());
            parsedArguments[argumentName] = argv[++i];
        }
    }


    return parsedArguments;
}


vector<vector<double>> Parser::parseDistanceFile(string stationDistanceFile, int numStations){
    vector<vector<double>> distances(numStations, vector<double>(numStations));

    for (auto &row: Parser::myFileReader.readCSV(stationDistanceFile)) {
        distances[stoi(row[0])][stoi(row[1])] = stof(row[2]);
    }
    for (int i = 0; i < numStations; i++) {
        for (int j = 0; j <= i; j++) {
            if (i == j) {
                distances[i][j] = 0.0;
            } else if (i > 0) {
                distances[i][j] = distances[j][i];
            }
        }
    }
    return distances;

}

ModelParameters Parser::parseBusData(string busDataFile, ModelParameters parameters) {
    map<string, pair<int, int>> busTimeTables;

    json busData = Parser::myFileReader.readJson(busDataFile);

    for (auto &busRoute: busData) {
        // todo maybe have a vector of forbidden lines instead of hard coding it?
        if (busRoute.at("line") == "350-1") {
            continue;
        }

        json routeBuses = busRoute.at("buses");
        for (auto &bus: routeBuses) {

            vector<int> sequence;
            vector<double> times;
            vector<int> rest;

            int busNum = bus.at("bus");
            int sequenceCap = -1;
            for (int i = 0; i < bus.at("path").size(); i++) {
                if (times.size() == sequenceCap) {
                    break;
                }
                json stop = bus.at("path")[i];
                string time = stop.at("time");
                double convertedTime = Parser::myUtils.convertTime(time);
                if (convertedTime == 0) {
                    convertedTime = 24.0;
                }

                times.push_back(convertedTime);
                sequence.push_back(stop.at("station_id"));
                if (stop.contains("rest")) {
                    rest.push_back(1);
                } else {
                    rest.push_back(0);
                }
            }
            parameters.busKeys.push_back(busNum);
            parameters.busSequencesRaw[busNum] = sequence;
            parameters.busTimeRaw[busNum] = times;
            parameters.rests[busNum] = rest;
        }
    }
    return parameters;
}

