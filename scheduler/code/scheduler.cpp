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
    /// x_i Binary variable which is 1 if charging station is install at station i
    IloIntArray chargingStation;

    /// B set of available buses
    IloIntArray buses;

    /// nc_bi amount of non-clean energy (in kWh) used by bus b at stop i
    map<int, IloNumVarArray> nonRenewable;

    /// S set of stop sequences for each bus
    map<int, IloIntArray> busSequences;

    /// \Tau_bi scheduled arrival time (in hour decimal) of bus b at stop j
    map<int, IloNumArray> scheduledArrival;

    /// t_bi actual arrival time (in hour decimal) of bus b at stop j
    map<int, IloNumVarArray> actualArrival;

    /// delta tbi difference between actual arrival time and original schedule time of bus b at stop j
    map<int, IloNumVarArray> deviationTime;

    /// c_bi amount of capacity (in kWh) bus b has at stop i
    map<int, IloNumVarArray> batteryCapacity;

    /// e_bi amount of energy gained (in kWh) by bus b at stop i
    map<int, IloNumVarArray> chargeAmount;

    /// ct_bi charge time (in hour decimal) of bus b at stop i
    map<int, IloNumVarArray> chargeTime;

    /// x_bi binary variable assigned 1 if bus b charges at stop i
    map<int, IloIntVarArray> charge;

    /// T_ij amount of time required for a trip between stations i and j
    IloArray<IloNumArray> tripTime;

    /// \Gamma_k information about the k^th Clean Energy Window
    IloArray<IloNumArray> powerExcess;

    /// Dij amount of energy required for a trip between stations i and j
    IloArray<IloNumArray> tripCost;

    /// ce_kbi amount of clean energy used in CEW k by bus b at stop i
    vector<map<int, IloNumVarArray>> windowEnergyUsed;

    /// ase_bi a binary variable assigned 1 if bus b arrives at stop i before the end of the current checkpoint (Omega)
    map<int, IloIntVarArray> ases;

    /// r_bi the reduction in energy (in kWh) given to charges after the current checkpoint.
    map<int, IloNumVarArray> discounts;

    /// \Omega the time which the current horizon/checkpoint ends
    IloNum horizonEndTime;

    /// wt_bik the time (in hour decimal) bus b spends charging at stop i using clean energy from CEW k
    map<int, vector<IloNumVarArray>> cleanChargeTime;

    /// kt_bik binary variable assigned 1 if bus b charges at stop i during CEW k
    map<int, vector<IloIntVarArray>> cleanWindowCharge;

}modelVariables;

