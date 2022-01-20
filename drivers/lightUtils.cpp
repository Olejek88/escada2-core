#include <mysql/mysql.h>
#include "lightUtils.h"
#include "dbase.h"
#include <map>
#include "suninfo.h"
#include <iostream>
#include <ctime>
#include <main.h>
#include "kernel.h"
#include "MtmZigbee.h"
#include "function.h"
#include "DeviceProgram.h"

extern std::string coordinatorUuid;
// массив в который складируем текущие уровни яркости
std::map<uint8_t, uint8_t> lightGroupBright = {
        {0,  0},
        {1,  0},
        {2,  0},
        {3,  0},
        {4,  0},
        {5,  0},
        {6,  0},
        {7,  0},
        {8,  0},
        {9,  0},
        {10, 0},
        {11, 0},
        {12, 0},
        {13, 0},
        {14, 0},
        {15, 0},
};

std::map<std::string, std::map<int, time_t>> *getDefaultAstroEvents(time_t currentTime, double lon, double lat) {
    // получаем три даты для выборки
    char *tmpStr = new char[11];

    std::tm tmpTm = {0};
    std::map<int, std::string> actualDates;
    localtime_r(&currentTime, &tmpTm);
    // вчера
    tmpTm.tm_mday--;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({0, tmpStr});
    // сегодня
    tmpTm.tm_mday++;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({1, tmpStr});
    // завтра
    tmpTm.tm_mday++;
    std::strftime(tmpStr, 11, "%F", &tmpTm);
    actualDates.insert({2, tmpStr});

    auto *defaultAstroEvents = new std::map<std::string, std::map<int, time_t>>;

    // зарание строим массив с расчитанными значениями, которые далее будем переопределять данными полученными из базы
    localtime_r(&currentTime, &tmpTm);
    // вчерашняя дата
    tmpTm.tm_mday--;
    time_t tmpTime = std::mktime(&tmpTm);

    auto it = actualDates.begin();
    while (it != actualDates.end()) {
        auto second = it->second;
        double r, s;
        double twb, twe;

        localtime_r(&tmpTime, &tmpTm);

        sun_rise_set(tmpTm.tm_year + 1900, tmpTm.tm_mon + 1, tmpTm.tm_mday, lon, lat, &r, &s);
        fillTimeStruct(r, &tmpTm);
        (*defaultAstroEvents)[second][0] = mktime(&tmpTm);
        fillTimeStruct(s, &tmpTm);
        (*defaultAstroEvents)[second][1] = mktime(&tmpTm);

        civil_twilight(tmpTm.tm_year + 1900, tmpTm.tm_mon + 1, tmpTm.tm_mday, lon, lat, &twb, &twe);
        fillTimeStruct(twb, &tmpTm);
        (*defaultAstroEvents)[second][2] = mktime(&tmpTm);
        fillTimeStruct(twe, &tmpTm);
        (*defaultAstroEvents)[second][3] = mktime(&tmpTm);

        tmpTm.tm_mday++;
        tmpTime = std::mktime(&tmpTm);
        it++;
    }

    it = actualDates.begin();
    auto next = actualDates.begin();
    next++;
    while (next != actualDates.end()) {
        auto prevDay = it->second;
        auto currDay = next->second;
        uint16_t nightLenght;
        uint16_t twilightLength;

        // длительность ночи восход сегодня - закат вчера
        nightLenght = (*defaultAstroEvents)[currDay][0] - (*defaultAstroEvents)[prevDay][1];
        // длительность ночи со вчера на сегодня
        (*defaultAstroEvents)[prevDay][4] = nightLenght;
        // длительность вчерашних сумерек конец сумерек - закат
        twilightLength = (*defaultAstroEvents)[prevDay][3] - (*defaultAstroEvents)[prevDay][1];
        // длительность вчерашних сумерек
        (*defaultAstroEvents)[prevDay][5] = twilightLength;

        it++;
        next++;
    }

    return defaultAstroEvents;
}

