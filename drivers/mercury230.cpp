//-----------------------------------------------------------------------------
#include <fcntl.h>
#include <sys/termios.h>
#include <pthread.h>
#include <cstdio>
#include <mysql/mysql.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <zconf.h>
#include "errors.h"
#include "main.h"
#include "mercury230.h"
#include "dbase.h"
#include "TypeThread.h"
#include "kernel.h"
#include "function.h"

//-----------------------------------------------------------------------------
static MYSQL_RES *res;
static MYSQL_ROW row;
static char query[500];
static int fd = 0;
bool rs = true;

DeviceMER deviceMER;
static DBase dBase;
auto &currentKernelInstance = Kernel::Instance();

bool OpenCom(char *block, uint16_t speed, uint16_t parity);

uint16_t CRC(uint8_t *Data, uint8_t DataSize, uint8_t type);

uint16_t Crc16(uint8_t *Data, uint8_t DataSize);

static uint8_t Ct(uint8_t Data);

uint8_t CRC_CE(const uint8_t *Data, uint8_t DataSize, uint8_t type);

//-----------------------------------------------------------------------------
void *mekDeviceThread(void *pth) {
    TypeThread thread = *((TypeThread *) pth);
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] mercury device thread started");
    if (dBase.openConnection() == OK) {
        while (true) {
            sprintf(query, "SELECT * FROM device WHERE thread=%d", thread.id);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] (%s)", query);
            res = dBase.sqlexec(query);
            u_long nRow = mysql_num_rows(res);
            for (u_long r = 0; r < nRow; r++) {
                row = mysql_fetch_row(res);
                if (row) {
                    strncpy(deviceMER.uuid, row[2], 20);
                    strncpy(deviceMER.address, row[3], 10);
                    strncpy(deviceMER.port, row[10], 20);
                }
                if (!fd) {
                    rs = OpenCom(deviceMER.port, thread.speed, 0);
                    if (rs) {
                        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadInfo (%s)", deviceMER.address);
                        UpdateThreads(dBase, thread.id, 0, 1, deviceMER.uuid);
                        rs = deviceMER.ReadInfoCE();
                        if (rs) {
                            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataCurrent (%d)", deviceMER.id);
                            UpdateThreads(dBase, thread.id, 0, 1, deviceMER.uuid);
                            deviceMER.ReadDataCurrentCE();
                            if (currentKernelInstance.current_time->tm_min > 45) {
                                UpdateThreads(dBase, thread.id, 1, 1, deviceMER.uuid);
                                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataArchive (%d)",
                                                                deviceMER.id);
                                deviceMER.ReadAllArchiveCE(5);
                            }
                            if (thread.work == 0) {
                                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                                "[303] mercury 230 & set 4TM & CE303 threads stopped");
                                pthread_exit(nullptr);
                            }
                        }
                    } else {
                        sleep(10);
                    }
                }
            }
            if (fd) close(fd);
            fd = 0;
        }
    }
    dBase.disconnect();
    pthread_exit(nullptr);
}


// ReadDataCurrent - read single device. Readed data will be stored in DB
int DeviceMER::ReadDataCurrent() {
    bool rs;
    uint16_t rec = 0;
    char *chan;
    // TODO решить что делать с коэффициентом трансформации
    float fl, knt = 6200, A = 0;
    unsigned char data[400];
    char date[20];
    this->q_attempt++;
    if (this->deviceType == TYPE_MERCURY230)
        rs = send_mercury(READ_PARAMETRS, 0x11, ENERGY_SUM, 0);
    else
        rs = send_mercury(READ_PARAMETRS, 0x11, 0x8, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec > 0) {
        if (this->deviceType == TYPE_SET_4TM) {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
            fl = fl / 500000;
        } else {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
            fl = fl / 100000;
        }
        fl *= knt;

        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (chan != nullptr) {
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
            sprintf(date, "%04d%02d%02d%02d%02d00", currentKernelInstance.current_time->tm_year + 1900,
                    currentKernelInstance.current_time->tm_mon + 1,
                    currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
                    (5 * (currentKernelInstance.current_time->tm_min / 5)));
            dBase.StoreData(TYPE_INCREMENTS, 0, fl, date, chan);
            free(chan);
        }
    }

    if (this->deviceType == TYPE_MERCURY230)
        rs = send_mercury(READ_PARAMETRS, 0x11, 0xF0, 0);
    else rs = send_mercury(READ_PARAMETRS, 0xD, 0xFF, 0);

    if (rs) rec = this->read_mercury(data, 0);
    if (rec > 0) {
        if (this->deviceType == TYPE_MERCURY230) {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
            fl = fl / 100;
            if (knt) fl *= knt;
        } else {
            fl = ((data[0] * 256 * 256 * 256) + (data[1] * 256 * 256) + (data[2] * 256) + (data[3]));
            if (knt) fl *= knt;
            if (A > 0) fl = fl / (2 * A);
        }
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x 0x%x] W=[%f]", data[0], data[1], data[2],
                                        data[3], fl);
        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (fl < 50000000 && chan != nullptr) {
            sprintf(date, "%04d%02d%02d%02d%02d00", currentKernelInstance.current_time->tm_year + 1900,
                    currentKernelInstance.current_time->tm_mon + 1,
                    currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
                    (5 * (currentKernelInstance.current_time->tm_min / 5)));
            dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
            free(chan);
        }
    }

    rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);

    if (rs) rec = this->read_mercury(data, 0);
    if (rec > 0) {
        if (this->deviceType == TYPE_MERCURY230) {
            fl = (float) ((data[0]) * 256 * 256) + (float) (data[1] * 256 * 256 * 256) + (float) data[2] +
                 (float) (data[3] * 256); //fl=fl/100;
            //fl*=10; // !!!!!! 05.12.2014 fatal error
            if (knt > 0) {
                fl *= knt;
                if (A > 0) fl = fl / (2 * A);
                //if (this->knt) fl *= this->knt;
            }
        } else {
            fl = ((data[0] * 256 * 256 * 256) + (data[1] * 256 * 256) + (data[2] * 256) + (data[3]));
            //fl=((data[0]*256*256)+(data[1]*256)+(data[2]));
            //if (debug>2) ULOGW ("[mer][0x%x 0x%x 0x%x 0x%x] Wn[%d]=[%f]",data[0],data[1],data[2],data[3],this->device,fl);
            if (knt > 0) {
                fl *= knt;
                if (A > 0) fl = fl / (2 * A);
            }
            //if (A>0) fl=fl*this->Kt*this->Kn/(2*A);
        }
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x 0x%x] Wn[%d]=[%f] [%f][%f]",
                                        data[0], data[1], data[2], data[3], this->id, fl, A, knt);
        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (fl < 500000000 && chan != nullptr) {
            sprintf(date, "%04d%02d%02d%02d%02d00", currentKernelInstance.current_time->tm_year + 1900,
                    currentKernelInstance.current_time->tm_mon + 1,
                    currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
                    (5 * (currentKernelInstance.current_time->tm_min / 5)));
            dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
            free(chan);
        }
    }

    rs = send_mercury(READ_PARAMETRS, 0x11, 0x20, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec) {
        if (this->deviceType == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 10000;
        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (chan != nullptr) {
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x] I3=[%f]", data[0], data[1], data[2],
                                            fl);
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
            free(chan);
        }
    }

    rs = send_mercury(READ_PARAMETRS, 0x11, U1, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec) {
        if (this->deviceType == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 100;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x] U3=[%f]", data[0], data[1], data[2], fl);
        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (fl < 300 && chan != nullptr) {
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
            sprintf(date, "%04d%02d%02d%02d%02d00", currentKernelInstance.current_time->tm_year + 1900,
                    currentKernelInstance.current_time->tm_mon + 1,
                    currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
                    (5 * (currentKernelInstance.current_time->tm_min / 5)));
            dBase.StoreData(TYPE_INCREMENTS, 0, fl, nullptr, chan);
            free(chan);
        }
    }

    rs = send_mercury(READ_PARAMETRS, 0x11, F, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec) {
        if (this->deviceType == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 100;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x] F=[%f]", data[0], data[1], data[2], fl);
        chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
        if (fl < 300 && chan != nullptr) {
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
            sprintf(date, "%04d%02d%02d%02d%02d00", currentKernelInstance.current_time->tm_year + 1900,
                    currentKernelInstance.current_time->tm_mon + 1,
                    currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
                    (5 * (currentKernelInstance.current_time->tm_min / 5)));
            dBase.StoreData(TYPE_INCREMENTS, 0, fl, nullptr, chan);
            free(chan);
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::ReadInfo() {
    unsigned serial = 0, soft, cnt;
    bool rs;
    unsigned rec = 0;
    uint8_t data[400];
    char date[20] = {0};

    float kn, kt, A = 1;

    rs = send_mercury(OPEN_CHANNEL, 0, 0, 0);
    usleep(400000);
    rec = this->read_mercury(data, 0);

    if (!rec || data[0] != 0) {
        rs = send_mercury(OPEN_CHANNEL, 0, 0, 1);
        usleep(200000);
        rec = this->read_mercury(data, 0);
        if (!rs || data[0] != 0) {
            rs = send_mercury(OPEN_CHANNEL, 1, 0, 0);
            usleep(200000);
            rec = this->read_mercury(data, 0);
        }
    }
    if (rec && data[0] == 0) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] open channel success %x %x [%d]", data[0], data[1], rs);
        rs = send_mercury(READ_PARAMETRS, 0x0, 0xff, 0);
        if (rs) rec = this->read_mercury(data, 0);
        if (rec) {
            serial = static_cast<unsigned int>(data[0] + data[1] * 256 + data[2] * 256 * 256 +
                                               data[3] * 256 * 256 * 256);
            if (this->deviceType == TYPE_SET_4TM)
                serial = static_cast<unsigned int>(data[3] + data[2] * 256 + data[1] * 256 * 256 +
                                                   data[0] * 256 * 256 * 256);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [serial=%d (%d/%d/%d)]", serial, data[4], data[5],
                                            data[6]);
        }
        rec = this->read_mercury(data, 0);

        rs = send_mercury(READ_PARAMETRS, 0x2, 0xff, 0);
        if (rs) rec = this->read_mercury(data, 0);
        if (rec) {
            soft = static_cast<unsigned int>(data[0] + data[1] * 256 + data[2] * 256 * 256);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [soft=%d]", soft);
        }
        rec = this->read_mercury(data, 0);

        for (cnt = 1; cnt < 3; cnt++) {
            rs = send_mercury(READ_TIME_230, 0x0, 0xff, 0);
            if (rs) rec = this->read_mercury(data, 0);
            if (rec && serial > 0) {
                sprintf(date, "%02d-%02d-%d %02d:%02d:%02d", BCD(data[4]), BCD(data[5]), BCD(data[6]), BCD(data[2]),
                        BCD(data[1]), BCD(data[0]));
                sprintf(this->dev_time, "%04d%02d%02d%02d%02d%02d", 2000 + BCD(data[6]), BCD(data[5]), BCD(data[4]),
                        BCD(data[2]), BCD(data[1]), BCD(data[0]));
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [date=%s]", date);
                sprintf(query, "UPDATE device SET lastdate=NULL,conn=1,devtim=%s WHERE id=%d", this->dev_time,
                        this->id);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [%s]", query);
                this->q_error = 0;
                if (BCD(data[6]) < 20 && BCD(data[6]) > 0) {
                    res = dBase.sqlexec(query);
                    if (res) mysql_free_result(res);
                    break;
                }
            }
        }

        rs = send_mercury(READ_PARAMETRS, 0x02, 0xff, 0);
        if (rs) rec = this->read_mercury(data, 0);
        if (rec) {
            kn = data[1];
            kt = data[3];
            rs = send_mercury(READ_PARAMETRS, 0x12, 0xff, 0);
            if (rs) rec = this->read_mercury(data, 0);
            if (rec) {
                switch (data[1] & 0xf) {
                    case 0:
                        A = 5000;
                        break;
                    case 1:
                        A = 25000;
                        break;
                    case 2:
                        A = 1250;
                        break;
                    case 3:
                        A = 6250;
                        break; // !!! no correct
                    case 4:
                        A = 500;
                        break;  // !!! no correct
                    case 5:
                        A = 250;
                        break;
                    case 6:
                        A = 6400;
                        break;
                    default:
                        break;
                }
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [Kn=%f|Kt=%f] A=%f (%d)", kn, kt, A,
                                                data[1] & 0xf);
                if (A > 100) {
                    sprintf(query, "UPDATE dev_mer SET A=%f WHERE device=%d", A, this->id);
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [%s]", query);
                    res = dBase.sqlexec(query);
                    if (res) mysql_free_result(res);
                }
            }
        }
        return true;
    } else {
        sprintf(query, "UPDATE device SET last_date=NULL WHERE id=%d", this->id);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] [%s]", query);
        this->q_error++;
        if (this->q_error > 10) {
            res = dBase.sqlexec(query);
            if (res) mysql_free_result(res);
            this->q_error = 0;
        }
        return false;
    }
}