/// create the variables used for the MIP model constraints
void createVariables(IloEnv env, ModelParameters parameters, map<string, string> arguments) {

    double deviationTime = stod(arguments["deviationTime"]);
    double maxChargeTime = stod(arguments["maxChargeTime"]);
    double chargeRate = stod(arguments["chargeRate"]);
    double maxBatteryCapacity = stod(arguments["maxBatteryCapacity"]);
    double minBatteryCapacity = stod(arguments["minBatteryCapacity"]);

    modelVariables.horizonEndTime = IloNum(stod(arguments["horizonEndTime"]));
    modelVariables.buses = IloIntArray (env);
    modelVariables.chargingStation = IloIntArray (env, parameters.numberStations);

    /// assign values of X_i as CPLEX variables
    for(int i=0;i<parameters.chargingStops.size();i++){
        modelVariables.chargingStation[i] = parameters.chargingStops[i];
    }

    /// not all CEW have to be considered, ones that occur before the first bus are removed.
    modelVariables.powerExcess = IloArray<IloNumArray>(env);
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

    /// create variables associated with each bus
    for(int b : parameters.busKeys){
        int numStops = parameters.busSequencesRaw[b].size();
        modelVariables.buses.add(b);
        IloIntArray busBSequence = IloIntArray(env, numStops);
        IloNumArray busBTimes = IloNumArray(env, numStops);
        IloNumVarArray busBActualArrivalTimeVars(env, numStops);
        IloNumVarArray busBDeviation(env, numStops);
        IloNumVarArray busBBatteryCapacities(env, numStops);
        IloNumVarArray busBChargeTime(env, numStops);
        IloIntVarArray busBCharge(env, numStops);
        IloNumVarArray busBChargeAmount(env, numStops);
        IloNumVarArray busBNonRenewable(env, numStops);
        IloIntVarArray busBAses(env, numStops);
        vector<IloNumVarArray> stopWindowTime(numStops);
        vector<IloIntVarArray> stopWindowCharge(numStops);

        IloNumVarArray discount(env, numStops);

        for(int i=0; i<numStops; i++){
            string varString = "Bus" + to_string(b) + "SequenceStop" + to_string(i);
            /// assign the ID of the ith stop of bus b
            busBSequence[i] = parameters.busSequencesRaw[b][i];

            /// assign the value of \tau _bi
            busBTimes[i] = parameters.busTimeRaw[b][i];

            /// create the variable for t_bi
            busBActualArrivalTimeVars[i] = IloNumVar(env, 0.0, 24.00,
                                                     (varString + "ArrivalTime").c_str());

            /// create the variable for \delta t_bi
            busBDeviation[i] = IloNumVar(env, 0.0, deviationTime,
                                          (varString + "DeltaTime").c_str());

            /// create the variable for c_bi
            busBBatteryCapacities[i] = IloNumVar(env, minBatteryCapacity, maxBatteryCapacity,
                                                 (varString + "BatteryCapacity").c_str());
            /// create the variable for ct_bi
            busBChargeTime[i] = IloNumVar(env, 0.0, maxChargeTime, (varString + "ChargeTime").c_str());

            /// create the variable for x_bi
            busBCharge[i] = IloIntVar(env, 0, 1, (varString + "Charge").c_str());

            /// create the variable for e_bi
            busBChargeAmount[i] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                            (varString + "chargeAmount").c_str());

            /// create variable for nc_bi
            busBNonRenewable[i] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                         (varString + "nonRenewable").c_str());

            /// create variable for ase_bi
            busBAses[i] = IloIntVar(env, 0, 1, (varString + "ase").c_str());

            /// create variables for the individual CEW's
            IloNumVarArray cleanEnergyTime(env,  modelVariables.powerExcess.getSize());
            IloIntVarArray cleanEnergyCharge(env, modelVariables.powerExcess.getSize());
            discount[i] = IloNumVar(env, 0, maxChargeTime * chargeRate, (varString+"Discount").c_str());
            for(int k=0;k<modelVariables.powerExcess.getSize();k++){
                /// create variable for wt_bik
                cleanEnergyTime[k] = IloNumVar(env, 0.0, maxChargeTime,
                                               (varString+"CleanEnergyTime"+ to_string(k)).c_str());

                /// create variable for kt_bik
                cleanEnergyCharge[k] = IloIntVar(env, 0, 1,
                                                 (varString+"CleanEnergyCharge"+ to_string(k)).c_str());
            }
            stopWindowTime[i] = cleanEnergyTime;
            stopWindowCharge[i] = cleanEnergyCharge;
        }
        modelVariables.busSequences[b] = busBSequence;
        modelVariables.scheduledArrival[b] = busBTimes;
        modelVariables.actualArrival[b] = busBActualArrivalTimeVars;
        modelVariables.deviationTime[b] = busBDeviation;
        modelVariables.batteryCapacity[b] = busBBatteryCapacities;
        modelVariables.chargeAmount[b] = busBChargeAmount;
        modelVariables.chargeTime[b] = busBChargeTime;
        modelVariables.charge[b] = busBCharge;
        modelVariables.nonRenewable[b] = busBNonRenewable;
        modelVariables.ases[b] = busBAses;
        modelVariables.cleanChargeTime[b] = stopWindowTime;
        modelVariables.cleanWindowCharge[b] = stopWindowCharge;

        modelVariables.discounts[b] = discount;
    }
    /// assign D_ij
    modelVariables.tripCost = IloArray<IloNumArray>(env, parameters.numberStations);

    /// assign T_ij
    modelVariables.tripTime = IloArray<IloNumArray> (env, parameters.numberStations);

    for (int i = 0; i < parameters.numberStations; i++) {
        IloNumArray ijCost = IloNumArray(env, parameters.numberStations);
        IloNumArray ijTime = IloNumArray(env, parameters.numberStations);
        for (int j = 0; j < parameters.numberStations; j++) {
            double cost = parameters.distances[i][j] * stod(arguments["busEnergyCost"]);
            /// D_ij is the distance between two stops multiplied by the energy consumption per km
            ijCost[j]=cost;
            ijCost[j] = parameters.distances[i][j] * stod(arguments["busEnergyCost"]);

            /// T_ij is the distance / (time * speed) formula using the distance between ij and the bus speed.
            ijTime[j] = ((60 / stod(arguments["busSpeed"])) * parameters.distances[i][j]) / 60;

        }
        modelVariables.tripCost[i] = ijCost;
        modelVariables.tripTime[i] = ijTime;
    }

    /// assign the values for \Gamma_k
    for(int k=0;k<modelVariables.powerExcess.getSize();k++){
        map<int, IloNumVarArray> stopWindowEnergy;

        for(int b : parameters.busKeys){

            int numStops = modelVariables.busSequences[b].getSize();
            IloNumVarArray windowEnergy(env, numStops);
            for(int j=0;j<numStops;j++){

                /// assign the variable to determine how much energy was used for each CEW.
                windowEnergy[j] = IloNumVar(env, 0.0, maxChargeTime * chargeRate,
                                            ("window"+to_string(modelVariables.powerExcess[k][0])+
                                            "to"+to_string(modelVariables.powerExcess[k][1])+"bus"+to_string(b)
                                            +"stop"+to_string(j)).c_str());
            }
            stopWindowEnergy[b] = windowEnergy;
        }

        modelVariables.windowEnergyUsed.push_back(stopWindowEnergy);
    }
    cout << "Clean Energy Windows: " << modelVariables.powerExcess << endl;


}

