#!/bin/sh
# Declare the data-sets and their location on the computer drive
locations=("location_1" "location_2")
locationPaths=("path_to_location_1_data" "path_to_location_2_data")

# Declare the values for C_max to explore
maximumBatteryCapacities=(120 240)

# Declare the value for C_min
minimumBatteryCapacity=12

# Declare the values for Beta
maximumChargeTimes=(0.16 0.32)

# Declare the values for gamma
minChargeTime=0.0166

# Declare the values for \delta t_bi
deviationTimes=(0.0833 0.1666)

busSpeed=35

# Declare the start and end times of each horizon/checkpoint
horizonStartTimes=(0 6 12 18)
horizonEndTimes=(6 12 18 24)

# Declare the dates from which the CEW information will be taken from
dates=(14 15 16)
year=2022
month=2

# Set a timeout for the search process
timeout=720

# Set an exit condition for maximum allowed solutions. 0 = no limit
maxSolutions=0

# Set the starting capacity (in kWh) for each bus
startingCapacity=30

# Declare the types of what knowledge the optimizer has about the availability of clean energy in the future
datatypes=("noClean" "ideal" "predicted") # no clean is Gamma = 0

# Declare the power split/ratio which each city receives from the total amount of clean energy available
powerRatios=(0.6 0.4)


folderName="save_folder_location"

# the warming solution file (optional)
warmingSolutionFile="solFileOPL"

# The methods used by the optimizer. Please refer to WP5-D2 for details
methods=("MPM" "SPM")

# The placement of charging stations as described in WP5-D3
chargeStationPlacement="Inf-A"

# The name of the file which previous solutions are saved to. Needed for recalculating schedules
solutionSaveFile="scheduleDetails"

# The charge rate (R) used to charge buses in kWh
chargeRate=600

# A sufficiently large constant to deal with time related constraints.
bigM=25

# The rate at which energy is expended for eBuses (in kWh per km)
busEnergyCost=1.0

# Name of the output log file
logFile="logFile.txt"

# Name of the file where a solution is stored, used for warming solutions when recalculating new schedules
LPFile="solution.lp"

# the name of the file containing the bus route information
busDataFile="buses_schedule_input_file.json"

# the name of the file containing the distances between all stations
stationDistanceFile="station_distances_input.csv"

# the name of the file containing the names of all bus stops
stationDataFile="stations_input.csv"

# The name of the file containing the charging stations for a particular city
chargingStationsFile="_charging_stations.txt"

# the path to the folder containing the CEW information.
cew_folder="path_to_cew_files"

# the discount factor (\phi) used for SPM
discountFactor=0.01

mkdir -p ./"${folderName}"
printf "Minimum Capacity:${minimumBatteryCapacity}\nMinimum charge time:${minChargeTime}\nBus speed:${busSpeed}
Timeout:${timeout}\nStatring capacity:${startingCapacity}\nwarmingSolutionFile:${warmingSolutionFile}
Charging station placement:${chargeStationPlacement}\nSolution save file:${solutionSaveFile}\nCharge rate:${chargeRate}
Big M:${bigM}\nBus energy cost per km:${busEnergyCost}\nLpFile:${LPFile}\nBus data file:${busDataFile}
station Distance file:${stationDistanceFile}\nstation data file:${stationDataFile}\n
discount Factor:${discountFactor}" > ${folderName}/settings.txt

