//
// Created by Padraigh on 23/10/2020.
//

#include <iostream>
#include <ilcplex/ilocplex.h>
#include <vector>
#include <ctime>
#include "FileReader.h"
#include "Utils.h"
#include "DataStructures.h"
#include "Output.h"
#include "Parser.h"

ILOSTLBEGIN
using namespace std;


struct variables{
    // xi boolean variable which is true if charging station is install at station i
    IloIntArray chargingStation;
    // B set of available buses
    IloIntArray buses;
    // rbi amount of renewable energy used by bus b at stop i
    map<int, IloNumVarArray> nonRenewable;
    // S set of stop sequences for each bus
    map<int, IloIntArray> busSequences;
    // Tbj scheduled arrival time of bus b at stop j
    map<int, IloNumArray> scheduledArrival;
    // tbj actual arrival time of bus b at stop j
    map<int, IloNumVarArray> actualArrival;
    // delta tbj difference between actual arrival time and original schedule time of bus b at stop j
    map<int, IloNumVarArray> deviationTime;
    // cbj amount of energy bus b has at stop j
    map<int, IloNumVarArray> batteryCapacity;
    // ebj amount of energy gained by bus b at stop j if charged for ctbj time units.
    map<int, IloNumVarArray> chargeAmount;
    // ctbj charge time of bus b at stop j
    map<int, IloNumVarArray> chargeTime;
    // xbj boolean variable if bus b charges at stop j
    map<int, IloIntVarArray> charge;
    // Tij amount of time required for trip i to j
    IloArray<IloNumArray> tripTime;
    // Gammak information about the kth clean energy window
    IloArray<IloNumArray> powerExcess;
    // Dij amount of energy required for trip i to j
    IloArray<IloNumArray> tripCost;

    //cekbi amount of clean energy used in CEW k by bus b at stop i
    vector<map<int, IloNumVarArray>> windowEnergyUsed;
    map<int, IloIntVarArray> ases;
    map<int, IloNumVarArray> discounts;
    // Omega time which the current horizon ends
    IloNum horizonEndTime;

    //wtbik amount of time bus b spends charging at stop i using clean energy from window k
    map<int, vector<IloNumVarArray>> cleanChargeTime;
    //ktbik boolean variable if bus b charges at stop i during CEW k
    map<int, vector<IloIntVarArray>> cleanWindowCharge;

}modelVariables;

