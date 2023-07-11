#!/bin/sh
locations=("location_1" "location_2")
locationPaths=("path_to_location_1_data" "path_to_location_2_data")
maximumBatteryCapacities=(120 240)
minimumBatteryCapacity=12
maximumChargeTimes=(0.16 0.32)
minChargeTime=0.0166
deviationTimes=(0.0833 0.1666)
busSpeed=35
horizonStartTimes=(0 6 12 18)
horizonEndTimes=(6 12 18 24)
dates=(14 15)
year=2022
month=2
timeout=720
datatypes=("noClean" "ideal" "predicted") # no clean === Gamma = 0
powerRatios=(0.6167 0.1539) # 1.0 1.0 1.0
startingCapacity=30
folderName="save_folder_location"
warmingSolutionFile="solFileOPL"
methods=("MPM" "SPM")
chargeStationDistance="12km"
solutionSaveFile="scheduleDetails"
chargeRate=600
bigM=25
busEnergyCost=1.0
logFile="logFile.txt"
maxSolutions=0
LPFile="solution.lp"
busDataFile="buses_schedule_input_file.json"
stationDistanceFile="station_distances_input.csv"
stationDataFile="stations_input.csv"
chargingStationsFile="_charging_stations.txt"
cew_folder="path_to_cew_files"
discountFactor=0.01

mkdir -p ./"${folderName}"
printf "Minimum Capacity:${minimumBatteryCapacity}\nMinimum charge time:${minChargeTime}\nBus speed:${busSpeed}
Timeout:${timeout}\nStatring capacity:${startingCapacity}\nwarmingSolutionFile:${warmingSolutionFile}
Charging station distances:${chargeStationDistance}\nSolution save file:${solutionSaveFile}\nCharge rate:${chargeRate}
Big M:${largeConstant}\nBus energy cost per km:${busEnergyCost}\nLpFile:${LPFile}\nBus data file:${busDataFile}
station Distance file:${stationDistanceFile}\nstation data file:${stationDataFile}\nNew cew constraints:${newCEWConstraints}
discount Factor:${discountFactor}\nNew progressive:${newProgressive}" > ${folderName}/settings.txt

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
							cd code/release-build
							cmake -DCMAKE_BUILD_TYPE=Release ..
							cmake --build . --config Release
							if(($t > 0))
							then
								./scheduler --discountFactor "${discountFactor}" --busEnergyCost "${busEnergyCost}" --chargeRate "${chargeRate}" --bigM "${bigM}" --maxSolutions $maxSolutions --LPFile $LPFile --timeout $timeout --solutionSaveFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/${solutionSaveFile}  --maxBatteryCapacity "${maxBatteryCapacity}" --minBatteryCapacity "${minBatteryCapacity}" --deviationTime "${deviationTime}" --timeWindows "${timeWindows}" --busSpeed "$busSpeed" --busDataFile "${path}"/${busDataFile} --stationDataFile "${path}"/${stationDataFile} --stationDistanceFile "${path}"/"${stationDistanceFile}" --logFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/"${logFile}" --horizonStartTime "${horizonStartTime}" --startingCapacity "${startingCapacity}" --chargingStationsFile ../../charging_station_locations/"${chargeStationDistance}"/"${location}""${chargingStationsFile}" --powerRatio "${powerRatio}" --maxChargeTime "${maxChargeTime}" --minChargeTime "${minChargeTime}" --location "${location}" --horizonEndTime "${horizonEndTime}" --method "${method}" --solutionDataFile ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${date}"_"${location}"_"${maxBatteryCapacity}"_"${deviationTime}"_"${busSpeed}"_"${horizonStartTimes[$t-1]}"_"${powerRatio}"/${solutionSaveFile} --recalculate "true" --warmingSolutionFile ${LPFile} > ../../${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/result.txt
							else
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