//-----------------------------------------------------------------------------
// ReadDataArchive - read single device. Readed data will be stored in DB
int DeviceMER::ReadAllArchive(uint16_t tp) {
    bool rs;
    unsigned rec = 0;
    uint8_t data[400], data2[100];
    char date[20];
    unsigned month, year, index;
    char *chan;
    float fl, fl2, fl3, fl4, knt = 1, A = 1;
    unsigned code, vsk = 0, adr = 0, j = 0, f = 0;
    time_t tims, tim;
    tims = time(&tims);
    struct tm tt{};

    this->q_attempt++;  // attempt

    if (tp == 99) {
        //if (this->device>10006)
        if (this->deviceType == TYPE_MERCURY230) {
            rs = send_mercury(READ_PARAMETRS, 0x13, 0xff, 0);
            if (rs) rec = this->read_mercury(data, 0);
            if (rec) {
                //03-30 14:07:10 [mer] READ [24][0x1,0x15,0x1,0x2,0x19,0x2c,0x0,0x0,0x0,0x40,0x1e,0xc,0x17,0x1,0x14,0x0,0x30,0x3,0x15][crc][0x27,0x4f]
                //03-30 14:07:10 [set][hh][17:12 01-14-2000] [c 17]
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][hh][%02d:%02d %02d-%02d-%d] [%x %x]",
                                                BCD(data[3]), BCD(static_cast<uint8_t>(data[4] & 0x7f)), BCD(data[5]),
                                                BCD(data[6]), 2000 + BCD(data[7]), data[0], data[1]);
                if (BCD(data[3]) >= 0 && BCD(data[3]) < 24)
                    if (BCD(data[5]) > 0 && BCD(data[5]) < 32)
                        if (BCD(data[6]) > 0 && BCD(data[6]) < 13) {
                            //if (debug>2) ULOGW ("[set][hh][+++]");
                            adr = static_cast<unsigned int>(data[0] * 0x100 + data[1]);
                            //if (BCD(static_cast<uint8_t>(data[4] & 0x7f)) == 30 && adr >= 0x3) if (this->device <= 10006) adr -= 0x3;
                            //if (BCD(static_cast<uint8_t>(data[4] & 0x7f)) == 0 && adr >= 0x2) if (this->device <= 10006) adr -= 0x2;
                            //if (this->device <= 10006)
                            adr *= 0x10;
                            vsk = 250;
                            //if (this->device==10014 || this->device==10015) vsk=800;
                            while (vsk) {
                                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][hh][%d][%d]", vsk, adr);
                                rs = send_mercury(READ_POWER, 0x3, static_cast<u_int8_t>(adr), 2 * 0xf);
                                if (rs) rec = this->read_mercury(data, 0);
                                if (rec) {
                                    if (BCD(data[1]) >= 0 && BCD(data[1]) < 24 && BCD(data[3]) > 0 &&
                                        BCD(data[3]) < 32 && BCD(data[4]) > 0 && BCD(data[4]) < 13) {
                                        fl = (float) (data[8] * 256) + (float) (data[7]);
                                        fl2 = (float) (data[23] * 256) + (float) (data[22]);
                                        fl3 = (float) (data[10] * 256) + (float) (data[9]);
                                        fl4 = (float) (data[25] * 256) + (float) (data[24]);
                                        fl = (fl + fl2) / 2;
                                        fl3 = (fl3 + fl4) / 2;
                                        if (knt > 0) {
                                            fl *= knt;
                                            if (A > 0) fl = fl / (A);
                                            fl3 *= knt;
                                            if (A > 0) fl3 = fl3 / (A);
                                        }
                                        sprintf(date, "%04d%02d%02d%02d0000", 2000 + BCD(data[5]),
                                                BCD(data[4]), BCD(data[3]), BCD(data[1]));
                                        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                                        "[mer] T [%d][%d:00 %02d-%02d-%d] (%f)", j,
                                                                        BCD(data[1]),
                                                                        BCD(data[3]), BCD(data[4]), BCD(data[5]), fl);

                                        dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
                                        dBase.StoreData(TYPE_CURRENTS, 0, fl, date, chan);
                                    }

                                }
                                vsk--;
                                if (adr >= 0x20) adr -= 0x20;
                                else adr = 0xfff * 0x10;
                            }
                        }
            }
        } else {
            rs = send_mercury(READ_PARAMETRS, 0x4, 0xff, 0);
            if (rs) rec = this->read_mercury(data, 0);
            if (rec) {
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][hh][%02d:%02d %02d-%02d-%d] [%x %x]",
                                                BCD(data[1]), BCD(static_cast<uint8_t>(data[0] & 0x7f)), BCD(data[2]),
                                                BCD(data[3]), 2000 + BCD(data[4]), data[5], data[6]);
                if (BCD(data[1]) >= 0 && BCD(data[1]) < 24)
                    if (BCD(data[2]) > 0 && BCD(data[2]) < 32)
                        if (BCD(data[3]) > 0 && BCD(data[3]) < 13)
                            if (BCD(data[4]) == 15) /// !!!!!
                            {
                                adr = static_cast<unsigned int>(data[5] * 0x100 + data[6]);
                                // rs=send_mercury (READ_PARAMETRS, 0x6, 0xff, 0);
                                // if (rs)  rs = this->read_mercury(data, 0);
                                //if (debug>2) ULOGW ("[set][hh][int_time=%0d]",data[1]);
                                adr -= 0x8;
                                vsk = 8;
                                while (vsk) {
                                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][hh][%d][%d]", vsk, adr);
                                    rs = send_mercury(READ_POWER, 0x3, adr, 0x20);
                                    if (rs) rec = this->read_mercury(data, 0);
                                    if (rs) {
                                        for (j = 0; j < rs; j++)
                                            if (BCD(data[j + 0]) >= 0 && BCD(data[j + 0]) < 24)
                                                if (BCD(data[j + 1]) > 0 && BCD(data[j + 1]) < 32)
                                                    if (BCD(data[j + 2]) > 0 && BCD(data[j + 2]) < 13)
                                                        if (BCD(data[j + 3]) == 15) /// !!!!!
                                                        {
                                                            if (rs - j > 10 && rs - j < 80) {
                                                                for (f = 0; f < rs - j; f++)
                                                                    data2[f] = data[j + f];
                                                            } else {
                                                                vsk--;
                                                                continue;
                                                            }
                                                            //for (f=0; f<rs-j;f++) ULOGW ("[mer][%d(%d)][%d][%x]",j,rs,f,data2[f]);
                                                            if (data2[8] < 0x80)
                                                                fl = (float) (data2[8] * 256) + (float) (data2[9]);
                                                            else fl = 0;
                                                            //if (data2[16]<0x80) fl2=(float)(data2[16]&0x7f*256)+(float)(data2[17]);
                                                            //else fl2=0;
                                                            //fl=fl;
                                                            if (knt > 0) {
                                                                //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] fl=[%f]",fl);
                                                                fl *= knt;
                                                                //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] fl=[%f]",fl);
                                                                if (A > 0) fl = fl / (A);
                                                                //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] fl=[%f]",fl);
                                                                //if (this->knt) fl*=this->knt;
                                                            }
                                                            sprintf(date, "%04d%02d%02d%02d0000",
                                                                    2000 + BCD(data2[3]), BCD(data2[2]), BCD(data2[1]),
                                                                    BCD(data2[0]));
                                                            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                                                            "[mer] T [%d][%d:00 %02d-%02d-%d] (%f) [%x %x]",
                                                                                            j, BCD(data2[0]),
                                                                                            BCD(data2[1]),
                                                                                            BCD(data2[2]),
                                                                                            BCD(data2[3]), fl, data2[8],
                                                                                            data2[9]);
                                                            //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] P+[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] (%d)",data2[8]&0x7f,data2[9],data2[10],data2[11],data2[12],data2[13],data2[14],data2[15],data2[8]&0x7f*256+data2[9]);
                                                            //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] P+[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] (%d)",data2[16]&0x7f,data2[17],data2[18],data2[19],data2[20],data2[21],data2[22],data2[23],data2[16]&0x7f*256+data2[17]);
                                                            strncpy(chan,
                                                                    dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY),
                                                                                     1, this->uuid), 20);
                                                            dBase.StoreData(TYPE_CURRENTS, 0, fl, date, chan);
                                                            break;
                                                        }
                                    }
                                    if (adr >= 0x18) adr -= 0x18;
                                    else adr = 65500;
                                    vsk--;
                                }
                            }
            }
        }
    }

    rs = send_mercury(READ_DATA_230, 0x50, 0x0, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec) {
        tim = time(&tim);
        tim -= 3600 * 24;
        localtime_r(&tim, &tt);
        if (this->deviceType == TYPE_SET_4TM)
            fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                  (float) (data[3]));
        else
            fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                  (float) (data[2]));
        // fl=fl/1000;
        if (knt > 0) {
            fl *= knt;
            if (A > 0) fl = fl / (2 * A);
            if (knt) fl *= knt;
        }
        sprintf(date, "%04d%02d%02d000000", tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][2][0x%x 0x%x 0x%x 0x%x] [%f][%s][%f]",
                                        data[0], data[1], data[2], data[3], fl, date, knt);
        dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
    }

    //rs=send_mercury (READ_DATA_230, 0x84, 0x0, 0);
    rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec) {
        tim = time(&tim);
        tim -= 3600 * 24;
        localtime_r(&tim, &tt);
        if (this->deviceType == TYPE_SET_4TM)
            fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                  (float) (data[3]));