/// create the constraints for the CEW's
IloModel addCEWConstraints(IloModel model, IloEnv env, map<string, string> arguments, int b, int index,
                           IloIntArray busSequence, string varString){

    double maxChargeTime = stod(arguments["maxChargeTime"]);
    double chargeRate = stod(arguments["chargeRate"]);
    int bigM = stoi(arguments["bigM"]);
    double deviationTime = stod(arguments["deviationTime"]);
    string method = arguments["method"];

    if(method == "SPM"){
        /// Constraint 2.1 WP5-D2
        model.add(modelVariables.actualArrival[b][index] + (bigM * modelVariables.ases[b][index]) >= modelVariables.horizonEndTime );

        /// Constraint 2.2 WP5-D2
        model.add(modelVariables.discounts[b][index] <= modelVariables.chargeAmount[b][index] * stod(arguments["discountFactor"]));

        /// Constraint 2.3 WP5-D2
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
            /// Constraint 3.16 WP5-D1
            model.add(modelVariables.actualArrival[b][index] + modelVariables.chargeTime[b][index] +
                      (bigM * (1 - modelVariables.cleanWindowCharge[b][index][k])) >= modelVariables.powerExcess[k][0]);

            /// Constraint 3.17 WP5-D1
            model.add(modelVariables.actualArrival[b][index] -
                      (bigM * (1-modelVariables.cleanWindowCharge[b][index][k])) <= modelVariables.powerExcess[k][1]);

            /// Constraint 3.18 WP5-D1
            model.add(modelVariables.cleanWindowCharge[b][index][k] >= modelVariables.cleanChargeTime[b][index][k]);

            /// Constraint 3.19 WP5-D1
            model.add(modelVariables.cleanWindowCharge[b][index][k]*chargeRate >= modelVariables.windowEnergyUsed[k][b][index]);

            /// Constraint 3.20 WP5-D1
            model.add(modelVariables.charge[b][index] >= modelVariables.cleanWindowCharge[b][index][k]);

            /// Constraint 3.21 WP5-D1
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

            /// Constraint 3.22 WP5-D1
            model.add(modelVariables.actualArrival[b][index] + modelVariables.chargeTime[b][index] -
                      modelVariables.powerExcess[k][0] + (bigM * (1-modelVariables.cleanWindowCharge[b][index][k])) >=
                      modelVariables.cleanChargeTime[b][index][k]);

            /// Constraint 3.23 WP5-D1
            model.add(modelVariables.windowEnergyUsed[k][b][index] <= modelVariables.cleanChargeTime[b][index][k]
                                                                      * chargeRate);

            windowTimeValues.add(modelVariables.windowEnergyUsed[k][b][index]);

        }

        /// Constraint 3.24 WP5-D1
        model.add(IloSum(modelVariables.cleanChargeTime[b][index]) <= modelVariables.chargeTime[b][index]);

        /// Setting the upper bounds for the amount of energy charged during CEWs
        model.add(IloSum(windowTimeValues) <= modelVariables.chargeAmount[b][index]);


        if(method == "SPM"){
            /// Constraint 2.4 of WP5-D2
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - IloSum(windowTimeValues) - modelVariables.discounts[b][index]);
        }
        else{
            /// Constraint 3.25 WP5-D1
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - IloSum(windowTimeValues));
        }

    }
    else{
        if(method == "SPM"){
            /// Set the lower bound for non-clean energy if there is no charging station/CEW
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index] - modelVariables.discounts[b][index]);
        }
        else{
            /// Constraint 3.26 WP5-D1
            model.add(modelVariables.nonRenewable[b][index] >= modelVariables.chargeAmount[b][index]);
        }

        for(int k = 0; k < modelVariables.powerExcess.getSize(); k++) {
            model.add(modelVariables.windowEnergyUsed[k][b][index] + modelVariables.cleanChargeTime[b][index][k] +
                          modelVariables.cleanWindowCharge[b][index][k] <= 0.0);
        }
    }
    return model;
}