std::map<std::string, DeviceProgram> *getUsedPrograms(DBase *dBase, std::map<std::string, std::string> *programUuids) {
    auto *result = new std::map<std::string, DeviceProgram>;
    // выбираем программы из базы и складываем их в список в виде объектов
    std::string progList;
    auto prgUuidsIt = programUuids->begin();
    bool firstTime = true;
    while (prgUuidsIt != programUuids->end()) {
        if (firstTime) {
            firstTime = false;
            progList += "'" + prgUuidsIt->first + "'";
        } else {
            progList += ", '" + prgUuidsIt->first + "'";
        }

        prgUuidsIt++;
    }

    auto query = std::string("SELECT * FROM device_program WHERE uuid IN (" + progList + ")");
    MYSQL_ROW row;
    auto res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        char *tmpData;
        char *tmpDataEnd;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            DeviceProgram program;
            program.uuid = dBase->getFieldValue(row, "uuid");
            program.title = dBase->getFieldValue(row, "title");

            program.period_title1 = dBase->getFieldValue(row, "period_title1");
            program.value1 = std::stoi(dBase->getFieldValue(row, "value1"));

            program.period_title2 = dBase->getFieldValue(row, "period_title2");
            tmpData = dBase->getFieldValue(row, "time2");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time2 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value2 = std::stoi(dBase->getFieldValue(row, "value2"));

            program.period_title3 = dBase->getFieldValue(row, "period_title3");
            tmpData = dBase->getFieldValue(row, "time3");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time3 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value3 = std::stoi(dBase->getFieldValue(row, "value3"));

            program.period_title4 = dBase->getFieldValue(row, "period_title4");
            tmpData = dBase->getFieldValue(row, "time4");
            tmpDataEnd = &tmpData[strlen(tmpData)];
            program.time4 = std::strtoll(tmpData, &tmpDataEnd, 10);
            program.value4 = std::stoi(dBase->getFieldValue(row, "value4"));

            program.period_title5 = dBase->getFieldValue(row, "period_title5");
            program.value5 = std::stoi(dBase->getFieldValue(row, "value5"));

            (*result)[program.uuid] = program;
        }

        mysql_free_result(res);
    }

    return result;
}

void getNodeAstroEvents(DBase *dBase, std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    auto daeIt = defAstroEvents->begin();
    std::string query;
    query =
            "SELECT DATE(nct.date) AS date, TIME(nct.date) AS time, type, UNIX_TIMESTAMP(nct.date) AS datetime FROM node_control AS nct "
            "WHERE DATE(nct.date) IN ('" + (daeIt++)->first + "', '" + (daeIt++)->first + "', '" + (daeIt++)->first +
            "')";
    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        // обновляем данные в массиве
        while ((row = mysql_fetch_row(res)) != nullptr) {
            char *tmpData;
            std::string date;
            time_t time = 0;
            int type;

            tmpData = row[dBase->getFieldIndex("date")];
            if (tmpData == nullptr) {
                date.clear();
            } else {
                date = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("datetime")];
            if (tmpData == nullptr) {
                time = -1;
            } else {
                char *endPtr = &tmpData[strlen(tmpData)];
                time = std::strtoll(tmpData, &endPtr, 10);
            }

            tmpData = row[dBase->getFieldIndex("type")];
            if (tmpData == nullptr) {
                type = -1;
            } else {
                type = std::stoi(tmpData);
            }

            if (!date.empty() && (type == 0 || type == 1) && time > -1) {
                (*defAstroEvents)[date][type] = time;
            }
        }

        mysql_free_result(res);
    }
}