//            fl=((float)(data[0]*256*256)+(float)(data[1]*256)+(float)(data[2]));
        else
            fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                  (float) (data[2]));
        if (knt > 0) {
            fl *= knt;
            if (A > 0) fl = fl / (2 * A);
            if (knt) fl *= knt;
        }
        //fl=((float)(data[0]&0x3f)+(float)data[1]*256+(float)data[2]*256*256+(float)data[3]*256*256*256); fl=fl;
        sprintf(date, "%04d%02d%02d000000", tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][0x%x 0x%x 0x%x 0x%x] [%f][%s]", data[0], data[1],
                                        data[2], data[3], fl, date);
        dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
    }

    tim = time(&tim);
    localtime_r(&tim, &tt);
    for (int i = 0; i < 10; i++) {
        if (tt.tm_mon == 0) {
            tt.tm_mon = 12;
            tt.tm_year--;
        }
        rs = send_mercury(READ_DATA_230, static_cast<u_int8_t>(0x30 + tt.tm_mon), 0x0, 0);
        if (rs) rec = this->read_mercury(data, 0);
        if (rec) {
            if (this->deviceType == TYPE_SET_4TM)
                fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                      (float) (data[3]));
            else
                fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                      (float) (data[2]));
            //fl=fl/1000;
            if (knt > 0) {
                fl *= knt;
                if (A > 0) fl = fl / (2 * A);
                if (knt) fl *= knt;
            }
            sprintf(date, "%04d%02d01000000", tt.tm_year + 1900, tt.tm_mon + 1);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][4][%d][0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                                chan, data[0], data[1], data[2], data[3], fl, date);
                free(chan);
            }
        }

        rs = send_mercury(READ_DATA_230, static_cast<u_int8_t>(0x00 + tt.tm_mon), 0x0, 0);
        if (rs) rec = this->read_mercury(data, 0);
        if (rec) {
            if (this->deviceType == TYPE_SET_4TM)
                fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) +
                      (float) (data[2] * 256) + (float) (data[3]));
            else
                fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) +
                      (float) (data[3] * 256) + (float) (data[2]));
            //fl=fl/1000;
            if (knt > 0) {
                fl *= knt;
                if (A > 0) fl = fl / (2 * A);
                if (knt) fl *= knt;
            }
            sprintf(date, "%04d%02d01000000", tt.tm_year + 1900, tt.tm_mon + 1);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer][4][%d][0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                                chan, data[0], data[1], data[2], data[3], fl, date);
                free(chan);
            }
        }
        tt.tm_mon--;
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::send_mercury(u_int8_t op, u_int8_t prm, u_int8_t frame, u_int8_t index) {
    if (this->deviceType == TYPE_INPUT_CE)
        return this->send_mercury(op, prm, frame, index);

    uint32_t crc = 0;          //(* CRC checksum *)
    uint32_t nbytes = 0;     //(* number of bytes in send packet *)
    char data[100];      //(* send sequence *)
    uint32_t ht = 0, len = 0, nr = 0;
    char path[100] = {0};

    if (op == 0x1) // open/close session
    {
        if (this->deviceType == TYPE_MERCURY230) len = 11;
        if (this->deviceType == TYPE_SET_4TM) len = 10;
    }
    if (op == 0x4) // time
        len = 5;
    if (op == 0x5 || op == 0x8 || op == 0x11 || op == 0x14) // nak
    {
        if (index < 0xff) len = 6;
        else len = 5;
        if (frame == 5) len = 7;
    }
    if (op == 0x6)
        len = 8;

    if (this->protocol == 13) {
        for (ht = 0; ht < 8; ht++)
            if (!this->address[ht]) break;
            else nr++;

        data[0] = 1;
        data[1] = static_cast<uint8_t>(6 + 3 * nr + len);
        data[2] = static_cast<uint8_t>(16 + (nr - 1));
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;    // comp address
        for (ht = 0; ht < nr; ht++) {
            data[6 + ht * 3] = static_cast<uint8_t>(this->address[ht] / 0x10000);
            data[7 + ht * 3] = static_cast<uint8_t>((this->address[ht] & 0xff00) / 0x100);
            data[8 + ht * 3] = static_cast<uint8_t>(this->address[ht] & 0xff);    // PPL-N address
            sprintf(path, "%s[%d.%d.%d]", path, data[6 + ht * 3], data[7 + ht * 3], data[8 + ht * 3]);
        }
        if (frame == 0 || frame == 5 || op == 6) data[6 + nr * 3] = 0x40;
        else data[6 + nr * 3] = 0xf0;            // commands
        ht = 7 + nr * 3;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[mer] [ht](%d) open channel [%s] wr[0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x]",
                                        ht, path, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                                        data[7], data[8], data[9], data[10], data[11], data[12]);
    }
    data[ht + 0] = static_cast<uint8_t>(adr & 0xff);
    data[ht + 1] = op;

    if (op == 0x1 || op == 0x2) // open/close session
    {
        if (this->deviceType == TYPE_MERCURY230) {
            if (prm == 0) {
                data[ht + 2] = 2;
                data[ht + 3] = 2;
                data[ht + 4] = 2;
                data[ht + 5] = 2;
                data[ht + 6] = 2;
                data[ht + 7] = 2;
                data[ht + 8] = 2;
            } else {
                data[ht + 2] = 1;
                data[ht + 3] = 1;
                data[ht + 4] = 1;
                data[ht + 5] = 1;
                data[ht + 6] = 1;
                data[ht + 7] = 1;
                data[ht + 8] = 1;
            }
            crc = CRC((uint8_t *)(data + ht), 9, 0);
            data[ht + 9] = static_cast<uint8_t>(crc / 256);
            data[ht + 10] = static_cast<uint8_t>(crc % 256);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                            "[mer][%d][%s] open channel [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                            this->id, path, op, prm, data[ht + 0], data[ht + 1], data[ht + 2],
                                            data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7],
                                            data[ht + 8], data[ht + 9], data[ht + 10]);

            if (this->protocol == 13) {
                crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(ht + 10));
                data[ht + 11] = static_cast<uint8_t>(crc % 256);
                data[ht + 12] = static_cast<uint8_t>(crc / 256);
                //for (len=0; len<=ht+12;len++) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
                //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x] [%d]",data[ht+11],data[ht+12],ht);
                ht += 2;
            }
            //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr [%d]",11+ht);
            write(fd, &data, 11 + ht);
        }
        if (this->deviceType == TYPE_SET_4TM) {
            data[ht + 2] = 0x30;
            data[ht + 3] = 0x30;
            data[ht + 4] = 0x30;
            data[ht + 5] = 0x30;
            data[ht + 6] = 0x30;
            data[ht + 7] = 0x30;
            crc = CRC((uint8_t * )(data + ht), 8, 0);
            data[ht + 8] = static_cast<uint8_t>(crc / 256);
            data[ht + 9] = static_cast<uint8_t>(crc % 256);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                            "[set] open channel [%s] [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                            path, op, prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                            data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8],
                                            data[ht + 9], data[ht + 10]);
            if (this->protocol == 13) {
                crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(9 + ht));
                data[ht + 10] = static_cast<uint8_t>(crc % 256);
                data[ht + 11] = static_cast<uint8_t>(crc / 256);
                //for (len=0; len<=ht+11;len++)   currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[set] %d=%x",len,data[len]);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[set] wr crc [0x%x,0x%x]", data[ht + 11],
                                                data[ht + 12]);
                ht += 2;
            }
            write(fd, &data, 10 + ht);
        }
    }
    if (op == 0x4) // time
    {
        data[ht + 2] = 0x0;
        crc = CRC((uint8_t * )(data + ht), 3, 0);
        data[ht + 3] = static_cast<uint8_t>(crc / 256);
        data[ht + 4] = static_cast<uint8_t>(crc % 256);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] read time [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]", op,
                                        prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4]);
        if (this->protocol == 13) {
            crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(4 + ht));
            data[ht + 5] = static_cast<uint8_t>(crc % 256);
            data[ht + 6] = static_cast<uint8_t>(crc / 256);
            //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);
            //for (len=0; len<=ht+6;len++) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
            ht += 2;
        }
        write(fd, &data, 5 + ht);
    }

    if (frame != 5)
        if (op == 0x5 || op == 0x8 || op == 0x11 || op == 0x14) // nak
        {
            data[ht + 2] = prm;
            if (index < 0xff) {
                data[ht + 3] = index;
                crc = CRC((uint8_t * )(data + ht), 4, 0);
                data[ht + 4] = static_cast<uint8_t>(crc / 256);
                data[ht + 5] = static_cast<uint8_t>(crc % 256);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                "[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                                op, prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                                data[ht + 4], data[ht + 5]);
                if (this->protocol == 13) {
                    crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(5 + ht));
                    data[ht + 6] = static_cast<uint8_t>(crc % 256);
                    data[ht + 7] = static_cast<uint8_t>(crc / 256);
                    //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
                    //for (len=0; len<=ht+7;len++)   currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
                    ht += 2;
                }
                write(fd, &data, 6 + ht);
            } else {
                data[ht + 2] = prm;
                crc = CRC((uint8_t * )(data + ht), 3, 0);
                data[ht + 3] = static_cast<uint8_t>(crc / 256);
                data[ht + 4] = static_cast<uint8_t>(crc % 256);
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                "[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]", op,
                                                prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                                data[ht + 4]);
                if (this->protocol == 13) {
                    crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(4 + ht));
                    data[ht + 5] = static_cast<uint8_t>(crc % 256);
                    data[ht + 6] = static_cast<uint8_t>(crc / 256);
                    //for (len=0; len<=ht+6;len++)		    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
                    //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);
                    ht += 2;
                }
                write(fd, &data, 5 + ht);
            }
        }

    if (op == 0x6) {
        data[ht + 2] = prm;
        data[ht + 3] = static_cast<uint8_t>(index / 256);
        data[ht + 4] = static_cast<uint8_t>(index % 256);
        data[ht + 5] = frame;
        crc = CRC((uint8_t * )(data + ht), 6, 0);
        data[ht + 6] = static_cast<uint8_t>(crc / 256);
        data[ht + 7] = static_cast<uint8_t>(crc % 256);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[mer] read parametrs!! [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", op,
                                        prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5]);
        if (this->protocol == 13) {
            crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(7 + ht));
            data[ht + 8] = static_cast<uint8_t>(crc % 256);
            data[ht + 9] = static_cast<uint8_t>(crc / 256);
            //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
            //for (len=0; len<=ht+9;len++)   currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer-s] %d=%x(%d)",len,data[len],data[len]);
            ht += 2;
        }
        write(fd, &data, 8 + ht);
    }

    if (op == READ_DATA_230L) {
        data[ht + 2] = prm;
        data[ht + 3] = index;
        data[ht + 4] = 0;
        data[ht + 5] = 1;
        data[ht + 6] = 0;
        crc = CRC((uint8_t * )(data + ht), 7, 0);
        data[ht + 7] = static_cast<uint8_t>(crc / 256);
        data[ht + 8] = static_cast<uint8_t>(crc % 256);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[mer] read long [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        op, prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                        data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8]);
        if (this->protocol == 13) {
            crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(8 + ht));
            data[ht + 9] = static_cast<uint8_t>(crc % 256);
            data[ht + 10] = static_cast<uint8_t>(crc / 256);
            //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
            //for (len=0; len<=ht+7;len++)   currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
            ht += 2;
        }
        write(fd, &data, 9 + ht);
    }

    if (frame == 5)
        if (op == 0x5 || op == 0x8 || op == 0x11 || op == 0x14) // nak
        {
            data[ht + 2] = prm;
            data[ht + 3] = 1;
            data[ht + 4] = index;
            crc = CRC((uint8_t * )(data + ht), 5, 0);
            data[ht + 5] = static_cast<uint8_t>(crc / 256);
            data[ht + 6] = static_cast<uint8_t>(crc % 256);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                            "[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                            op, prm, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                            data[ht + 4], data[ht + 5], data[ht + 6]);
            if (this->protocol == 13) {
                crc = Crc16((uint8_t * )(data + 1), static_cast<const uint8_t>(6 + ht));
                data[ht + 7] = static_cast<uint8_t>(crc % 256);
                data[ht + 8] = static_cast<uint8_t>(crc / 256);
                //for (len=0; len<=ht+8;len++)		    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] %d=%x(%d)",len,data[len],data[len]);
                //if (debug>3) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);

                ht += 2;
            }
            write(fd, &data, 7 + ht);
        }

    if (0 && op == 0x8) // parametrs
    {
        data[ht + 2] = prm;
        crc = CRC((uint8_t *) data, 3, 0);
        data[ht + 3] = static_cast<uint8_t>(crc / 256);
        data[ht + 4] = static_cast<uint8_t>(crc % 256);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]", op, prm,
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4]);
        write(fd, &data, 5);
    }
    return true;
}