/// create the constraints for the MIP model
IloModel addConstraints(IloModel model, IloEnv env, ModelParameters parameters, map<string, string> arguments) {
    double minChargeTime = stod(arguments["minChargeTime"]);
    double maxChargeTime = stod(arguments["maxChargeTime"]);
    double chargeRate = stod(arguments["chargeRate"]);
    double startingCapacity = stod(arguments["startingCapacity"]);
    int bigM = stoi(arguments["bigM"]);
    double maxBatteryCapacity = stod(arguments["maxBatteryCapacity"]);
    double minBatteryCapacity = stod(arguments["minBatteryCapacity"]);
    double deviationTime = stod(arguments["deviationTime"]);

    for (int busIndex=0;busIndex<modelVariables.buses.getSize();busIndex++ ) {
        int b = modelVariables.buses[busIndex];
        vector<int> busRests = parameters.rests[b];
        IloIntArray busSequence = modelVariables.busSequences[b];
        double minEnergyNeeded = 0.0;

        /// create the constraints for the first stop of b
        /// Constraint 3.1  WP5-D1 For first stop the capacity must be equal to the starting capacity. Thus it cannot be below the minimum battery capacity
        model.add(modelVariables.batteryCapacity[b][0] + modelVariables.chargeAmount[b][0] <= maxBatteryCapacity);
        model.add(modelVariables.batteryCapacity[b][0] == startingCapacity);

        /// Constraint 3.2 WP5-D1
        model.add(modelVariables.chargeTime[b][0] <= modelVariables.charge[b][0] * maxChargeTime);

        /// Constraint 3.3 WP5-D1
        model.add(modelVariables.chargingStation[busSequence[0]] >= modelVariables.charge[b][0]);

        /// Constraint 3.4 WP5-D1
        model.add(modelVariables.chargeAmount[b][0] <= modelVariables.chargeTime[b][0] * chargeRate);

        /// Constraint 3.5 WP5-D1
        model.add(modelVariables.chargeTime[b][0] >= modelVariables.charge[b][0] * minChargeTime);

        /// Constraint 3.8/3.9 WP5-D1 For the first stop it is assumed that there is no deviation from the original schedule
        model.add(modelVariables.deviationTime[b][0] <= 0.0);
        model.add(modelVariables.actualArrival[b][0] == modelVariables.scheduledArrival[b][0]);

        /// Constraint 3.26 WP5-D1
        model.add(modelVariables.nonRenewable[b][0] <= modelVariables.chargeAmount[b][0]);


        /// Add the CEW constraints for the first stop
        model = addCEWConstraints(model, env, arguments, b, 0, busSequence, "Bus" + to_string(b) + "Station" + to_string(0));

        /// create constraints for the rest of the bus stops.
        for (int i = 1; i < busSequence.getSize(); i++) {
            int j = i - 1;

            minEnergyNeeded += modelVariables.tripCost[busSequence[i]][busSequence[j]];

            /// Constraint 3.1 WP5-D1
            model.add(modelVariables.batteryCapacity[b][i] >= minBatteryCapacity);
            model.add(modelVariables.batteryCapacity[b][i] + modelVariables.chargeAmount[b][i] <= maxBatteryCapacity);

            /// Constraint 3.2 WP5-D1
            model.add(maxChargeTime * modelVariables.charge[b][i] >= modelVariables.chargeTime[b][i]);

            /// Constraint 3.3 WP5-D1
            model.add(modelVariables.chargingStation[busSequence[i]] >= modelVariables.charge[b][i]);

            /// Constraint 3.4 WP5-D1
            model.add(modelVariables.chargeAmount[b][i] <= modelVariables.chargeTime[b][i] * chargeRate);

            /// Constraint 3.5 WP5-D1
            model.add(modelVariables.chargeTime[b][i] >= modelVariables.charge[b][i] * minChargeTime);

            /// Constraint 3.6 WP5-D1
            model.add(modelVariables.batteryCapacity[b][i] <=
                          modelVariables.batteryCapacity[b][j] + modelVariables.chargeAmount[b][j] - modelVariables.tripCost[busSequence[i]][busSequence[j]]);

            /// Constraint 3.7 WP5-D1
            /// in some cases the bus schedule expects buses to travel at extremely high speeds to reach the next stop when adhering to the original schedule (i.e., traveling at 77 km/h).
            /// it is assumed there is some issue with this, as a result it is assumed the travel time from ij in this situation is the difference between the scheduled times.
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

            /// If a driver rest is required then we enforce that there must be no deviation in arrival time for the following stop
            if(busRests[j] == 1 && busSequence[i] == busSequence[j]){
                model.add(modelVariables.deviationTime[b][i] <= 0.0);
            }

            /// Constraint 3.8 WP5-D1
            model.add(modelVariables.deviationTime[b][i] >= modelVariables.actualArrival[b][i] - modelVariables.scheduledArrival[b][i]);
            /// Constraint 3.9 WP5-D1
            model.add(modelVariables.deviationTime[b][i] >= modelVariables.scheduledArrival[b][i] - modelVariables.actualArrival[b][i]);

            /// Constraint 3.26 WP5-D1
            model.add(modelVariables.nonRenewable[b][i] <= modelVariables.chargeAmount[b][i]);

            string varString = "Bus" + to_string(b) + "Station" + to_string(i);

            /// Add the CEW constraints for the current stop
            model = addCEWConstraints(model, env, arguments, b, i, busSequence,
                                       "Bus" + to_string(b) + "Station" + to_string(i));



        }
        /// add the non-overlapping constraints
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

                            /// Constraint 3.10 WP5-D1
                            model.add(sameStop <= modelVariables.charge[b][i]);

                            /// Constraint 3.11 WP5-D1
                            model.add(sameStop <= modelVariables.charge[d][j]);

                            /// Constraint 3.12 WP5-D1
                            model.add(modelVariables.charge[b][i] + modelVariables.charge[d][j] <= sameStop + 1);
                            IloIntVar const11(env, 0, 1, (varName + "jbeforei").c_str());
                            IloIntVar const12(env, 0, 1, (varName + "ibeforej").c_str());

                            /// Constraint 3.13 WP5-D1
                            model.add(modelVariables.actualArrival[b][i] >=
                                              modelVariables.actualArrival[d][j] + modelVariables.chargeTime[d][j] - bigM * const11);

                            /// Constraint 3.14 WP5-D1
                            model.add(modelVariables.actualArrival[d][j] >=
                                              modelVariables.actualArrival[b][i] + modelVariables.chargeTime[b][i] - bigM * const12);

                            /// Constraint 3.15 WP5-D1
                            model.add(const11 + const12 - (1 - sameStop) <= 1);
                        }
                    }
                }
            }
        }

        /// a simplification. A bus needs as much energy as is needed to reach the end of their route minus the min battery capacity and starting capacity
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
        /// Constraint 3.27 WP5-D1
        model.add(modelVariables.powerExcess[k][2]>=IloSum(windowTotals));

    }

    return model;
}