void createVariables(IloEnv env, ModelParameters parameters, map<string, string> arguments) {

    double deviationTime = stod(arguments["deviationTime"]);
    double maxChargeTime = stod(arguments["maxChargeTime"]);
    double chargeRate = stod(arguments["chargeRate"]);
    double maxBatteryCapacity = stod(arguments["maxBatteryCapacity"]);
    double minBatteryCapacity = stod(arguments["minBatteryCapacity"]);

    modelVariables.horizonEndTime = IloNum(stod(arguments["horizonEndTime"]));

    modelVariables.buses = IloIntArray (env);

    modelVariables.chargingStation = IloIntArray (env, parameters.numberStations);

    for(int i=0;i<parameters.chargingStops.size();i++){
        modelVariables.chargingStation[i] = parameters.chargingStops[i];
    }
    modelVariables.powerExcess = IloArray<IloNumArray>(env);
    // Removing time windows before the first bus
    if (!parameters.cleanEnergyWindows.empty()) {
        for(auto& window: parameters.cleanEnergyWindows){
            bool beforeFirstBus = true;
            for(int b: parameters.busKeys){
                if(window.endTime>parameters.busTimeRaw[b][0]){
                    beforeFirstBus=false;
                }
            }
            if(!beforeFirstBus){
                modelVariables.powerExcess.add(IloNumArray(env, 3, window.startTime, window.endTime,
                                                           window.availableEnergy));
            }
        }
    }
    for(int b : parameters.busKeys){
        int numStops = parameters.busSequencesRaw[b].size();
        modelVariables.buses.add(b);
        IloIntArray busBSequence = IloIntArray(env, numStops);
        IloNumArray busBTimes = IloNumArray(env, numStops);
        IloNumVarArray busBActualArrivalTimeVars(env, numStops);
        IloNumVarArray busBDeltaTimes(env, numStops);
        IloNumVarArray busBBatteryCapacities(env, numStops);
        IloNumVarArray busBChargeTime(env, numStops);
        IloIntVarArray busBCharge(env, numStops);
        IloNumVarArray busBChargeAmount(env, numStops);
        IloNumVarArray busBRenewable(env, numStops);
        IloIntVarArray busBAses(env, numStops);
        vector<IloNumVarArray> stopWindowTime(numStops);
        vector<IloIntVarArray> stopWindowCharge(numStops);

        IloNumVarArray discount(env, numStops);

        for(int j=0; j<numStops; j++){
            string varString = "Bus" + to_string(b) + "SequenceStop" + to_string(j);
            busBSequence[j] = parameters.busSequencesRaw[b][j];
            busBTimes[j] = parameters.busTimeRaw[b][j];

            busBActualArrivalTimeVars[j] = IloNumVar(env, 0.0, 24.00,
                                                     (varString + "ArrivalTime").c_str());
            busBDeltaTimes[j] = IloNumVar(env, 0.0, deviationTime,
                                          (varString + "DeltaTime").c_str());
            busBBatteryCapacities[j] = IloNumVar(env, minBatteryCapacity, maxBatteryCapacity,
                                                 (varString + "BatteryCapacity").c_str());

            busBChargeTime[j] = IloNumVar(env, 0.0, maxChargeTime, (varString + "ChargeTime").c_str());
            busBCharge[j] = IloIntVar(env, 0, 1, (varString + "Charge").c_str());
            busBChargeAmount[j] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                            (varString + "chargeAmount").c_str());
            busBRenewable[j] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                         (varString + "nonRenewable").c_str());
            busBAses[j] = IloIntVar(env, 0, 1, (varString + "ase").c_str());
            IloNumVarArray cleanEnergyTime(env,  modelVariables.powerExcess.getSize());
            IloIntVarArray cleanEnergyCharge(env, modelVariables.powerExcess.getSize());

            discount[j] = IloNumVar(env, 0, maxChargeTime * chargeRate, (varString+"Discount").c_str());

            for(int k=0;k<modelVariables.powerExcess.getSize();k++){
                cleanEnergyTime[k] = IloNumVar(env, 0.0, maxChargeTime,
                                               (varString+"CleanEnergyTime"+ to_string(k)).c_str());
                cleanEnergyCharge[k] = IloIntVar(env, 0, 1,
                                                 (varString+"CleanEnergyCharge"+ to_string(k)).c_str());
            }
            stopWindowTime[j] = cleanEnergyTime;
            stopWindowCharge[j] = cleanEnergyCharge;
        }
        modelVariables.busSequences[b] = busBSequence;
        modelVariables.scheduledArrival[b] = busBTimes;
        modelVariables.actualArrival[b] = busBActualArrivalTimeVars;
        modelVariables.deviationTime[b] = busBDeltaTimes;
        modelVariables.batteryCapacity[b] = busBBatteryCapacities;
        modelVariables.chargeAmount[b] = busBChargeAmount;
        modelVariables.chargeTime[b] = busBChargeTime;
        modelVariables.charge[b] = busBCharge;
        modelVariables.nonRenewable[b] = busBRenewable;
        modelVariables.ases[b] = busBAses;
        modelVariables.cleanChargeTime[b] = stopWindowTime;
        modelVariables.cleanWindowCharge[b] = stopWindowCharge;

        modelVariables.discounts[b] = discount;
    }
    modelVariables.tripCost = IloArray<IloNumArray>(env, parameters.numberStations);
    modelVariables.tripTime = IloArray<IloNumArray> (env, parameters.numberStations);

    cout << "Charging stations:" << modelVariables.chargingStation << endl;
    cout << "Total charging stations: " << IloSum(modelVariables.chargingStation) << endl;
    for (int i = 0; i < parameters.numberStations; i++) {
        IloNumArray ijCost = IloNumArray(env, parameters.numberStations);
        IloNumArray ijTime = IloNumArray(env, parameters.numberStations);
        for (int j = 0; j < parameters.numberStations; j++) {
            double cost = parameters.distances[i][j] * stod(arguments["busEnergyCost"]);

            ijCost[j]=cost;
            ijCost[j] = parameters.distances[i][j] * stod(arguments["busEnergyCost"]);
            ijTime[j] = ((60 / stod(arguments["busSpeed"])) * parameters.distances[i][j]) / 60;

        }
        modelVariables.tripCost[i] = ijCost;
        modelVariables.tripTime[i] = ijTime;
    }

    for(int k=0;k<modelVariables.powerExcess.getSize();k++){
        map<int, IloNumVarArray> stopWindowEnergy;

        for(int b : parameters.busKeys){

            int numStops = modelVariables.busSequences[b].getSize();
            IloNumVarArray windowEnergy(env, numStops);
            for(int j=0;j<numStops;j++){

                windowEnergy[j] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                            ("window"+to_string(modelVariables.powerExcess[k][0])+
                                            "to"+to_string(modelVariables.powerExcess[k][1])+"bus"+to_string(b)
                                            +"stop"+to_string(j)).c_str());
            }
            stopWindowEnergy[b] = windowEnergy;
        }

        modelVariables.windowEnergyUsed.push_back(stopWindowEnergy);
    }
    cout << "Targeted Time windows: " << modelVariables.powerExcess << endl;


}