//-----------------------------------------------------------------------------
u_int16_t DeviceMER::read_mercury(u_int8_t *dat, u_int8_t type) {
    if (this->deviceType == TYPE_INPUT_CE)
        return this->read_ce(dat, type);

    unsigned crc = 0;        //(* CRC checksum *)
    int nbytes = 0;     //(* number of bytes in recieve packet *)
    int bytes = 0;      //(* number of bytes in packet *)
    uint8_t data[500];      //(* recieve sequence *)
    unsigned i = 0;            //(* current position *)
    char op = 0;           //(* operation *)
    unsigned crc_in = 0;    //(* CRC checksum *)

//     sleep(1);
    usleep(500000);
    ioctl(fd, FIONREAD, &nbytes);
    nbytes = static_cast<int>(read(fd, &data, 75));
    usleep(200000);
    ioctl(fd, FIONREAD, &bytes);

    if (bytes > 0 && nbytes > 0 && nbytes < 50) {
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[set] bytes=%d fd=%d adr=%d",bytes,fd,&data+nbytes);
        bytes = static_cast<int>(read(fd, &data + nbytes, bytes));
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[set] bytes=%d",bytes);
        nbytes += bytes;
        //for (i=0; i<nbytes;i++)	    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[set] %d=%x",i,data[i]);
    }

    if (nbytes >= 4) {
        if (this->protocol == 14)
            crc = CRC((uint8_t * )(data + 1), static_cast<const uint8_t>(nbytes - 3), 0);
        else crc = Crc16(data + 1, static_cast<const uint8_t>(nbytes - 3));
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[mer] READ [%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x][crc][0x%x,0x%x]",
                                        nbytes, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                                        data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14],
                                        data[15], data[16], data[17], data[18], crc / 256, crc % 256);
        //for (UINT rr=0; rr<nbytes;rr++) if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] [%d][0x%x]",rr,data[rr]);
        if (this->protocol == 14)
            crc_in = static_cast<unsigned int>(data[nbytes - 2] * 256 + data[nbytes - 1]);
        else crc_in = static_cast<unsigned int>(data[nbytes - 2] + data[nbytes - 1] * 256);
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[mer] [%d]c[0x%x,0x%x]i[0x%x,0x%x] [%x %x]",nbytes,crc%256,crc/256,data[nbytes-2],data[nbytes-1],crc,crc_in);
        if (this->protocol == 13 && (crc != crc_in || nbytes < 3))
            nbytes = 0;
        else {
            if (this->protocol == 14)
                memcpy(dat, data + 1, static_cast<size_t>(nbytes - 3));
            if (this->protocol == 13) {
                if (nbytes > 16) {
                    if (nbytes > 50) // memory
                        memcpy(dat, data + 10, static_cast<size_t>(nbytes - 10));
                    else
                        memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
                    return static_cast<u_int16_t>(nbytes);
                } else {
                    if (nbytes == 12 && (data[9] == 0xf0 || data[9] == 0x40)) {
                        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                        "[mer] HTC answer: no counter answer present [%d]", data[9]);
                    }
                    if (nbytes == 16 && ((data[11] & 0xf) == 0x5))
                        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[mer] channel not open correctly [%d]",
                                                        data[11]);
                    memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
                }
            }
        }
        return static_cast<u_int16_t>(nbytes);
    } else {
        //Events (dbase, 1, this->device, 1, 0, 3, 0);
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::send_ce(uint16_t op, u_int16_t prm, char *request, u_int8_t frame) {
    unsigned crc = 0;          //(* CRC checksum *)
    unsigned ht = 0, nr = 0, len = 0, nbytes = 0;     //(* number of bytes in send packet *)
    unsigned char data[100];      //(* send sequence *)
    char path[100] = {0};
    char adr[10] = {0};

    if (op == SN) // open/close session
        len = 8;
    if (op == 0x4) // time
        len = 13;
    if (op == OPEN_PREV)
        len = 6;
    if (op == OPEN_CHANNEL)
        len = 14;
    if (op == 0x88)
        len = 18;
    if (op == 0x89)
        len = 5;

    if (op == READ_DATE || op == READ_TIME || op == READ_PARAMETERS)
        len = 13;
    if (op == ARCH_MONTH || op == ARCH_DAYS)
        len = static_cast<uint32_t>(strlen(request) + 6);

    if (this->protocol == 13) {
        for (ht = 0; ht < 8; ht++)
            if (!address[ht]) break;
            else nr++;

        data[0] = 1;    // redunancy
        data[1] = static_cast<uint8_t>(6 + 3 * nr + len);
        data[2] = static_cast<uint8_t>(16 + (nr - 1));
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;    // comp address
        for (ht = 0; ht < nr; ht++) {
            data[6 + ht * 3] = static_cast<uint8_t>(address[ht] / 0x10000);
            data[7 + ht * 3] = static_cast<uint8_t>((address[ht] & 0xff00) / 0x100);
            data[8 + ht * 3] = static_cast<uint8_t>(address[ht] & 0xff);    // PPL-N address
            sprintf(path, "%s[%d.%d.%d]", path, data[6 + ht * 3], data[7 + ht * 3], data[8 + ht * 3]);
        }
        data[6 + nr * 3] = 0x40;
        //data[6+nr*3]=0xf0;			// commands
        ht = 7 + nr * 3;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303] [ht](%d) open channel [%s] wr[0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x]",
                                        ht, path, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                                        data[7], data[8], data[9], data[10], data[11], data[12]);
    }