for method in "${methods[@]}";
do
	for ((l=0; l<"${#locations[@]}"; l++))
	do
		location="${locations[$l]}"
		path="${locationPaths[$l]}"
		powerRatio="${powerRatios[$l]}"
		for datatype in "${datatypes[@]}";
		do
			for ((bi=0; bi<"${#maximumBatteryCapacities[@]}"; bi++))
			do
				maxBatteryCapacity="${maximumBatteryCapacities[$bi]}"
				minBatteryCapacity="${minimumBatteryCapacity}"
				maxChargeTime="${maximumChargeTimes[$bi]}"
				for deviationTime in "${deviationTimes[@]}";
				do
					for date in "${dates[@]}";
					do
						for((t=0; t<"${#horizonStartTimes[@]}"; t++))
						do
							horizonStartTime="${horizonStartTimes[$t]}"
							horizonEndTime="${horizonEndTimes[$t]}"
							if [[ "$datatype" != "predicted"  && "$method" != "single-period model" ]]
              then
              				if [[ "$horizonStartTime" = 0 ]]
              				then
              				  horizonEndTime=24
              				else
              				  continue
              				fi
              fi
							if [[ "$datatype" = "noClean" ]]
							then
							  timeWindows="18.00-24.00=0,"
                if [[ ${date} != "${dates[0]}" ]]
                then
                    continue
                fi

							else
								dataFile="${cew_folder}/${datatype}/${method}/${year}-${month}-${date}-${horizonStartTime}.txt"
								while read timeWindows;
								do
									echo "CEW: " "$timeWindows"
								done < "${dataFile}"
							fi

							resultDir="${date}"_"${location}"_"${maxBatteryCapacity}"_"${deviationTime}"_"${busSpeed}"_"${horizonStartTime}"_"${powerRatio}"
							mkdir -p ./"${folderName}"/"${location}"/"${datatype}"/"${method}"/"${resultDir}"
							mkdir -p "code"/"release-build"
       							cd code/release-build
							cmake -DCMAKE_BUILD_TYPE=Release ..
							cmake --build . --config Release
							if(($t > 0))
							then
							  # If we are recalculating the schedule then execute this
								./scheduler --discountFactor "${discountFactor}" --busEnergyCost "${busEnergyCost}" --chargeRate "${chargeRate}" --bigM "${bigM}" --maxSolutions $maxSolutions --LPFile $LPFile --timeout $timeout --solutionSaveFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/${solutionSaveFile}  --maxBatteryCapacity "${maxBatteryCapacity}" --minBatteryCapacity "${minBatteryCapacity}" --deviationTime "${deviationTime}" --timeWindows "${timeWindows}" --busSpeed "$busSpeed" --busDataFile "${path}"/${busDataFile} --stationDataFile "${path}"/${stationDataFile} --stationDistanceFile "${path}"/"${stationDistanceFile}" --logFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/"${logFile}" --horizonStartTime "${horizonStartTime}" --startingCapacity "${startingCapacity}" --chargingStationsFile ../../charging_station_locations/"${chargeStationDistance}"/"${location}""${chargingStationsFile}" --powerRatio "${powerRatio}" --maxChargeTime "${maxChargeTime}" --minChargeTime "${minChargeTime}" --location "${location}" --horizonEndTime "${horizonEndTime}" --method "${method}" --solutionDataFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${date}"_"${location}"_"${maxBatteryCapacity}"_"${deviationTime}"_"${busSpeed}"_"${horizonStartTimes[$t-1]}"_"${powerRatio}"/${solutionSaveFile} --recalculate "true" --warmingSolutionFile ${LPFile} > ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/result.txt
							else
							  # Otherwise if its the first time we are calculating the schedule for this configuration execute this
							  ./scheduler --discountFactor "${discountFactor}" --busEnergyCost "${busEnergyCost}" --chargeRate "${chargeRate}" --bigM "${bigM}" --maxSolutions $maxSolutions --LPFile $LPFile --timeout $timeout --solutionSaveFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/${solutionSaveFile}  --maxBatteryCapacity "${maxBatteryCapacity}" --minBatteryCapacity "${minBatteryCapacity}" --deviationTime "${deviationTime}" --timeWindows "${timeWindows}" --busSpeed "$busSpeed" --busDataFile "${path}"/${busDataFile} --stationDataFile "${path}"/${stationDataFile} --stationDistanceFile "${path}"/"${stationDistanceFile}" --logFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/"${logFile}" --horizonStartTime "${horizonStartTime}" --startingCapacity "${startingCapacity}" --chargingStationsFile ../../charging_station_locations/"${chargeStationDistance}"/"${location}""${chargingStationsFile}" --powerRatio "${powerRatio}" --maxChargeTime "${maxChargeTime}" --minChargeTime "${minChargeTime}" --location "${location}" --horizonEndTime "${horizonEndTime}" --method "${method}" --warmingSolutionFile ../../warming_solutions/"${location}"/"${chargeStationDistance}"/"${method}"/"${warmingSolutionFile}" > ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/result.txt
							fi
							cd ../..
						done
					done
				done
			done
		done
	done
done
rm code/release-build/model.lp
rm code/release-build/solution.lp
find . -name scheduleDetails -type f -delete