void recalcAstroEvents(std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    auto prevDayIt = defAstroEvents->begin();
    auto currDayIt = defAstroEvents->begin();
    currDayIt++;
    while (currDayIt != defAstroEvents->end()) {
        auto prevDay = prevDayIt->first;
        auto currDay = currDayIt->first;
        uint32_t nightLenght;
        uint32_t twilightLength;

        // длительность ночи = восход сегодня - закат вчера
        nightLenght = (*defAstroEvents)[currDay][0] - (*defAstroEvents)[prevDay][1];
        double K = (double) nightLenght / (*defAstroEvents)[prevDay][4];
        // длительность ночи со вчера на сегодня
        (*defAstroEvents)[prevDay][4] = nightLenght;
        // длительность вчерашних сумерек = конец сумерек - закат
        twilightLength = (uint32_t) ((*defAstroEvents)[prevDay][5] * K);
        // длительность вчерашних сумерек
        (*defAstroEvents)[prevDay][5] = twilightLength;

        // пересчитываем время наступления сумерек
        // начало сумерек
        (*defAstroEvents)[prevDay][2] = (*defAstroEvents)[prevDay][0] - twilightLength;
        // конец сумерек
        (*defAstroEvents)[prevDay][3] = (*defAstroEvents)[prevDay][1] + twilightLength;

        prevDayIt++;
        currDayIt++;
    }
}

void fillGroupsDefValues(groupsMap *groups, std::map<std::string, std::map<int, time_t>> *defAstroEvents) {
    // т.к. у нас 16 групп, создаём для них данные по умолчанию, реальные астрособытия
    // (это не совсем так, реальные астрособытия уже изменены данными из календаря шкафа, так и должно быть)
    for (int i = 0; i < 16; i++) {
        auto daeIt = defAstroEvents->begin();
        while (daeIt != defAstroEvents->end()) {
            auto first = daeIt->first;
            auto second = daeIt->second;
            int type;
            time_t time;

            // восход
            type = second.find(0)->first;
            time = second.find(0)->second;
            (*groups)[i][first][type]["time"] = std::to_string(time);
            // закат
            type = second.find(1)->first;
            time = second.find(1)->second;
            (*groups)[i][first][type]["time"] = std::to_string(time);
//            // начало сумерек
//            type = second.find(2)->first;
//            time = second.find(2)->second;
//            (*groups)[i][first][type]["time"] = std::to_string(time);
//            // конец сумерек
//            type = second.find(3)->first;
//            time = second.find(3)->second;
//            (*groups)[i][first][type]["time"] = std::to_string(time);
            // длительность ночи
            type = second.find(4)->first;
            time = second.find(4)->second;
            (*groups)[i][first][type]["nLength"] = std::to_string(time);
            // длительность сумерек
            type = second.find(5)->first;
            time = second.find(5)->second;
            (*groups)[i][first][type]["twLength"] = std::to_string(time);

            daeIt++;
        }
    }
}

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    Kernel *kernel = &Kernel::Instance();

    // асоциативный массив в котором будем хранить данные по датам, времени, действиям, программам групп
    groupsMap groups;

    // плучаем реальные данные по астро событиям за три дня ()
    auto defAstroEvents = getDefaultAstroEvents(currentTime, lon, lat);

    // получаем данные из календаря шкафа
    getNodeAstroEvents(dBase, defAstroEvents);

    // пересчитываем длительность сумерек, сохраняем новую длительность ночи
    // так как в календаре шкафа могут быть указаны другие значения заката/восхода
    recalcAstroEvents(defAstroEvents);

    // заполняем группы астрособытиями "по умолчанию", расчитаными и полученными из календаря шкафа
    fillGroupsDefValues(&groups, defAstroEvents);

    // вспомогательный массив для сохранения программ по умолчанию для групп
    std::map<int, std::string> grpProgram;
    // вспомогательный массив для списка используемых программ
    std::map<std::string, std::string> programUuids;

    // выбираем данные по группам
    auto daeIt = defAstroEvents->begin();
    std::string query;
    query =
            "SELECT gt.title, gt.groupId, gt.deviceProgramUuid AS gProgram, date(gct.date) as date, time(gct.date) as time, gct.type as type, gct.deviceProgramUuid AS gcProgram, UNIX_TIMESTAMP(gct.date) AS datetime "
            "FROM `group` AS gt "
            "LEFT JOIN group_control AS gct ON "
            "(gct.groupUuid=gt.uuid AND DATE(gct.date) IN('" + (daeIt++)->first + "', '" + (daeIt++)->first + "', '" +
            (daeIt++)->first + "'))";
