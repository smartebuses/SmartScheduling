#!/bin/sh
locations=("cork" "limerick")
locationPaths=("../../../../../data_sets/data/timetable/Cork/instances/cork-11-lines" "../../../../../data_sets/data/timetable/Limerick/instances/limerick-7-lines")
maximumBatteryCapacities=(120)
minimumBatteryCapacity=12
maximumChargeTimes=(0.16)
minChargeTime=0.0166
deviationTimes=(0.0833)
busSpeed=35
horizonStartTimes=(0 6 12 18)
horizonEndTimes=(6 12 18 24)
dates=(14)
year=2022
month=2
timeout=720
datatypes=("noClean" "ideal" "predicted")
powerRatios=(0.6167 0.1539) # 1.0 1.0 1.0
startingCapacity=30
folderName="results/experiment_set_1"
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
busDataFile="buses_input_20_0.json"
stationDistanceFile="distances_input.csv"
stationDataFile="stations_input.csv"
chargingStationsFile="_charging_stations.txt"
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
			echo "$datatype"
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
							if [[ "$datatype" != "predicted"  && "$method" != "SPM" ]]
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
								dataFile="../CEWs/February/${datatype}/${method}/${year}-${month}-${date}-${horizonStartTime}.txt"
								while read timeWindows;
								do
									echo "CEW: " "$timeWindows"
								done < "${dataFile}"

							fi
							echo "Location:" "$location"							
							echo "Method:" "$method"
							echo "CEW:" "$timeWindows"
							echo "Charge station interval:" "$chargeStationDistance"
							echo "Battery capacity:" "$maxBatteryCapacity"
							echo "Max charge time:" "$maxChargeTime"
							echo "Min charge time:" "$minChargeTime"
							echo "Date:" "$date"
							echo "Horizon start time:" "$horizonStartTime"
							echo "Horizon end time:" "$horizonEndTime"
							echo "Deviation time:" "$deviationTime"
							echo "Data type:" "$datatype"
								
							resultDir="${date}"_"${location}"_"${maxBatteryCapacity}"_"${deviationTime}"_"${busSpeed}"_"${horizonStartTime}"_"${powerRatio}"
							mkdir -p ./"${folderName}"/"${location}"/"${datatype}"/"${method}"/"${resultDir}"
							printf "Location: ${location}\nWindow: ${timeWindows}\nBattery capacity: ${maxBatteryCapacity}\nDelta time: ${deviationTime}\nBus speed: ${busSpeed}\nInterval: ${chargeStationDistance}\nTime: ${horizonStartTime}\nDate ${date}\nPower ratio ${powerRatio}\nTimeout ${timeout}\nType $datatype" > ${folderName}/"${location}"/"${datatype}"/"${method}"/"${resultDir}"/details.txt
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