//     data[ht+0]=Adr&0xff;
//     data[ht+1]=op;

    if (op == SN) {
        data[ht + 0] = START;
        data[ht + 1] = REQUEST;
        sprintf(adr, "%d", prm);
        data[ht + 2] = static_cast<uint8_t>(adr[0]);
        data[ht + 3] = static_cast<uint8_t>(adr[1]);
        data[ht + 4] = static_cast<uint8_t>(adr[2]);
        data[ht + 5] = 0x21;
        data[ht + 6] = CR;
        data[ht + 7] = LF;
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303][SN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] [0x%x][0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8]);
        for (len = 0; len <= ht + 7; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303][SN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] [0x%x][0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8]);
        if (this->protocol == 13) {
            crc = Crc16(data + 1, static_cast<const uint8_t>(7 + ht));
            data[ht + 8] = static_cast<uint8_t>(crc % 256);
            data[ht + 9] = static_cast<uint8_t>(crc / 256);
        }
        for (len = 0; len <= ht + 9; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, 10 + ht);
    }

    if (op == OPEN_PREV) {
        data[ht + 0] = 0x6;
        data[ht + 1] = 0x30;
        data[ht + 2] = 0x35;
        data[ht + 3] = 0x31;
        data[ht + 4] = CR;
        data[ht + 5] = LF;
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
        for (len = 0; len <= 5; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", data[ht + 0],
                                        data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5]);
        if (this->protocol == 13) {
            crc = Crc16(data + 1, static_cast<const uint8_t>(5 + ht));
            data[ht + 6] = static_cast<uint8_t>(crc % 256);
            data[ht + 7] = static_cast<uint8_t>(crc / 256);
        }
        for (len = 0; len < ht + 8; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, 8 + ht);
    }

    if (op == 0x88) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        data[ht + 4] = 0x45; // ENMPE
        data[ht + 5] = 0x4E;
        data[ht + 6] = 0x4D;
        data[ht + 7] = 0x50;
        data[ht + 8] = 0x45;
        data[ht + 9] = 0x28;
        data[ht + 10] = 0x31;
        data[ht + 11] = 0x32;
        data[ht + 12] = 0x2E;
        data[ht + 13] = 0x31;
        data[ht + 14] = 0x34;
        data[ht + 15] = 0x29;
        data[ht + 16] = ETX;
        data[ht + 17] = CRC_CE(data + ht + 1, 16, 1);
        //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
        for (len = 0; len < ht + 18; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][SNUM] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5]);
        if (this->protocol == 13) {
            crc = Crc16(data + 1, static_cast<const uint8_t>(17 + ht));
            data[ht + 18] = static_cast<uint8_t>(crc % 256);
            data[ht + 19] = static_cast<uint8_t>(crc / 256);
        }
        for (len = 0; len < ht + 20; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, 20 + ht);
    }

    if (op == 0x89 && 0) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x42;
        data[ht + 2] = 0x30;
        data[ht + 3] = 0x3;
        data[ht + 4] = 0x75;
        for (len = 0; len < 5; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][HZ] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", data[ht + 0],
                                        data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5]);
        if (this->protocol == 13) {
            crc = Crc16(data + 1, static_cast<const uint8_t>(4 + ht));
            data[ht + 5] = static_cast<uint8_t>(crc % 256);
            data[ht + 6] = static_cast<uint8_t>(crc / 256);
        }
        for (len = 0; len < ht + 7; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, 7 + ht);
    }

    if (op == OPEN_CHANNEL_CE) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x50;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        data[ht + 4] = 0x28;
        // default password
        if (prm == 0) {
            data[ht + 5] = 0x37;
            data[ht + 6] = 0x37;
            data[ht + 7] = 0x37;
            data[ht + 8] = 0x37;
            data[ht + 9] = 0x37;
            data[ht + 10] = 0x37;
        } else {
            data[ht + 5] = 0x31;
            data[ht + 6] = 0x31;
            data[ht + 7] = 0x31;
            data[ht + 8] = 0x31;
            data[ht + 9] = 0x31;
            data[ht + 10] = 0x31;
        }
        // true password
        //memcpy (data+ht+5,this->pass,6); !!!!!


        data[ht + 11] = 0x29;
        data[ht + 12] = ETX;
        data[ht + 13] = CRC_CE(data + ht + 1, 12, 1);
        for (len = 0; len <= 13; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303][OPEN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8], data[ht + 9],
                                        data[ht + 10], data[ht + 11], data[ht + 12], data[ht + 13]);
        crc = Crc16(data + 1, static_cast<const uint8_t>(13 + ht));
        data[ht + 14] = static_cast<uint8_t>(crc % 256);
        data[ht + 15] = static_cast<uint8_t>(crc / 256);
        for (len = 0; len < ht + 16; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, ht + 16);
    }

    if (op == READ_DATE || op == READ_TIME) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        if (op == READ_DATE) sprintf((char *) data + ht + 4, "DATE_()");
        if (op == READ_TIME) sprintf((char *) data + ht + 4, "TIME_()");
        data[ht + 11] = ETX;
        data[ht + 12] = CRC_CE(data + ht + 1, 11, 1);
        for (len = 0; len <= ht + 12; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303][DATE|TIME] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8], data[ht + 9],
                                        data[ht + 10], data[ht + 11], data[ht + 12]);
        crc = Crc16(data + 1, static_cast<const uint8_t>(12 + ht));
        data[ht + 13] = static_cast<uint8_t>(crc % 256);
        data[ht + 14] = static_cast<uint8_t>(crc / 256);
        for (len = 0; len < ht + 15; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, ht + 15);
    }
    if (op == READ_PARAMETERS) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;

        if (prm == CURRENT_W) sprintf((char *) data + ht + 4, "POWPP()");
        //if (op==CURRENT_I) sprintf (data+4,"CURRE()");
        if (prm == CURRENT_I) sprintf((char *) data + ht + 4, "ET0PE()");
        if (prm == CURRENT_F) sprintf((char *) data + ht + 4, "FREQU()");
        if (prm == CURRENT_U) sprintf((char *) data + ht + 4, "VOLTA()");
        if (prm == 30) sprintf((char *) data + ht + 4, "CONDI()");
        if (prm == 31) sprintf((char *) data + ht + 4, "SNUMB()");
        if (prm == 32) sprintf((char *) data + ht + 4, "CURRE()");
        if (prm == 33) sprintf((char *) data + ht + 4, "VOLTA()");

        data[ht + 11] = ETX;
        data[ht + 12] = CRC_CE(data + ht + 1, 11, 1);
        for (len = 0; len <= 12; len++) data[ht + len] = Ct(data[ht + len]);

        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303][CURR] wr[%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        13, data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8], data[ht + 9],
                                        data[ht + 10], data[ht + 11], data[ht + 12]);
        crc = Crc16(data + 1, static_cast<const uint8_t>(12 + ht));
        data[ht + 13] = static_cast<uint8_t>(crc % 256);
        data[ht + 14] = static_cast<uint8_t>(crc / 256);
        for (len = 0; len < ht + 15; len++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %d=%x(%c)", len, data[len] & 0x7f, data[len] & 0x7f);
        write(fd, &data, ht + 15);
    }
    if (op == ARCH_MONTH || op == ARCH_DAYS) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        sprintf((char *) data + ht + 4, "%s", request);
        len = static_cast<uint32_t>(strlen(request) + 3);
        data[ht + len + 1] = ETX;
        data[ht + len + 2] = CRC_CE(data + ht, static_cast<const uint8_t>(len + 2), 1);
        //crc=CRC (data+ht, 7, 0);
        for (len = 0; len <= ht + len + 2; len++) data[ht + len] = Ct(data[ht + len]);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[303][ARCH] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4],
                                        data[ht + 5], data[ht + 6], data[ht + 7], data[ht + 8], data[ht + 9],
                                        data[ht + 10], data[ht + 11], data[ht + 12], data[ht + 13], data[ht + 14],
                                        data[ht + 15], data[ht + 16], data[ht + 17]);
        crc = Crc16(data + 1, static_cast<const uint8_t>(len + 2 + ht));
        data[ht + len + 3] = static_cast<uint8_t>(crc % 256);
        data[ht + len + 4] = static_cast<uint8_t>(crc / 256);
        write(fd, &data, ht + len + 5);
    }

    return true;
}