IloModel addCEWConstraints(IloModel model, IloEnv env, map<string, string> arguments, int b, int index,
                           IloIntArray busSequence, string varString){

    double minChargeTime = stod(arguments["minChargeTime"]);
    double maxChargeTime = stod(arguments["maxChargeTime"]);
    double chargeRate = stod(arguments["chargeRate"]);
    int bigM = stoi(arguments["bigM"]);
    double maxBatteryCapacity = stod(arguments["maxBatteryCapacity"]);
    double minBatteryCapacity = stod(arguments["minBatteryCapacity"]);
    double deviationTime = stod(arguments["deviationTime"]);
    string method = arguments["method"];

    if(method == "SPM"){

        model.add(modelVariables.actualArrival[b][index] + (bigM * modelVariables.ases[b][index]) >= modelVariables.horizonEndTime );
        model.add(modelVariables.discounts[b][index] <= modelVariables.chargeAmount[b][index] * stod(arguments["discountFactor"]));
        model.add(modelVariables.discounts[b][index] <= bigM * (1 - modelVariables.ases[b][index]));
    }
    if (modelVariables.chargingStation[busSequence[index]] == 1 && modelVariables.powerExcess.getSize() >= 1) {
        IloNumVarArray windowTimeValues(env);
        IloNumVarArray previousK(env);
        for (int k = 0; k < modelVariables.powerExcess.getSize(); k++) {

            if(modelVariables.powerExcess[k][1] < modelVariables.scheduledArrival[b][index]-deviationTime ||
               modelVariables.powerExcess[k][0] > modelVariables.scheduledArrival[b][index]+((deviationTime+maxChargeTime)*2)){

                model.add(modelVariables.windowEnergyUsed[k][b][index] + modelVariables.cleanChargeTime[b][index][k] +
                              modelVariables.cleanWindowCharge[b][index][k] <= 0.0);
                windowTimeValues.add(modelVariables.windowEnergyUsed[k][b][index]);
                continue;
            }

            model.add(modelVariables.charge[b][index] >= modelVariables.cleanWindowCharge[b][index][k]);

            model.add(modelVariables.cleanWindowCharge[b][index][k] >= modelVariables.cleanChargeTime[b][index][k]);
            model.add(modelVariables.cleanWindowCharge[b][index][k]*chargeRate >= modelVariables.windowEnergyUsed[k][b][index]);

            model.add(modelVariables.actualArrival[b][index] + modelVariables.chargeTime[b][index] +
                          (bigM * (1 - modelVariables.cleanWindowCharge[b][index][k])) >= modelVariables.powerExcess[k][0]);

            model.add(modelVariables.actualArrival[b][index] -
                          (bigM * (1-modelVariables.cleanWindowCharge[b][index][k])) <= modelVariables.powerExcess[k][1]);

            model.add(modelVariables.cleanWindowCharge[b][index][k] >= modelVariables.cleanChargeTime[b][index][k]);
            if(previousK.getSize() == 0){
                model.add(modelVariables.powerExcess[k][1] - modelVariables.actualArrival[b][index] +
                              (bigM * (1 - modelVariables.cleanWindowCharge[b][index][k])) >=
                              modelVariables.cleanChargeTime[b][index][k]);
            }
            else{
                model.add(modelVariables.powerExcess[k][1] - modelVariables.actualArrival[b][index] -
                              IloSum(previousK) + (bigM * (1 - modelVariables.cleanWindowCharge[b][index][k])) >=
                              modelVariables.cleanChargeTime[b][index][k]);
            }
            previousK.add(modelVariables.cleanChargeTime[b][index][k]);

            model.add(modelVariables.actualArrival[b][index] + modelVariables.chargeTime[b][index] -
                          modelVariables.powerExcess[k][0] + (bigM * (1-modelVariables.cleanWindowCharge[b][index][k])) >=
                          modelVariables.cleanChargeTime[b][index][k]);
            model.add(modelVariables.windowEnergyUsed[k][b][index] <= modelVariables.cleanChargeTime[b][index][k]
                                                                      * chargeRate);
            windowTimeValues.add(modelVariables.windowEnergyUsed[k][b][index]);

        }
        model.add(IloSum(windowTimeValues) <= modelVariables.chargeAmount[b][index]);
        model.add(IloSum(modelVariables.cleanChargeTime[b][index]) <= modelVariables.chargeTime[b][index]);

        if(method == "SPM"){
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - IloSum(windowTimeValues) - modelVariables.discounts[b][index]);
        }
        else{
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - IloSum(windowTimeValues));
        }

    }
    else{
        if(method == "SPM"){
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - modelVariables.discounts[b][index]);
        }
        else{
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index]);
        }
        for(int k = 0; k < modelVariables.powerExcess.getSize(); k++) {
            model.add(modelVariables.windowEnergyUsed[k][b][index] + modelVariables.cleanChargeTime[b][index][k] +
                          modelVariables.cleanWindowCharge[b][index][k] <= 0.0);
        }
    }
    return model;
}