//    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] QUERY GROUP: %s", TAG, query.data());
    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        // обновляем данные в массиве
        while ((row = mysql_fetch_row(res)) != nullptr) {
            char *tmpData;
            int groupId;
            std::string gProgram;
            std::string date;
            std::string time;
            int type;
            std::string gcProgram;

            groupId = std::stoi(row[dBase->getFieldIndex("groupId")]);

            tmpData = row[dBase->getFieldIndex("gProgram")];
            if (tmpData == nullptr) {
                gProgram.clear();
            } else {
                gProgram = tmpData;
            }

            // запоминаем программу по умолчанию
            grpProgram[groupId] = gProgram;
            if (!gProgram.empty()) {
                programUuids[gProgram] = gProgram;
            }

            tmpData = row[dBase->getFieldIndex("date")];
            if (tmpData == nullptr) {
                date.clear();
            } else {
                date = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("datetime")];
            if (tmpData == nullptr) {
                time.clear();
            } else {
                time = tmpData;
            }

            tmpData = row[dBase->getFieldIndex("type")];
            if (tmpData == nullptr) {
                type = -1;
            } else {
                type = std::stoi(tmpData);
            }

            tmpData = row[dBase->getFieldIndex("gcProgram")];
            if (tmpData == nullptr) {
                gcProgram.clear();
            } else {
                gcProgram = tmpData;
            }

            // вносим данные в массив
            if (type == 1) {
                if (!gcProgram.empty()) {
                    groups[groupId][date][type]["prog"] = gcProgram;
                    programUuids[gcProgram] = gcProgram;
                } else {
                    groups[groupId][date][type]["prog"] = gProgram;
                    if (!gProgram.empty()) {
                        programUuids[gProgram] = gProgram;
                    }
                }

                if (!time.empty()) {
                    groups[groupId][date][type]["time"] = time;
                }
            }
        }

        mysql_free_result(res);
    }

    // на текущий момент, в массиве не хватает данных по программам которые назначены группам
    // бежим по списку grpProgram, проставляем в массиве groups программы, для тех дат, для которых они не указанны
    for (int i = 0; i < 16; i++) {
        daeIt = defAstroEvents->begin();
        std::string defPrg;
        while (daeIt != defAstroEvents->end()) {
            for (int j = 0; j < 2; j++) {
                defPrg = groups[i][daeIt->first][j]["prog"];
                if (defPrg.empty()) {
                    defPrg = grpProgram[i];
                    groups[i][daeIt->first][j]["prog"] = defPrg;
                }
            }

            daeIt++;
        }
    }

    for (int i = 0; i < 16; i++) {
        auto prevDayIt = defAstroEvents->begin();
        auto currDayIt = defAstroEvents->begin();
        currDayIt++;

        while (currDayIt != defAstroEvents->end()) {
            auto prevDay = prevDayIt->first;
            auto currDay = currDayIt->first;

            uint32_t nightLenght;
            uint32_t twilightLength;

            // длительность ночи = восход сегодня - закат вчера
            nightLenght = std::stol(groups[i][currDay][0]["time"]) - std::stol(groups[i][prevDay][1]["time"]);
            double K = (double) nightLenght / std::stol(groups[i][prevDay][4]["nLength"]);
            // длительность ночи со вчера на сегодня
            groups[i][prevDay][4]["nLength"] = std::to_string(nightLenght);
            // длительность вчерашних сумерек = конец сумерек - закат
            twilightLength = (uint32_t) (std::stol(groups[i][prevDay][5]["twLength"]) * K);
            // длительность вчерашних сумерек
            groups[i][prevDay][5]["twLength"] = std::to_string(twilightLength);

            prevDayIt++;
            currDayIt++;
        }
    }

    // получаем список программ в виде объектов
    auto devPrograms = getUsedPrograms(dBase, &programUuids);

    // к этому моменту у меня есть список программ, список групп со всеми астрособытиями для них, астрособытия для шкафа (с учётом календарей)

    // массив в который складируем время-уровень освещённости (должно быть 16 значений)
    std::map<time_t, uint8_t> testData;

    // расчитываем временные метки за вчерашний день начиная с заката и сегодняшнего рассвета плюс с сегодняшнего заката до завтрашнего рассвета
    // для меток получаем значения яркости светильников в зависимости от программы управления
    // строим массив из 16 пар значений группа-яркость
    for (int i = 0; i < 16; i++) {
        testData.clear();

        auto prevDayIt = defAstroEvents->begin();
        auto currDayIt = defAstroEvents->begin();
        currDayIt++;

        DeviceProgram program;
        uint8_t brightValue = 99;
        std::string prog = groups[i][prevDayIt->first][1]["prog"];
        if (!prog.empty()) {
            program = devPrograms->find(prog)->second;

            // протоколируем название программы для группы
            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] light group #%d, program name '%s'", TAG, i,
                                  program.title.c_str());
            }


            while (currDayIt != defAstroEvents->end()) {

                long twLength = std::stol(groups[i][prevDayIt->first][5]["twLength"]);
                long nLength = std::stol(groups[i][prevDayIt->first][4]["nLength"]);
                nLength = nLength - twLength * 2;

                time_t sunrise = std::stol(groups[i][currDayIt->first][0]["time"]);
                time_t sunset = std::stol(groups[i][prevDayIt->first][1]["time"]);
                time_t twEnd = sunset + twLength;
                auto time2 = (time_t) ((double) twEnd + (nLength * (1.0 / (100.0 / program.time2))));
                auto time3 = (time_t) ((double) time2 + (nLength * (1.0 / (100.0 / program.time3))));
                time_t twBegin = sunrise - twLength;

                testData[sunset] = program.value1;
                testData[twEnd] = program.value2;
                testData[time2] = program.value3;
                testData[time3] = program.value4;
                testData[twBegin] = program.value5;
                testData[sunrise] = 0;

                prevDayIt++;
                currDayIt++;
            }

            auto testDataItCurr = testData.begin();
            auto testDataItNext = testData.begin();
            testDataItNext++;
            while (testDataItNext != testData.end()) {
                if (currentTime >= testDataItCurr->first && currentTime < testDataItNext->first) {
                    brightValue = testDataItCurr->second;
                    break;
                }

                testDataItCurr++;
                testDataItNext++;
            }
        }

        // протоколируем уровень яркости
        if (kernel->isDebug) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] light group #%d, bright: %d", TAG, i, brightValue);
        }

        lightGroupBright[i] = brightValue;
    }
}

