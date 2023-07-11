//
// Created by padraigh on 02/03/2022.
//


#include "DataStructures.h"
#include "FileReader.h"
#include "Utils.h"

class Parser {
public:
    vector<vector<string>> parseStopsFile(string stopsFile);
    vector<int> parseChargingStationsFile(string chargingStationFile);
    primitiveVariables parseSolutionFile(string solutionFile);
    map<string, string> parseArguments(int argc, char *argv[]);
    vector<vector<double>> parseDistanceFile(string stationDistanceFile, int numStations);
    ModelParameters parseBusData(string busDataFile, ModelParameters parameters);

private:
    FileReader myFileReader;
    Utils myUtils;
};