IloModel addConstraints(IloModel model, IloEnv env, ModelParameters parameters, map<string, string> arguments) {
    for (int busIndex=0;busIndex<modelVariables.buses.getSize();busIndex++ ) {
        int b = modelVariables.buses[busIndex];
        vector<int> busRests = parameters.rests[b];
        IloIntArray busSequence = modelVariables.busSequences[b];
        double minEnergyNeeded = 0.0;
        double minChargeTime = stod(arguments["minChargeTime"]);
        double maxChargeTime = stod(arguments["maxChargeTime"]);
        double chargeRate = stod(arguments["chargeRate"]);
        double startingCapacity = stod(arguments["startingCapacity"]);
        int bigM = stoi(arguments["bigM"]);
        double maxBatteryCapacity = stod(arguments["maxBatteryCapacity"]);
        double minBatteryCapacity = stod(arguments["minBatteryCapacity"]);
        double deviationTime = stod(arguments["deviationTime"]);
        string method = arguments["method"];

        model.add(modelVariables.deviationTime[b][0] <= 0.0);
        model.add(modelVariables.charge[b][0] >= modelVariables.chargeTime[b][0]);

        model.add(modelVariables.chargeTime[b][0] >= modelVariables.charge[b][0] * minChargeTime);
        model.add(modelVariables.chargeTime[b][0] <= modelVariables.charge[b][0] * maxChargeTime);

        model.add(modelVariables.nonRenewable[b][0] <= modelVariables.chargeAmount[b][0]);
        model.add(modelVariables.chargeAmount[b][0] <= modelVariables.chargeTime[b][0] * chargeRate);
        model.add(modelVariables.actualArrival[b][0] == modelVariables.scheduledArrival[b][0]);
        model.add(modelVariables.batteryCapacity[b][0] == startingCapacity);

        model.add(modelVariables.chargingStation[busSequence[0]] >= modelVariables.charge[b][0]);

        model.add(modelVariables.batteryCapacity[b][0] + modelVariables.chargeAmount[b][0] <= maxBatteryCapacity);


        model = addCEWConstraints(model, env, arguments, b, 0, busSequence, "Bus" + to_string(b) + "Station" + to_string(0));
        for (int i = 1; i < busSequence.getSize(); i++) {
            int j = i - 1;
            minEnergyNeeded += modelVariables.tripCost[busSequence[i]][busSequence[j]];
            // constraint 1
            model.add(modelVariables.batteryCapacity[b][i] >= minBatteryCapacity);
            model.add(modelVariables.batteryCapacity[b][i] + modelVariables.chargeAmount[b][i] <= maxBatteryCapacity);

            // constraint 2
            model.add(modelVariables.batteryCapacity[b][i] <=
                          modelVariables.batteryCapacity[b][j] + modelVariables.chargeAmount[b][j] - modelVariables.tripCost[busSequence[i]][busSequence[j]]);


            model.add(modelVariables.chargeAmount[b][i] <= modelVariables.chargeTime[b][i] * chargeRate);

//             constraint 3
            if(busRests[j] == 1 && busSequence[i] == busSequence[j]){
                model.add(modelVariables.deviationTime[b][i] <= 0.0);
            }

            if((modelVariables.scheduledArrival[b][i] - modelVariables.scheduledArrival[b][j]) <
            modelVariables.tripTime[busSequence[i]][busSequence[j]]){
                model.add(modelVariables.actualArrival[b][i] >=
                          modelVariables.actualArrival[b][j] + modelVariables.chargeTime[b][j] + (modelVariables.scheduledArrival[b][i] - modelVariables.scheduledArrival[b][j]));
            }
            else{
                model.add(modelVariables.actualArrival[b][i] >=
                          modelVariables.actualArrival[b][j] + modelVariables.chargeTime[b][j] +
                          modelVariables.tripTime[busSequence[i]][busSequence[j]]);
            }

            // constraint 4
            model.add(modelVariables.deviationTime[b][i] >= modelVariables.actualArrival[b][i] - modelVariables.scheduledArrival[b][i]);
            // constraint 5
            model.add(modelVariables.deviationTime[b][i] >= modelVariables.scheduledArrival[b][i] - modelVariables.actualArrival[b][i]);
            // constraint 6
            model.add(maxChargeTime * modelVariables.charge[b][i] >= modelVariables.chargeTime[b][i]);
            // constraint 7
            model.add(modelVariables.chargingStation[busSequence[i]] >= modelVariables.charge[b][i]);

            model.add(modelVariables.chargeTime[b][i] >= modelVariables.charge[b][i] * minChargeTime);

            model.add(modelVariables.nonRenewable[b][i] <= modelVariables.chargeAmount[b][i]);

            string varString = "Bus" + to_string(b) + "Station" + to_string(i);
            model = addCEWConstraints(model, env, arguments, b, i, busSequence,
                                       "Bus" + to_string(b) + "Station" + to_string(i));



        }
        if (busIndex != modelVariables.buses.getSize() - 1) {
            for (int busIndexD = busIndex + 1; busIndexD < modelVariables.buses.getSize(); busIndexD++) {
                int d = modelVariables.buses[busIndexD];
                for (int i = 0; i < busSequence.getSize(); i++) {
                    IloIntArray otherBusSequence = modelVariables.busSequences[d];
                    for (int j = 0; j < otherBusSequence.getSize(); j++) {
                        if (otherBusSequence[j] == busSequence[i] &&
                        abs(modelVariables.scheduledArrival[b][i] -
                        modelVariables.scheduledArrival[d][j]) <= (maxChargeTime + deviationTime) * 2) {
                            string varName = "busb" +  to_string(b)+"busd"+ to_string(d) +"stopi"+to_string(i)+"stopj"+to_string(j);
                            IloIntVar sameStop(env, 0, 1, (varName + "samestop").c_str());
                            // constraint 8
                            model.add(sameStop <= modelVariables.charge[b][i]);
                            // constraint 9
                            model.add(sameStop <= modelVariables.charge[d][j]);
                            // constraint 10
                            model.add(modelVariables.charge[b][i] + modelVariables.charge[d][j] <= sameStop + 1);
                            IloIntVar const11(env, 0, 1, (varName + "jbeforei").c_str());
                            IloIntVar const12(env, 0, 1, (varName + "ibeforej").c_str());
                            // constraint 11
                            model.add(modelVariables.actualArrival[b][i] >=
                                              modelVariables.actualArrival[d][j] + modelVariables.chargeTime[d][j] - bigM * const11);
                            // constraint 12
                            model.add(modelVariables.actualArrival[d][j] >=
                                              modelVariables.actualArrival[b][i] + modelVariables.chargeTime[b][i] - bigM * const12);
                            // constraint 13
                            model.add(const11 + const12 - (1 - sameStop) <= 1);
                        }
                    }
                }
            }
        }
        model.add(IloSum(modelVariables.chargeAmount[b]) >= minEnergyNeeded + minBatteryCapacity - startingCapacity);
        if(minEnergyNeeded + minBatteryCapacity - startingCapacity <= 0){
            model.add(IloSum(modelVariables.chargeAmount[b])<=0.0);
        }
        cout << "Bus: " << b << "\tTravel energy:" << minEnergyNeeded <<"\tMinBatCap: " << minBatteryCapacity
        <<"\tStarting cap:" << startingCapacity << "\tmin energy needed:" << minEnergyNeeded +
        minBatteryCapacity - startingCapacity <<endl;
    }

    for(int k=0;k<modelVariables.powerExcess.getSize();k++){
        IloNumVarArray windowTotals = IloNumVarArray(env);
        for(int busIndex = 0; busIndex<modelVariables.buses.getSize();busIndex++){
            int b = modelVariables.buses[busIndex];
            IloNumVar busTotal = IloNumVar(env, ("window"+to_string(k) + "bus"+to_string(b)).c_str());
            model.add(busTotal >= IloSum(modelVariables.windowEnergyUsed[k][b]));
            windowTotals.add(busTotal);
        }
        model.add(modelVariables.powerExcess[k][2]>=IloSum(windowTotals));

    }

    return model;
}