/// load information from a number of files
ModelParameters parseData(map<string, string> arguments, primitiveVariables& loadedVars){
    Parser parser;
    ModelParameters parameters;

    /// load the value of X_i
    if(!arguments["chargingStationsFile"].empty()){
        parameters.chargingStops = parser.parseChargingStationsFile(arguments["chargingStationsFile"]);
    }

    /// parse the command line argument for information about CEW
    if(arguments.find("CEW") != arguments.end()){
        string commaDelimiter = ",";

        while(arguments["CEW"].find(commaDelimiter) != string::npos){
            /// the start time and end time of a CEW are seperated by "-", the amount of excess clean energy is
            /// preceded by "=", and each CEW is terminated with a ","
            string timeDelimiter = "-";
            string amountDelimiter = "=";
            string window = arguments["CEW"].substr(0,arguments["CEW"].find(commaDelimiter));

            double windowStartTime = stod(window.substr(0, window.find(timeDelimiter)));
            double windowEndTime = stod(window.substr(window.find(timeDelimiter)+1));
            double windowEnergyAmount = stod(window.substr(window.find(amountDelimiter)+1));
            /// if there is no excess clean energy available for the current CEW then it is removed.
            if(windowEnergyAmount == 0 ){
                cout << "no energy " << windowStartTime << " " << windowEndTime << " " << windowEnergyAmount << endl;
                arguments["CEW"].erase(0, arguments["CEW"].find(commaDelimiter) + commaDelimiter.length());
                continue;
            }
            CleanEnergyWindow currentWindow{.startTime=windowStartTime, .endTime=windowEndTime,
                                            .availableEnergy=windowEnergyAmount * stod(arguments["powerRatio"])};
            parameters.cleanEnergyWindows.push_back(currentWindow);
            arguments["CEW"].erase(0, arguments["CEW"].find(commaDelimiter) + commaDelimiter.length());
        }
    }

    /// load station name
    parameters.stationData = parser.parseStopsFile(arguments["stationDataFile"]);
    parameters.numberStations = parameters.stationData.size();

    /// load the distance between each station (D_ij)
    parameters.distances = parser.parseDistanceFile(arguments["stationDistanceFile"],
                                                    parameters.numberStations);

    /// load bus route information (i.e., number of buses, their route etc)
    parameters = parser.parseBusData(arguments["busDataFile"], parameters);

    /// loads the values of previous solution when recalculating a schedule.
    if(arguments.find("recalculate") != arguments.end() && arguments["recalculate"] == "true"){
        loadedVars = parser.parseSolutionFile(arguments["solutionDataFile"]);
    }
    return parameters;
}