//-----------------------------------------------------------------------------
u_int16_t DeviceMER::read_ce(u_int8_t *dat, u_int8_t type) {
    unsigned crc_in = 0, crc = 0;        //(* CRC checksum *)
    int nbytes = 0;     //(* number of bytes in recieve packet *)
    int bytes = 0;      //(* number of bytes in packet *)
    uint8_t data[500];      //(* recieve sequence *)
    uint8_t data2[500];      //(* recieve sequence *)
    unsigned i = 0;            //(* current position *)
    unsigned char ok = 0xFF;        //(* flajochek *)
    char op = 0;           //(* operation *)
    unsigned rr = 0;

    usleep(700000);
    ioctl(fd, FIONREAD, &nbytes);
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] nbytes=%d", nbytes);
    nbytes = static_cast<int>(read(fd, &data, 100));
//     if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303] nbytes=%d",nbytes);
    //if (nbytes>0)for (rr=0; rr<nbytes;rr++) if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[ce] [%d][0x%x] (%c)",rr,data[rr]&0x7f,data[rr]&0x7f);

    //data[0]=0x2;
    //sprintf (data+1,"POWEP(%.5f)",8.14561);
    //data[13]=0x3; data[14]=CRC (data+1,13,1); nbytes=15;
    //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303] nbytes=%d %x",nbytes,data[0]);
    //tcsetattr (fd,TCSAFLUSH,&tio);
    //usleep (200000);
    //ioctl (fd,FIONREAD,&bytes);
    //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303] bytes=%d",bytes);
    //bytes=read (fd, &data+nbytes, 125);
    //if (debug>2) currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,"[303] bytes=%d",nbytes);

    if (nbytes == 1) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] [%d][0x%x]", nbytes, data[0]);
        dat[0] = data[0];
        return static_cast<uint32_t>(nbytes);
    }
    if (nbytes > 5) {
//	 crc=CRC (data+1, nbytes-2, 1);
        //crc=CRC (data+1, nbytes-3, 0);
        crc = Crc16(data + 1, static_cast<const uint8_t>(nbytes - 3));
        crc_in = static_cast<uint32_t>(data[nbytes - 2] + data[nbytes - 1] * 256);

        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                        "[ce] [%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x][crc][0x%x,0x%x]",
                                        nbytes, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                                        data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14],
                                        data[15], data[16], data[17], data[nbytes - 1], crc);
        for (rr = 0; rr < nbytes; rr++) data[rr] = static_cast<uint8_t>(data[rr] & 0x7f);
        for (rr = 0; rr < nbytes; rr++)
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] [%d][0x%x] (%c)", rr, data[rr] & 0x7f,
                                            data[rr] & 0x7f);
        // if (data[nbytes-1]!=0xa && data[nbytes-2]!=0xd)
        //    if (crc!=crc_in) nbytes=0;

        if (nbytes < 100 && nbytes > 7) {
            if (nbytes > 16)
                memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
            else {
                if (nbytes == 12 && (data[9] == 0xf0 || data[9] == 0x40)) {
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] HTC answer: no counter answer present [%d]",
                                                    data[9]);
                }
                if (nbytes == 16 && ((data[11] & 0xf) == 0x5))
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] channel not open correctly [%d]", data[11]);
                memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
            }
        } else dat[0] = 0;
        return static_cast<uint32_t>(nbytes);
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::ReadInfoCE() {
    u_int16_t res, serial, soft;
    bool rs;
    uint8_t data[400];
    char date[20] = {0};
    unsigned char time[20] = {0};

    rs = send_ce(SN, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (rs)
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] %s[serial=%s]%s", bright, data, nc);

    rs = send_ce(OPEN_PREV, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
// if (rs)  if (debug>2) ULOGW ("[303] [open channel prev: %d]",data[0]);

    rs = send_ce(OPEN_CHANNEL_CE, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (rs)
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [open channel answer: %d]", data[0]);

    rs = send_ce(READ_DATE, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (rs) {
        memcpy(date, data + 9, 8);
        rs = send_ce(READ_TIME, 0, date, 0);
        if (rs) res = this->read_ce(data, 0);
        if (rs) {
            memcpy(time, data + 6, 8);
            sprintf((char *) data, "%s %s", date, time);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [date=%s]", data);
            sprintf(this->dev_time, "20%c%c%c%c%c%c%c%c%c%c%c%c", data[6], data[7], data[3], data[4], data[0], data[1],
                    data[9], data[10], data[12], data[13], data[15], data[16]);
            //sprintf(query, "UPDATE device SET lastdate=NULL,conn=1,devtim=%s WHERE id=%d", this->dev_time, this->id);
            //res = dBase.sqlexec(query);
            //if (res) mysql_free_result(res);
        }
    }
}

//-----------------------------------------------------------------------------
// ReadDataCurrent - read single device. Readed data will be stored in DB
int DeviceMER::ReadDataCurrentCE() {
    uint16_t res;
    bool rs;
    char *chan;
    float fl;
    unsigned char data[400];
    char date[20] = {0};
    char param[20];
    this->q_attempt++;  // attempt

    rs = this->send_ce(CURRENT_W, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (rs) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "POWEP(%s)", param));
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] %d", data, rs);
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] W=[%f]", param, fl);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
                free(chan);
            }
        }
    }
    rs = send_ce(CURRENT_U, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "VOLTA(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] V=[%f]", param, fl);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
                free(chan);
            }
        }
    }
    rs = send_ce(CURRENT_F, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "FREQU(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] F=[%f]", param, fl);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
                free(chan);
            }
        }
    }
    rs = send_ce(CURRENT_I, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "ET0PE(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] F=[%f]", param, fl);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
                free(chan);
            }
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
// ReadDataArchive - read single device. Readed data will be stored in DB
int DeviceMER::ReadAllArchiveCE(uint16_t tp) {
    bool rs;
    char *chan;
    uint16_t res;
    uint8_t data[400];
    char date[20], param[20];
    uint16_t month, year, index;
    float fl;
    uint8_t code, vsk = 0;
    time_t tims, tim;
    tims = time(&tims);
    tims = time(&tims);
    auto tt = (tm *) malloc(sizeof(struct tm));
    this->q_attempt++;  // attempt

    tim = time(&tim);
    localtime_r(&tim, tt);
    for (int i = 0; i < tp; i++) {
        if (tt->tm_mon == 0) {
            tt->tm_mon = 12;
            tt->tm_year--;
        }
        sprintf(date, "EAMPE(%d.%d)", tt->tm_mon + 1, tt->tm_year - 100);        // ddMMGGtt
        rs = send_ce(ARCH_MONTH, 0, date, 1);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            rs = static_cast<bool>(sscanf((const char *) data, "EAMPE(%s)", param));
            fl = static_cast<float>(atof(param));
            sprintf(date, "%04d%02d01000000", tt->tm_year + 1900, tt->tm_mon);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                            data[0], data[1], data[2], data[3], fl);
            chan = dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid);
            if (chan != nullptr) {
                dBase.StoreData(TYPE_MONTH, 0, fl, date, chan);
                free(chan);
            }
        }