ModelParameters parseData(map<string, string> arguments, primitiveVariables& loadedVars){
    Parser parser;
    ModelParameters parameters;


    if(!arguments["chargingStationsFile"].empty()){
        parameters.chargingStops = parser.parseChargingStationsFile(arguments["chargingStationsFile"]);
    }

    if(arguments.find("timeWindows") != arguments.end()){
        string commaDelimiter = ",";

        while(arguments["timeWindows"].find(commaDelimiter) != string::npos){
            string timeDelimiter = "-";
            string amountDelimiter = "=";
            string window = arguments["timeWindows"].substr(0,arguments["timeWindows"].find(commaDelimiter));

            double windowStartTime = stod(window.substr(0, window.find(timeDelimiter)));
            double windowEndTime = stod(window.substr(window.find(timeDelimiter)+1));
            double windowEnergyAmount = stod(window.substr(window.find(amountDelimiter)+1));
            if(windowEnergyAmount == 0 ){
                cout << "no energy " << windowStartTime << " " << windowEndTime << " " << windowEnergyAmount << endl;
                arguments["timeWindows"].erase(0, arguments["timeWindows"].find(commaDelimiter) + commaDelimiter.length());
                continue;
            }
            CleanEnergyWindow currentWindow{.startTime=windowStartTime, .endTime=windowEndTime,
                                            .availableEnergy=windowEnergyAmount * stod(arguments["powerRatio"])};
            parameters.cleanEnergyWindows.push_back(currentWindow);
            arguments["timeWindows"].erase(0, arguments["timeWindows"].find(commaDelimiter) + commaDelimiter.length());
        }
    }
    parameters.stationData = parser.parseStopsFile(arguments["stationDataFile"]);
    parameters.numberStations = parameters.stationData.size();

    parameters.distances = parser.parseDistanceFile(arguments["stationDistanceFile"], parameters.numberStations);
    parameters = parser.parseBusData(arguments["busDataFile"], parameters);
    if(arguments.find("recalculate") != arguments.end() && arguments["recalculate"] == "true"){
        loadedVars = parser.parseSolutionFile(arguments["solutionDataFile"]);
    }
    return parameters;
}



