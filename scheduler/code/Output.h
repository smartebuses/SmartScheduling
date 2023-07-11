//
// Created by padraigh on 02/03/2022.
//


#include "DataStructures.h"

class Output{
public:
    void writeSolutionFile(primitiveVariables solutionVariables, string solutionFile);
    void printResults(primitiveVariables variables, vector<vector<string>> stationData, long elapsedTime,
                      double startingTime, double endingTime, double solutionValue, string status, double optimalGap,
                      string method);

private:
    template <class T>
    void printLoop(string variable, vector<T> values);
};