/*

        sprintf(date, "ENMPE(%d.%d)", tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_MONTH, 0, date, 1);
        if (rs) rs = this->read_ce(data, 0);
        if (rs) {
            rs = sscanf((const char *) data, "ENMPE(%s)", param);
            fl = atof(param);
            sprintf(this->lastdate, "%04d%02d01000000", tt->tm_year + 1900, tt->tm_mon);
            if (debug > 2)
                ULOGW("[303][0x%x 0x%x 0x%x 0x%x] [%f] [%s]", bright, data[0], data[1], data[2], data[3], fl,
                      this->lastdate, nc);
            //UpdateThreads (dbase, TYPE_MERCURY230-1, 1, 1, 14, this->device, 1, 1, this->lastdate);
            StoreData(dbase, this->device, 14, 2, 4, 0, fl, this->lastdate, 0, chan);
        }
        tt->tm_mon--;
    }

    tim = time(&tim);
    localtime_r(&tim, tt);
    for (int i = 0; i < tp; i++) {
//     sprintf (date,"%02d%02d%02d%02d",tt->tm_mday,tt->tm_mon+1,tt->tm_year,0);	// ddMMGGtt
        sprintf(date, "EADPE(%d.%d.%d)", tt->tm_mday, tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_DAYS, 0, date, 1);
        if (rs) rs = this->read_ce(data, 0);
        if (rs) {
            fl = ((float) data[0] + (float) data[1] * 256 + (float) data[2] * 256 * 256) / 100;
            if (debug > 2) ULOGW("[303][0x%x 0x%x 0x%x 0x%x] [%f]", data[0], data[1], data[2], data[3], fl);
            sprintf(this->lastdate, "%04d%02d%02d%02d0000", tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday,
                    tt->tm_hour);
            if (fl > 0) StoreData(dbase, this->device, 14, 0, 2, 0, fl, this->lastdate, 0, chan);
        }
        sprintf(date, "ENDPE(%d.%d.%d)", tt->tm_mday, tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_DAYS, 0, date, 1);
        if (rs) rs = this->read_ce(data, 0);
        if (rs) {
            fl = ((float) data[0] + (float) data[1] * 256 + (float) data[2] * 256 * 256) / 100;
            if (debug > 2) ULOGW("[303][0x%x 0x%x 0x%x 0x%x] [%f]", data[0], data[1], data[2], data[3], fl);
            sprintf(this->lastdate, "%04d%02d%02d%02d0000", tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday,
                    tt->tm_hour);
            if (fl > 0) StoreData(dbase, this->device, 14, 2, 2, 0, fl, this->lastdate, 0, chan);
        }

        tim -= 3600 * i;
        localtime_r(&tim, tt);
        */

    }
    free(tt);
    return 0;
}