primitiveVariables cplexToPrimitive(IloCplex cplex, IloEnv env, string method){
    primitiveVariables outputVars;
    for(int bIndex = 0; bIndex<modelVariables.buses.getSize();bIndex++){

        int b = modelVariables.buses[bIndex];
        outputVars.buses.push_back(b);
        IloNumArray valuesNonClean(env);
        IloNumArray valuesArrivalTime(env);
        IloNumArray valuesDeviationTime(env);
        IloNumArray valuesCapacity(env);
        IloNumArray valuesChargeTime(env);
        IloNumArray valuesChargeAmount(env);
        IloNumArray valuesCharge(env);

        cplex.getValues(valuesNonClean, modelVariables.nonRenewable[b]);
        cplex.getValues(valuesArrivalTime, modelVariables.actualArrival[b]);
        cplex.getValues(valuesDeviationTime, modelVariables.deviationTime[b]);
        cplex.getValues(valuesCapacity, modelVariables.batteryCapacity[b]);
        cplex.getValues(valuesChargeTime, modelVariables.chargeTime[b]);
        cplex.getValues(valuesChargeAmount, modelVariables.chargeAmount[b]);
        cplex.getValues(valuesCharge, modelVariables.charge[b]);

        map<int, vector<double>> cleanChargeTimeMap;
        map<int, vector<int>> cleanWindowChargeMap;
        for(int i=0; i<modelVariables.busSequences[b].getSize();i++) {
            outputVars.busSequences[b].push_back(modelVariables.busSequences[b][i]);
            outputVars.arrivalTime[b].push_back(valuesArrivalTime[i]);
            outputVars.scheduledTime[b].push_back(modelVariables.scheduledArrival[b][i]);
            outputVars.deviationTime[b].push_back(valuesDeviationTime[i]);
            outputVars.capacity[b].push_back(valuesCapacity[i]);
            outputVars.chargeTime[b].push_back(valuesChargeTime[i]);
            outputVars.charge[b].push_back(stoi(to_string(valuesCharge[i])));
            outputVars.nonRenewable[b].push_back(valuesNonClean[i]);
            outputVars.chargeAmount[b].push_back(valuesChargeAmount[i]);
            if(method == "SPM"){
                outputVars.ases[b].push_back(cplex.getValue(modelVariables.ases[b][i]));
                outputVars.discounts[b].push_back(cplex.getValue(modelVariables.discounts[b][i]));
            }

            vector<double> cleanChargeTimes(modelVariables.cleanChargeTime[b][i].getSize());
            vector<int> cleanWindowCharges(modelVariables.cleanWindowCharge[b][i].getSize());
            for (int k = 0; k < modelVariables.powerExcess.getSize(); k++) {

                cleanChargeTimes[k] = cplex.getValue(modelVariables.cleanChargeTime[b][i][k]);
                cleanWindowCharges[k] = cplex.getValue(modelVariables.cleanWindowCharge[b][i][k]);

            }
            outputVars.cleanChargeTime[b].push_back(cleanChargeTimes);
            outputVars.cleanWindowCharge[b].push_back(cleanWindowCharges);


        }
    }
    for(int station_i = 0; station_i<modelVariables.chargingStation.getSize(); station_i++){
        outputVars.chargingStations.push_back(modelVariables.chargingStation[station_i]);
        vector<double> stationCost(modelVariables.chargingStation.getSize());
        vector<double> stationTime(modelVariables.chargingStation.getSize());
        for(int station_j = 0; station_j < modelVariables.chargingStation.getSize(); station_j++){
            stationCost[station_j] = modelVariables.tripCost[station_i][station_j];
            stationTime[station_j] = modelVariables.tripTime[station_i][station_j];
        }
        outputVars.tripCost.push_back(stationCost);
        outputVars.tripTime.push_back(stationTime);
    }
    for(int k=0; k<modelVariables.powerExcess.getSize(); k++) {
        CleanEnergyWindow cew;
        cew.startTime = modelVariables.powerExcess[k][0];
        cew.endTime = modelVariables.powerExcess[k][1];
        cew.availableEnergy = modelVariables.powerExcess[k][2];
        outputVars.powerExcess.push_back(cew);
        map<int, vector<double>> busWindowMap;
        for (int bIndex = 0; bIndex < modelVariables.buses.getSize(); bIndex++) {
            int b = modelVariables.buses[bIndex];
            vector<double> stopWindow(modelVariables.busSequences[b].getSize());
            for (int i = 0; i < modelVariables.busSequences[b].getSize(); i++) {
                stopWindow[i] = cplex.getValue(modelVariables.windowEnergyUsed[k][b][i]);

            }
            busWindowMap[b] = stopWindow;
        }

        outputVars.windowEnergyUsed.push_back(busWindowMap);

    }
    return outputVars;

}