void makeLostLightList(DBase *dBase, Kernel *kernel) {
    // выбираем все управляемые светильники со статусом отличным от WORK и NOT_MOUNTED
    // полученные данные записываем в lost_light
    std::string currentDate("FROM_UNIXTIME(" + std::to_string(time(nullptr)) + ")");
    auto query = std::string(
            "INSERT INTO lost_light (uuid, date, title, status, macAddress, deviceUuid, nodeUuid, createdAt, changedAt) ")
            .append("SELECT UPPER(UUID()) uuid, " + currentDate +
                    ", dt.name, dst.title, dt.address, dt.uuid, dt.nodeUuid, " + currentDate + ", " + currentDate + " ")
            .append("FROM device dt ")
            .append("LEFT JOIN device_status dst ON dt.deviceStatusUuid=dst.uuid ")
            .append("WHERE deviceTypeUuid IN ('" + std::string(DEVICE_TYPE_ZB_LIGHT) + "') ")
            .append("AND deviceStatusUuid NOT IN ('" + std::string(DEVICE_STATUS_WORK) + "', ")
            .append("'" + std::string(DEVICE_STATUS_NOT_MOUNTED) + "')");

    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] lost_light: %s", TAG, query.data());
    }

    auto res = dBase->sqlexec(query.data());
    mysql_free_result(res);
}