//--------------------------------------------------------------------------------------
bool OpenCom(char *block, uint16_t speed, uint16_t parity) {
    Kernel &currentKernelInstance = Kernel::Instance();
    char dev_pointer[50];
    static termios tio;
    sprintf(dev_pointer, "%s", block);
    currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[mer] attempt open com-port %s on speed %d", dev_pointer, speed);
    fd = open(dev_pointer, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[mer] error open com-port %s", dev_pointer);
        return false;
    } else
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[mer] open com-port success");
    tcflush(fd, TCIOFLUSH);    //Clear send & receive buffers
    tcgetattr(fd, &tio);
    cfsetospeed(&tio, baudrate(speed));
    // tio.c_cflag =0;

    if (parity == 1)
        //     tio.c_cflag = CREAD|CLOCAL|baudrate(speed)|CS7|PARENB;	// CSIZE
        tio.c_cflag = CREAD | CLOCAL | baudrate(speed) | PARODD;    // CSIZE

    else
        tio.c_cflag = CREAD | CLOCAL | baudrate(speed); //|CS8;

    if (parity == 0)
        tio.c_cflag &= ~(CSIZE | PARENB | PARODD);

    tio.c_cflag &= ~CSTOPB;
    // tio.c_cflag |=CS8|PARENB|PARODD;
    tio.c_cflag |= CS8;
    // tio.c_cflag &= ~CRTSCTS;

    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_iflag = 0;

    // tio.c_iflag = IGNPAR|ISTRIP;	// !!!

    tio.c_iflag &= ~(INLCR | IGNCR | ICRNL);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    // tio.c_iflag &= (INLCR | ICRNL);
    // tio.c_iflag = IGNCR;
    tio.c_oflag &= ~(ONLCR);

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 5; //Time out in 10e-1 sec
    cfsetispeed(&tio, baudrate(speed));
    fcntl(fd, F_SETFL, FNDELAY);
    // fcntl(fd, F_SETFL, 0);
    tcsetattr(fd, TCSANOW, &tio);
    tcsetattr(fd, TCSAFLUSH, &tio);

    return true;
}

const uint8_t srCRCHi[] = {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1,
        0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1,
        0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1,
        0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40};

const uint8_t srCRCLo[] = {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C,
        0x0D, 0xCD,
        0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB,
        0xDA, 0x1A,
        0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12,
        0x13, 0xD3,
        0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35,
        0x34, 0xF4,
        0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8,
        0xE9, 0x29,
        0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7,
        0xE6, 0x26,
        0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6,
        0xA7, 0x67,
        0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9,
        0xA8, 0x68,
        0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74,
        0x75, 0xB5,
        0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53,
        0x52, 0x92,
        0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A,
        0x9B, 0x5B,
        0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D,
        0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40};

uint16_t CRC(uint8_t *Data, uint8_t DataSize, uint8_t type) {
    uint16_t InitCRC = 0xffff;
    uint16_t _CRC;
    uint8_t i, p;
    uint8_t arrCRC[2];

    for (p = 0; p < DataSize; p++) {
        arrCRC[0] = static_cast<uint8_t>(InitCRC % 256);
        arrCRC[1] = static_cast<uint8_t>(InitCRC / 256);
        i = arrCRC[1] ^ Data[p];
        arrCRC[1] = arrCRC[0] ^ srCRCHi[i];
        arrCRC[0] = srCRCLo[i];
        InitCRC = static_cast<uint16_t>(arrCRC[0] + arrCRC[1] * 256);
    }
    _CRC = static_cast<uint16_t>(arrCRC[0] + arrCRC[1] * 256);
    return _CRC;
}

uint8_t Ct(const uint8_t Data) {
    uint8_t i, p = 0;
    for (i = 1; i < 127; i *= 2)
        if (Data & i) p++;
    //ULOGW ("p=%d %d",p,p%2);
    if (p % 2 == 1) return static_cast<uint8_t>(Data + 128);
    else return Data;
}

uint16_t Crc16(uint8_t *Data, uint8_t DataSize) {
    uint8_t p = 0, w = 0, d = 0, q = 0;
    uint8_t sl = 0, sh = 0;
    for (p = 0; p < DataSize; p++) {
        d = Data[p];
        for (w = 0; w < 8; ++w) {
            q = 0;
            if (d & 1)++q;
            d >>= 1;    // d - байт данных
            if (sl & 1)++q;
            sl >>= 1;    // sl - младший байт контрольной суммы
            if (sl & 8)++q;
            if (sl & 64)++q;
            if (sh & 2)++q;    // sh - старший байт контрольной суммы
            if (sh & 1)sl |= 128;
            sh >>= 1;
            if (q & 1)sh |= 128;
        }
    }
//    sh|=128;
    return static_cast<uint16_t>(sl + sh * 256);
}

uint8_t CRC_CE(const uint8_t *Data, const uint8_t DataSize, uint8_t type) {
    uint8_t _CRC = 0;
    for (int i = 0; i < DataSize; i++) {
        if (Data[i] % 2 == 1) _CRC += (Data[i] | 0x80);
        else _CRC += ((Data[i]) & 0x7f);
    }
    if (_CRC > 0x80) _CRC -= 0x80;
    return _CRC;

    const unsigned char crc8tab[256] = {
            0x00, 0xb5, 0xdf, 0x6a, 0x0b, 0xbe, 0xd4, 0x61, 0x16, 0xa3, 0xc9, 0x7c, 0x1d, 0xa8,
            0xc2, 0x77, 0x2c, 0x99, 0xf3, 0x46, 0x27, 0x92, 0xf8, 0x4d, 0x3a, 0x8f, 0xe5, 0x50,
            0x31, 0x84, 0xee, 0x5b, 0x58, 0xed, 0x87, 0x32, 0x53, 0xe6, 0x8c, 0x39, 0x4e, 0xfb,
            0x91, 0x24, 0x45, 0xf0, 0x9a, 0x2f, 0x74, 0xc1, 0xab, 0x1e, 0x7f, 0xca, 0xa0, 0x15,
            0x62, 0xd7, 0xbd, 0x08, 0x69, 0xdc, 0xb6, 0x03, 0xb0, 0x05, 0x6f, 0xda, 0xbb, 0x0e,
            0x64, 0xd1, 0xa6, 0x13, 0x79, 0xcc, 0xad, 0x18, 0x72, 0xc7, 0x9c, 0x29, 0x43, 0xf6,
            0x97, 0x22, 0x48, 0xfd, 0x8a, 0x3f, 0x55, 0xe0, 0x81, 0x34, 0x5e, 0xeb, 0xe8, 0x5d,
            0x37, 0x82, 0xe3, 0x56, 0x3c, 0x89, 0xfe, 0x4b, 0x21, 0x94, 0xf5, 0x40, 0x2a, 0x9f,
            0xc4, 0x71, 0x1b, 0xae, 0xcf, 0x7a, 0x10, 0xa5, 0xd2, 0x67, 0x0d, 0xb8, 0xd9, 0x6c,
            0x06, 0xb3, 0xd5, 0x60, 0x0a, 0xbf, 0xde, 0x6b, 0x01, 0xb4, 0xc3, 0x76, 0x1c, 0xa9,
            0xc8, 0x7d, 0x17, 0xa2, 0xf9, 0x4c, 0x26, 0x93, 0xf2, 0x47, 0x2d, 0x98, 0xef, 0x5a,
            0x30, 0x85, 0xe4, 0x51, 0x3b, 0x8e, 0x8d, 0x38, 0x52, 0xe7, 0x86, 0x33, 0x59, 0xec,
            0x9b, 0x2e, 0x44, 0xf1, 0x90, 0x25, 0x4f, 0xfa, 0xa1, 0x14, 0x7e, 0xcb, 0xaa, 0x1f,
            0x75, 0xc0, 0xb7, 0x02, 0x68, 0xdd, 0xbc, 0x09, 0x63, 0xd6, 0x65, 0xd0, 0xba, 0x0f,
            0x6e, 0xdb, 0xb1, 0x04, 0x73, 0xc6, 0xac, 0x19, 0x78, 0xcd, 0xa7, 0x12, 0x49, 0xfc,
            0x96, 0x23, 0x42, 0xf7, 0x9d, 0x28, 0x5f, 0xea, 0x80, 0x35, 0x54, 0xe1, 0x8b, 0x3e,
            0x3d, 0x88, 0xe2, 0x57, 0x36, 0x83, 0xe9, 0x5c, 0x2b, 0x9e, 0xf4, 0x41, 0x20, 0x95,
            0xff, 0x4a, 0x11, 0xa4, 0xce, 0x7b, 0x1a, 0xaf, 0xc5, 0x70, 0x07, 0xb2, 0xd8, 0x6d,
            0x0c, 0xb9, 0xd3, 0x66};
    _CRC = 0;
    for (int i = 0; i < DataSize; i++) {
        _CRC = crc8tab[_CRC ^ Data[i]];
    }
    return _CRC;
}
