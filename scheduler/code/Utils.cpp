//
// Created by padraigh on 02/03/2022.
//

#include "Utils.h"
#include <string.h>

using namespace std;


double Utils::convertTime(string time){
    const char *minutesPointer = strchr(time.c_str(), ':');
    double hour = stod(time);
    double minutes = stod(++minutesPointer) / 60;
    double convertedTime = hour + minutes;
    return convertedTime;
}