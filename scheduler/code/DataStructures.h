#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include "vector"
#include "string"
#include "map"


using namespace std;
struct CleanEnergyWindow{
    double startTime;
    double endTime;
    double availableEnergy;
};

struct ModelParameters{
    vector<int> chargingStops;
    vector<CleanEnergyWindow> cleanEnergyWindows;
    vector<vector<string>> stationData;
    int numberStations;
    vector<vector<double>> distances;
    vector<int> busKeys;
    map<int, vector<int>> busSequencesRaw;
    map<int, vector<double>> busTimeRaw;
    map<int, vector<int>> rests;
};

struct primitiveVariables{
    vector<int> buses;
    vector<int> chargingStations;
    map<int, vector<int>> busSequences;
    map<int, vector<double>> nonRenewable;
    map<int, vector<double>> arrivalTime;
    map<int, vector<double>> scheduledTime;
    map<int, vector<double>> deviationTime;
    map<int, vector<double>> capacity;
    map<int, vector<double>> chargeTime;
    map<int, vector<double>> chargeAmount;
    map<int, vector<int>> charge;
    vector<vector<double>> tripTime;
    vector<vector<double>> tripCost;
    vector<CleanEnergyWindow> powerExcess;
    vector<map<int, vector<double>>> windowEnergyUsed;
    map<int, vector<int>> ases;
    map<int, vector<double>> discounts;
    map<int, vector<vector<double>>> cleanChargeTime;
    map<int, vector<vector<int>>> cleanWindowCharge;
};

namespace boost{
    namespace serialization{
        template<class Archive>
        void serialize(Archive & ar, primitiveVariables & vars, const unsigned int version){
            ar & vars.buses;
            ar & vars.chargingStations;
            ar & vars.busSequences;
            ar & vars.nonRenewable;
            ar & vars.arrivalTime;
            ar & vars.scheduledTime;
            ar & vars.deviationTime;
            ar & vars.capacity;
            ar & vars.chargeTime;
            ar & vars.chargeAmount;
            ar & vars.charge;
        }
    }
}
#endif DATASTRUCTURES_H
