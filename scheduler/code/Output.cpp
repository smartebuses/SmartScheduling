#include "Output.h"
#include "iostream"
#include <numeric>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

using namespace std;

void Output::printResults(primitiveVariables variables, vector<vector<string>> stationData, long elapsedTime,
                                  double startingTime, double endingTime, double solutionValue, string status,
                                  double optimalGap, string method) {

    cout << "Start time:" << startingTime << "\tEnd time:" << endingTime << endl;

    string chargingLocations = "Charging station installation locations:\n";
    for (int i = 0; i < variables.chargingStations.size(); i++) {
        if (variables.chargingStations[i] == 1) {
            chargingLocations += "\t" + stationData[i][0] + "\n";
        }
    }
    cout << chargingLocations << endl;
    double totalEnergyUsed = 0.0;
    double nonRenewableEnergy = 0.0;
    double horizonEnergy = 0.0;
    double horizonNonClean = 0.0;
    int horizonCharge = 0;
    int totalCharges = 0;
    double totalChargeAmount = 0.0;
    for (int busIndex = 0; busIndex < variables.buses.size(); busIndex++) {

        int b = variables.buses[busIndex];
        cout << "Bus " << b << ":" << endl;
        totalEnergyUsed += accumulate(variables.chargeAmount[b].begin(), variables.chargeAmount[b].end(), 0.0);
        nonRenewableEnergy += accumulate(variables.nonRenewable[b].begin(), variables.nonRenewable[b].end(), 0.0);
        totalCharges += accumulate(variables.charge[b].begin(), variables.charge[b].end(), 0);
        totalChargeAmount += accumulate(variables.chargeAmount[b].begin(), variables.chargeAmount[b].end(), 0.0);
        printLoop("non-clean energy used", variables.nonRenewable[b]);
        printLoop("Bus stops", variables.busSequences[b]);
        printLoop("Scheduled arrival time in hour decimal", variables.scheduledTime[b]);
        printLoop("Actual arrival time in hour decimal", variables.arrivalTime[b]);
        printLoop("Charge time in hour decimal",  variables.chargeTime[b]);
        printLoop("Battery capacity (kWh)",  variables.capacity[b]);
        printLoop("Charge amount (kWh)",  variables.chargeAmount[b]);
        printLoop("Charge",  variables.charge[b]);
        if(method == "SPM"){
            printLoop("Ase", variables.ases[b]);
            printLoop("Discount (kWh)", variables.discounts[b]);
        }
        cout << "\tTotal energy gained for bus:"<< accumulate(variables.chargeAmount[b].begin(), variables.chargeAmount[b].end(), 0.0) << endl;
        for (int i = 0; i < variables.nonRenewable[b].size(); i++) {
            if (variables.arrivalTime[b][i] >= startingTime && variables.arrivalTime[b][i] <= endingTime) {
                horizonEnergy += variables.chargeAmount[b][i];
                horizonNonClean += variables.nonRenewable[b][i];
                horizonCharge += variables.charge[b][i];
            }
        }
        cout << endl << endl;

    }
    cout << "--CEW information--"<< endl;
    for (int k = 0; k < variables.powerExcess.size(); k++) {
        cout << "CEW:" << k << endl;
        cout << "\tCEW Start time: " << variables.powerExcess[k].startTime << endl;
        cout << "\tCEW End time: " << variables.powerExcess[k].endTime << endl;

        double windowCleanEnergyUsed = 0.0;
        for (int busIndex = 0; busIndex < variables.buses.size(); busIndex++) {
            int b = variables.buses[busIndex];
            double busCleanEnergyUsed = accumulate(variables.windowEnergyUsed[k][b].begin(), variables.windowEnergyUsed[k][b].end(), 0.0);
            windowCleanEnergyUsed += busCleanEnergyUsed;
            if(busCleanEnergyUsed!=0.0){
                cout << "\t\tBus:" << b  << " used " << busCleanEnergyUsed << " from CEW " << k << endl;
            }
        }


        cout << "\tCEW Total clean energy used during window: " << windowCleanEnergyUsed << endl;
        cout << "\tCEW Total clean energy available: " << variables.powerExcess[k].availableEnergy << endl << endl;
    }
    cout << "Horizon energy used: " << horizonEnergy << endl;
    cout << "Horizon non-clean energy used: " << horizonNonClean << endl;
    cout << "Horizon charges: " << horizonCharge << endl;
    cout << "Total energy used: " << totalEnergyUsed << endl;
    cout << "Total charges: " << totalCharges << endl;
    cout << "Solution value:" << solutionValue << endl;
    cout << "Solution status:" << status << endl;
    cout << "Elapsed Time: " << elapsedTime << endl;
    cout << "Gap:" << optimalGap * 100 << endl;
    cout << "Total non-clean used: " << nonRenewableEnergy << endl;


}

void Output::writeSolutionFile(primitiveVariables solutionVariables, string solutionFile) {
    ofstream ofs(solutionFile);
    {
        boost::archive::text_oarchive oa(ofs);
        oa << solutionVariables;
    }

}

template <class T>
void Output::printLoop(string variable, vector<T> values){
    cout << "\t" << variable << ":\n\t\t[";
    for(int i=0;i<values.size();i++){
        cout << values[i];

        if(i != values.size()-1){
            cout << ", ";
        }
        else{
            cout << "]";
        }
        i = i + 1;
        if(i%10==0){
            cout << "\n\t\t";
        }
        i = i - 1;
    }
    cout << "\n" << endl;
}