IloModel setPreviousValues(IloModel model, IloEnv env, primitiveVariables loadedVars, double startTime){
    for (int busIndex = 0; busIndex < loadedVars.buses.size(); busIndex++) {
        int b = loadedVars.buses[busIndex];
        for(int i = 0; i<loadedVars.busSequences[b].size();i++){
            if(loadedVars.arrivalTime[b][i] <= startTime){

                double capacity = loadedVars.capacity[b][i];
                model.add(modelVariables.actualArrival[b][i] == loadedVars.arrivalTime[b][i]);
                model.add(modelVariables.deviationTime[b][i] == loadedVars.deviationTime[b][i]);
                model.add(modelVariables.batteryCapacity[b][i] == capacity);
                model.add(modelVariables.charge[b][i] == loadedVars.charge[b][i]);

                double chargeTime = loadedVars.chargeTime[b][i];
                double chargeAmount = loadedVars.chargeAmount[b][i];
                double nonReneweable = loadedVars.nonRenewable[b][i];
                model.add(modelVariables.chargeTime[b][i] == chargeTime);
                model.add(modelVariables.chargeAmount[b][i] == chargeAmount);
                model.add(modelVariables.nonRenewable[b][i] >= nonReneweable);

            }

        }
    }
    return model;
}


void createMIPModel( primitiveVariables loadedVars, ModelParameters parameters, map<string, string> arguments){
    IloEnv env;
    try {
        IloModel model(env);

        createVariables(env, parameters, arguments);
        // cost function
        IloNumExprArg renewableSum = IloSum(modelVariables.nonRenewable[parameters.busKeys[0]]);

        for (int bIndex=1; bIndex<parameters.busKeys.size();bIndex++) {
            int b = parameters.busKeys[bIndex];
            renewableSum = renewableSum + IloSum(modelVariables.nonRenewable[b]);
        }
        model.add(IloMinimize(env, renewableSum));

        model = addConstraints(model, env, parameters, arguments);
        if(arguments["recalculate"] == "true"){
            model = setPreviousValues(model, env, loadedVars, stod(arguments["horizonStartTime"]));
        }


        time_t solverStartTime = time(0);
        cout << "Solving..." << endl;
        IloCplex cplex(model);

        ifstream f(arguments["warmingSolutionFile"].c_str());
        if(f.good()){
            cout << "Previous solution file found. Using solution warming" << endl;
//	    cplex.readStartInfo(arguments["warmingSolutionFile"].c_str());
	    cplex.readSolution(arguments["warmingSolutionFile"].c_str());
        }
        else{
            cout << "No previous solution file found" << endl;
        }
        f.close();
        cplex.setParam(IloCplex::Param::MIP::Display, 3);
        cplex.setParam(IloCplex::Param::MIP::Tolerances::Integrality, 0.0);
//        cplex.setParam(IloCplex::Param::Simplex::Tolerances::Feasibility, 1e-07);
        cout << "Number of constraints: " << cplex.getNrows() << endl;
        cplex.exportModel("model.lp");
        if(stoi(arguments["maxSolutions"]) > 0){
            cplex.setParam(IloCplex::Param::MIP::Limits::Solutions, stoi(arguments["maxSolutions"]));
        }
        if(stoi(arguments["timeout"]) > 0){
            cplex.setParam(IloCplex::Param::TimeLimit, stoi(arguments["timeout"]));
        }
        ofstream myFile;
        if(arguments.find("logFile") != arguments.end()){
            myFile.open(arguments["logFile"]);
            cplex.setOut(myFile);
        }
        if (!cplex.solve()) {

            cout << cplex.getCplexStatus() << endl;
            env.error() << "ERROR FAILED TO SOLVE" << endl;
        } else {
            time_t solverEndTime = time(0);
            long elapsedTime = solverEndTime - solverStartTime;
            primitiveVariables outputVariables = cplexToPrimitive(cplex, env, arguments["method"]);
            cplex.writeSolution(arguments["LPFile"].c_str());
            Output printer;
            printer.printResults(outputVariables, parameters.stationData,
                                 elapsedTime, stod(arguments["horizonStartTime"]), stod(arguments["horizonEndTime"]),
                                 cplex.getObjValue(),to_string(cplex.getStatus()), cplex.getMIPRelativeGap(),
                                 arguments["method"]);
            printer.writeSolutionFile(outputVariables, arguments["solutionSaveFile"]);
        }
        if(arguments.find("logFile") != arguments.end()){
            myFile.close();
        }

    }
    catch (IloException &e) {
        cerr << "Concert exception caught:" << e << endl;
    }
    catch (...) {
        cerr << "Unknown error" << endl;
    }
    env.end();
}


int main(int argc, char *argv[]) {

    Parser myParse;
    map<string, string> arguments = myParse.parseArguments(argc, argv);
    for(auto & keyVal: arguments){
        cout << keyVal.first << ":" << keyVal.second<<endl;
    }

    primitiveVariables loadedVars;
    ModelParameters parameters = parseData(arguments, loadedVars);
    createMIPModel(loadedVars, parameters, arguments);


    return 0;

}