/// converts the CPLEX variables into primitives (i.e., int, float, bool etc) for printing.
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
            /// ase and r_bi are only assigned values if SPM is used.
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

/// sets the values of variables which occur before the current checkpoint denoted by startTime.
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

        /// create the variables used in the CPLEX/MIP model
        createVariables(env, parameters, arguments);

        /// Set up the calculations for minimizing the total amount of non-clean energy consumed.
        IloNumExprArg nonCleanSum = IloSum(modelVariables.nonRenewable[parameters.busKeys[0]]);
        for (int bIndex=1; bIndex<parameters.busKeys.size();bIndex++) {
            int b = parameters.busKeys[bIndex];
            nonCleanSum = nonCleanSum + IloSum(modelVariables.nonRenewable[b]);
        }

        /// objective function
        model.add(IloMinimize(env, nonCleanSum));

        /// create the constraints used in the MIP model
        model = addConstraints(model, env, parameters, arguments);

        /// if we want to recalculate the schedule for the current day then we must assign the values of variables which
        /// occur before the current checkpoint start
        if(arguments["recalculate"] == "true"){
            model = setPreviousValues(model, env, loadedVars, stod(arguments["horizonStartTime"]));
        }


        time_t solverStartTime = time(0);
        cout << "Solving..." << endl;
        IloCplex cplex(model);

        /// use a warming solution if one is available.
        ifstream f(arguments["warmingSolutionFile"].c_str());
        if(f.good()){
            cout << "Previous solution file found. Using solution warming" << endl;
	    cplex.readSolution(arguments["warmingSolutionFile"].c_str());
        }
        else{
            cout << "No previous solution file found" << endl;
        }
        f.close();

        /// set some parameters for CPLEX.
        cplex.setParam(IloCplex::Param::MIP::Display, 3);
        cplex.setParam(IloCplex::Param::MIP::Tolerances::Integrality, 0.0);
        cout << "Number of constraints: " << cplex.getNrows() << endl;

        /// export the created MIP model for debugging purposes, this should be deleted if not used as it might take up a large amount of space.
        cplex.exportModel("model.lp");

        /// set some conditions for ending search early
        if(stoi(arguments["maxSolutions"]) > 0){
            cplex.setParam(IloCplex::Param::MIP::Limits::Solutions, stoi(arguments["maxSolutions"]));
        }
        if(stoi(arguments["timeout"]) > 0){
            cplex.setParam(IloCplex::Param::TimeLimit, stoi(arguments["timeout"]));
        }

        /// save the infromation about the search process into a logFile
        ofstream myFile;
        if(arguments.find("logFile") != arguments.end()){
            myFile.open(arguments["logFile"]);
            cplex.setOut(myFile);
        }

        /// begin the search process
        if (!cplex.solve()) {
            cout << cplex.getCplexStatus() << endl;
            env.error() << "ERROR FAILED TO SOLVE" << endl;
        } else {
            time_t solverEndTime = time(0);
            long elapsedTime = solverEndTime - solverStartTime;

            /// convert the values of the CPLEX variables for the best solution into basic data-types (i.e., int, float)
            primitiveVariables outputVariables = cplexToPrimitive(cplex, env, arguments["method"]);

            /// write the solution into a LP file. This LP file can then be loaded to be used as a warming solution laer.
            cplex.writeSolution(arguments["LPFile"].c_str());

            /// print the results of the experiment
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
    /// read the command line arguments and print them
    map<string, string> arguments = myParse.parseArguments(argc, argv);
    for(auto & keyVal: arguments){
        cout << keyVal.first << ":" << keyVal.second<<endl;
    }

    /// load the data-set
    primitiveVariables loadedVars;
    ModelParameters parameters = parseData(arguments, loadedVars);

    /// generate the CPLEX model, add constraints, and execute search.
    createMIPModel(loadedVars, parameters, arguments);

    return 0;

}
