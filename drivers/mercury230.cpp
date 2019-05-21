//-----------------------------------------------------------------------------
#include "errors.h"
#include "main.h"
#include "mercury230.h"
#include "db.h"
#include "dbase.h"
#include <fcntl.h>
#include <sys/termios.h>
#include <pthread.h>
#include <cstdio>
#include <TypeThread.h>
#include <mysql/mysql.h>
#include <mysql.h>
#include <kernel.h>
#include <logs.h>
#include <mhash.h>
#include <tkDecls.h>
#include "function.h"

//-----------------------------------------------------------------------------
static MYSQL_RES *res;
static MYSQL_ROW row;
static char query[500];
static int fd = 0;
bool rs = true;
DeviceMER deviceMER;

bool OpenCom(char *block, UINT speed, UINT parity);

//-----------------------------------------------------------------------------
void *mekDeviceThread(void *pth) {
    TypeThread thread = *((TypeThread *) pth);
    DBase dBase;
    Kernel &currentKernelInstance = Kernel::Instance();
    if (dBase.openConnection()) {
        while (true) {
            sprintf(query, "SELECT * FROM device WHERE thread=%d", thread.id);
            res = dBase.sqlexec(query);
            u_long nRow = mysql_num_rows(res);
            for (u_long r = 0; r < nRow; r++) {
                row = mysql_fetch_row(res);
                if (row) {
                    strncpy(deviceMER.address, row[3], 10);
                    strncpy(deviceMER.port, row[16], 20);
                }
                if (!fd) {
                    rs = OpenCom(deviceMER.port, thread.speed, 0);
                    if (!rs) {
                        sleep(10);
                        continue;
                    }
                }
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadInfo (%s)", deviceMER.address);
                rs = deviceMER.ReadInfo();
                if (!rs) continue;
                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataCurrent (%d)", deviceMER.id);
                UpdateThreads(dBase, thread.id, 1, 1, 1, deviceMER.address, 1, 0, (CHAR *) "");
                deviceMER.ReadDataCurrent();
                if (currentKernelInstance.current_time->tm_min > 45) {
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataArchive (%d)", deviceMER.id);
                    deviceMER.ReadAllArchive(5);
                }
                if (thread.work == 0) {
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO,
                                                    "[m230] mercury 230 & set 4TM & CE303 threads stopped");
                    pthread_exit(nullptr);
                }
            }
            if (fd) close(fd);
            fd = 0;
        }
    }
    pthread_exit(nullptr);
}


// ReadDataCurrent - read single device. Readed data will be stored in DB
int DeviceMER::ReadDataCurrent() {
    bool rs;
    uint16_t rec;
    uint16_t chan;
    float fl;
    unsigned char data[400];
    char date[20];
    this->q_attempt++;
    if (this->deviceType == TYPE_MERCURY230)
        rs = send_mercury(READ_PARAMETRS, 0x11, ENERGY_SUM, 0);
    else
        rs = send_mercury(READ_PARAMETRS, 0x11, 0x8, 0);
    if (rs) rec = this->read_mercury(data, 0);
    if (rec>0) {
        if (this->deviceType == TYPE_SET_4TM) {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
            fl = fl / 500000;
        }
        else {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
            fl = fl / 100000;
        }
        // TODO решить что делать с коэффициентом трансформаци
        fl *= 6200;

        chan = GetChannelNum(dbase, 14, 0, this->device);
        StoreData(dbase, this->device, 14, 0, 0, fl, 0, chan);
        sprintf(date, "%04d%02d%02d%02d%02d00", currenttime->tm_year + 1900, currenttime->tm_mon + 1,
                currenttime->tm_mday, currenttime->tm_hour, (5 * (int) (currenttime->tm_min / 5)));
        StoreData(dbase, this->device, 14, 0, 9, 0, fl, date, 0, chan);
    }

    if (this->type == TYPE_MERCURY230)
        rs = send_mercury(READ_PARAMETRS, 0x11, 0xF0, 0);
    else rs = send_mercury(READ_PARAMETRS, 0xD, 0xFF, 0);

    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        if (this->type == TYPE_MERCURY230) {
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
            fl = fl / 100;
            if (this->rasknt) fl *= this->rasknt;
            if (this->knt) fl *= this->knt;
        } else {
            fl = ((data[0] * 256 * 256 * 256) + (data[1] * 256 * 256) + (data[2] * 256) + (data[3]));
            //fl=((data[0]*256*256)+(data[1]*256)+(data[2]));
            if (this->rasknt) fl *= this->rasknt;
            if (this->A > 0) fl = fl / (2 * this->A);
        }

        if (debug > 2) ULOGW("[mer][0x%x 0x%x 0x%x 0x%x] W=[%f]", data[0], data[1], data[2], data[3], fl);
        chan = GetChannelNum(dbase, 14, 2, this->device);
        if (fl < 50000000) {
            //StoreData (dbase, this->device, 14, 2, 0, fl, 0, chan);
            //sprintf (date,"%04d%02d%02d%02d%02d00",currenttime->tm_year+1900,currenttime->tm_mon+1,currenttime->tm_mday,currenttime->tm_hour,(5*(int)(currenttime->tm_min/5)));
            //StoreData (dbase, this->device, 14, 2, 9, 0, fl, date, 0, chan);
        }
    }


    if (this->type == TYPE_MERCURY230)
        rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);
    else rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);

    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        if (this->type == TYPE_MERCURY230) {
            //fl=((float)((data[0]&0x3f)*256*256)+(float)data[1]+(float)data[2]*256); //fl=fl/100;
            fl = (float) ((data[0]) * 256 * 256) + (float) (data[1] * 256 * 256 * 256) + (float) data[2] +
                 (float) (data[3] * 256); //fl=fl/100;
            //fl*=10; // !!!!!! 05.12.2014 fatal error
            if (this->rasknt > 0) {
                fl *= this->rasknt;
                if (this->A > 0) fl = fl / (2 * this->A);
                if (this->knt) fl *= this->knt;
            }
        } else {
            fl = ((data[0] * 256 * 256 * 256) + (data[1] * 256 * 256) + (data[2] * 256) + (data[3]));
            //fl=((data[0]*256*256)+(data[1]*256)+(data[2]));
            //if (debug>2) ULOGW ("[mer][0x%x 0x%x 0x%x 0x%x] Wn[%d]=[%f]",data[0],data[1],data[2],data[3],this->device,fl);
            if (this->rasknt > 0) {
                fl *= this->rasknt;
                if (this->A > 0) fl = fl / (2 * this->A);
            }
            //if (this->A>0) fl=fl*this->Kt*this->Kn/(2*this->A);
        }
        if (debug > 2)
            ULOGW("[mer][0x%x 0x%x 0x%x 0x%x] Wn[%d]=[%f] [%f][%f]", data[0], data[1], data[2], data[3], this->device,
                  fl, this->A, this->rasknt);
        chan = GetChannelNum(dbase, 14, 2, this->device);
        if (fl < 500000000) {
            StoreData(dbase, this->device, 14, 2, 0, fl, 0, chan);
            sprintf(date, "%04d%02d%02d%02d0000", currenttime->tm_year + 1900, currenttime->tm_mon + 1,
                    currenttime->tm_mday, currenttime->tm_hour, (5 * (int) (currenttime->tm_min / 5)));
            StoreData(dbase, this->device, 14, 2, 9, 0, fl, date, 0, chan);
        }
    }

// rs=send_mercury (READ_PARAMETRS, 0x11, I1, 0);
    rs = send_mercury(READ_PARAMETRS, 0x11, 0x20, 0);
    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        if (this->type == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 10000;
        if (debug > 2) ULOGW("[mer][0x%x 0x%x 0x%x] I3=[%f]", data[0], data[1], data[2], fl);
        chan = GetChannelNum(dbase, 14, 10, this->device);
        StoreData(dbase, this->device, 14, 10, 0, fl, 0, chan);
    }

    rs = send_mercury(READ_PARAMETRS, 0x11, U1, 0);
    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        if (this->type == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 100;
        if (debug > 2) ULOGW("[mer][0x%x 0x%x 0x%x] U3=[%f]", data[0], data[1], data[2], fl);
        chan = GetChannelNum(dbase, 14, 13, this->device);
        if (fl < 300) {
            StoreData(dbase, this->device, 14, 13, 0, fl, 0, chan);
            sprintf(date, "%04d%02d%02d%02d%02d00", currenttime->tm_year + 1900, currenttime->tm_mon + 1,
                    currenttime->tm_mday, currenttime->tm_hour, (5 * (int) (currenttime->tm_min / 5)));
            StoreData(dbase, this->device, 14, 13, 9, 0, fl, date, 0, chan);
        }
    }

    rs = send_mercury(READ_PARAMETRS, 0x11, F, 0);
    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        if (this->type == TYPE_SET_4TM)
            fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] * 256 + (float) data[2]);
        else fl = ((float) ((data[0] & 0x3f) * 256 * 256) + (float) data[1] + (float) data[2] * 256);
        fl = fl / 100;
        if (debug > 2) ULOGW("[mer][0x%x 0x%x 0x%x] F=[%f]", data[0], data[1], data[2], fl);
        chan = GetChannelNum(dbase, 14, 44, this->device);
        if (fl < 300) {
            StoreData(dbase, this->device, 14, 44, 0, fl, 0, chan);
            sprintf(date, "%04d%02d%02d%02d%02d00", currenttime->tm_year + 1900, currenttime->tm_mon + 1,
                    currenttime->tm_mday, currenttime->tm_hour, (5 * (int) (currenttime->tm_min / 5)));
            StoreData(dbase, this->device, 14, 44, 9, 0, fl, date, 0, chan);
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::ReadInfo() {
    UINT rs, serial = 0, soft, cnt;
    BYTE data[400];
    CHAR date[20] = {0};
    CHAR time[20] = {0};

    float kn, kt, A;

    rs = send_mercury(OPEN_CHANNEL, 0, 0, 0);
    usleep(400000);
    rs = this->read_mercury(data, 0);

    if (!rs || data[0] != 0) {
        rs = send_mercury(OPEN_CHANNEL, 0, 0, 1);
        usleep(200000);
        rs = this->read_mercury(data, 0);
        if (!rs || data[0] != 0) {
            rs = send_mercury(OPEN_CHANNEL, 1, 0, 0);
            usleep(200000);
            rs = this->read_mercury(data, 0);
        }
    }
    if (rs && data[0] == 0) {
        ULOGW("[mer] open channel success %x %x [%d]", data[0], data[1], rs);
        rs = send_mercury(READ_PARAMETRS, 0x0, 0xff, 0);
        if (rs) rs = this->read_mercury(data, 0);
        if (rs) {
            serial = data[0] + data[1] * 256 + data[2] * 256 * 256 + data[3] * 256 * 256 * 256;
            if (this->type == TYPE_SET_4TM)
                serial = data[3] + data[2] * 256 + data[1] * 256 * 256 + data[0] * 256 * 256 * 256;
            if (debug > 2) ULOGW("[mer] [serial=%d (%d/%d/%d)]", serial, data[4], data[5], data[6]);
        }

        rs = this->read_mercury(data, 0);

        rs = send_mercury(READ_PARAMETRS, 0x2, 0xff, 0);
        if (rs) rs = this->read_mercury(data, 0);
        if (rs) {
            soft = data[0] + data[1] * 256 + data[2] * 256 * 256;
            if (debug > 2) ULOGW("[mer] [soft=%d]", soft);
        }

        rs = this->read_mercury(data, 0);

        for (cnt = 1; cnt < 3; cnt++) {
            rs = send_mercury(READ_TIME_230, 0x0, 0xff, 0);
            if (rs) rs = this->read_mercury(data, 0);
            if (rs && serial > 0) {
                sprintf(date, "%02d-%02d-%d %02d:%02d:%02d", BCD(data[4]), BCD(data[5]), BCD(data[6]), BCD(data[2]),
                        BCD(data[1]), BCD(data[0]));
                sprintf(this->devtim, "%04d%02d%02d%02d%02d%02d", 2000 + BCD(data[6]), BCD(data[5]), BCD(data[4]),
                        BCD(data[2]), BCD(data[1]), BCD(data[0]));
                if (debug > 2) ULOGW("[mer] [date=%s]", date);
                if (serial > 0)
                    sprintf(query, "UPDATE device SET lastdate=NULL,conn=1,devtim=%s WHERE id=%d", this->devtim,
                            this->iddev);
                else
                    sprintf(query, "UPDATE device SET lastdate=NULL,conn=1,devtim=%s WHERE id=%d", this->devtim,
                            this->iddev);
                if (debug > 2) ULOGW("[mer] [%s]", query);
                this->qerrors = 0;
                if (BCD(data[6]) < 20 && BCD(data[6]) > 0) {
                    res = dbase.sqlexec(query);
                    if (res) mysql_free_result(res);
                    break;
                }
            }
        }

        rs = send_mercury(READ_PARAMETRS, 0x02, 0xff, 0);
        if (rs) rs = this->read_mercury(data, 0);
        if (rs) {
            kn = data[1];
            kt = data[3];
            rs = send_mercury(READ_PARAMETRS, 0x12, 0xff, 0);
            if (rs) rs = this->read_mercury(data, 0);
            if (rs) {
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
                }
                if (debug > 2) ULOGW("[mer] [Kn=%f|Kt=%f] A=%f (%d)", kn, kt, A, data[1] & 0xf);
                if (Kn > 0 && Kt > 0 && A > 100) {
                    // sprintf (query,"UPDATE dev_mer SET Kn=%f,Kt=%f,A=%f WHERE device=%d",kn,kt,A,this->device);
                    this->A = A;
                    sprintf(query, "UPDATE dev_mer SET A=%f WHERE device=%d", A, this->device);
                    if (debug > 2) ULOGW("[mer] [%s]", query);
                    res = dbase.sqlexec(query);
                    if (res) mysql_free_result(res);
                }
            }
        }
        return 1;
    } else {
        if (rs > 10) sprintf(query, "UPDATE device SET lastdate=NULL,conn=2 WHERE id=%d", this->iddev);
        else sprintf(query, "UPDATE device SET lastdate=NULL,conn=0 WHERE id=%d", this->iddev);
        if (debug > 2) ULOGW("[mer] [%s]", query);
        this->qerrors++;
        //if (rs>10)
        if (this->qerrors > 10) {
            res = dbase.sqlexec(query);
            if (res) mysql_free_result(res);
            this->qerrors = 0;
        }
        return 0;
    }
}

//-----------------------------------------------------------------------------
// ReadDataArchive - read single device. Readed data will be stored in DB
int DeviceMER::ReadAllArchive(uint16_t tp) {
    UINT rs;
    BYTE data[400], data2[100];
    CHAR date[20];
    UINT month, year, index, chan;
    float fl, fl2, fl3, fl4;
    UINT code, vsk = 0, adr = 0, j = 0, f = 0;
    time_t tims, tim;
    tims = time(&tims);
    struct tm tt;

    this->qatt++;  // attempt

    if (tp == 99) {
        //if (this->device>10006)
        if (this->type == TYPE_MERCURY230) {
            rs = send_mercury(READ_PARAMETRS, 0x13, 0xff, 0);
            if (rs) rs = this->read_mercury(data, 0);
            if (rs) {
                //03-30 14:07:10 [mer] READ [24][0x1,0x15,0x1,0x2,0x19,0x2c,0x0,0x0,0x0,0x40,0x1e,0xc,0x17,0x1,0x14,0x0,0x30,0x3,0x15][crc][0x27,0x4f]
                //03-30 14:07:10 [set][hh][17:12 01-14-2000] [c 17]
                if (debug > 2)
                    ULOGW("[mer][hh][%02d:%02d %02d-%02d-%d] [%x %x]", BCD(data[3]), BCD(data[4] & 0x7f), BCD(data[5]),
                          BCD(data[6]), 2000 + BCD(data[7]), data[0], data[1]);
                //if (debug>2) ULOGW ("[set][hh][---][%d]",this->device);
                if (BCD(data[3]) >= 0 && BCD(data[3]) < 24)
                    if (BCD(data[5]) > 0 && BCD(data[5]) < 32)
                        if (BCD(data[6]) > 0 && BCD(data[6]) < 13) {
                            //if (debug>2) ULOGW ("[set][hh][+++]");
                            adr = data[0] * 0x100 + data[1];
                            if (BCD(data[4] & 0x7f) == 30 && adr >= 0x3) if (this->device <= 10006) adr -= 0x3;
                            if (BCD(data[4] & 0x7f) == 0 && adr >= 0x2) if (this->device <= 10006) adr -= 0x2;
                            if (this->device <= 10006) adr *= 0x10;
                            vsk = 250;
                            //if (this->device==10014 || this->device==10015) vsk=800;
                            while (vsk) {
                                if (debug > 2) ULOGW("[mer][hh][%d][%d]", vsk, adr);
                                rs = send_mercury(READ_POWER, 0x3, adr, 2 * 0xf);
                                if (rs) rs = this->read_mercury(data, 0);
                                if (rs) {
                                    //for (f=0; f<rs;f++) ULOGW ("[mer][%d(%d)][%d][%x]",j,rs,f,data[f]);
                                    if (BCD(data[1]) >= 0 && BCD(data[1]) < 24 && BCD(data[3]) > 0 &&
                                        BCD(data[3]) < 32 && BCD(data[4]) > 0 && BCD(data[4]) < 13) {
                                        fl = (float) (data[8] * 256) + (float) (data[7]);
                                        fl2 = (float) (data[23] * 256) + (float) (data[22]);
                                        fl3 = (float) (data[10] * 256) + (float) (data[9]);
                                        fl4 = (float) (data[25] * 256) + (float) (data[24]);
                                        fl = (fl + fl2) / 2;
                                        fl3 = (fl3 + fl4) / 2;
                                        if (this->rasknt > 0) {
                                            fl *= this->rasknt;
                                            if (this->A > 0) fl = fl / (this->A);
                                            fl3 *= this->rasknt;
                                            if (this->A > 0) fl3 = fl3 / (this->A);
                                            //if (this->knt) fl*=this->knt;
                                        }
                                        sprintf(this->lastdate, "%04d%02d%02d%02d0000", 2000 + BCD(data[5]),
                                                BCD(data[4]), BCD(data[3]), BCD(data[1]));
                                        if (debug > 2)
                                            ULOGW("[mer] T [%d][%d:00 %02d-%02d-%d] (%f)", j, BCD(data[1]),
                                                  BCD(data[3]), BCD(data[4]), BCD(data[5]), fl);
                                        //if (debug>2) ULOGW("[mer] P+[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] (%d)",data[7]&0x7f,data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[8]*256+data[7]);
                                        chan = GetChannelNum(dbase, 14, 0, this->device);
                                        if (fl > 0)
                                            StoreData(dbase, this->device, 14, 0, 1, 0, fl, this->lastdate, 0, chan);
                                        chan = GetChannelNum(dbase, 14, 3, this->device);
                                        if (fl > 0 && chan > 0)
                                            StoreData(dbase, this->device, 14, 3, 1, 0, fl3, this->lastdate, 0, chan);
                                        //break;
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
            if (rs) rs = this->read_mercury(data, 0);
            if (rs) {
                if (debug > 2)
                    ULOGW("[mer][hh][%02d:%02d %02d-%02d-%d] [%x %x]", BCD(data[1]), BCD(data[0] & 0x7f), BCD(data[2]),
                          BCD(data[3]), 2000 + BCD(data[4]), data[5], data[6]);
                //if (debug>2) ULOGW ("[set][hh][---][%d]",this->device);
                if (BCD(data[1]) >= 0 && BCD(data[1]) < 24)
                    if (BCD(data[2]) > 0 && BCD(data[2]) < 32)
                        if (BCD(data[3]) > 0 && BCD(data[3]) < 13)
                            if (BCD(data[4]) == 15) /// !!!!!
                            {
                                //if (debug>2) ULOGW ("[set][hh][+++]");
                                adr = data[5] * 0x100 + data[6];
                                // rs=send_mercury (READ_PARAMETRS, 0x6, 0xff, 0);
                                // if (rs)  rs = this->read_mercury(data, 0);
                                //if (debug>2) ULOGW ("[set][hh][int_time=%0d]",data[1]);
                                adr -= 0x8;
                                vsk = 8;
                                while (vsk) {
                                    if (debug > 2) ULOGW("[mer][hh][%d][%d]", vsk, adr);
                                    rs = send_mercury(READ_POWER, 0x3, adr, 0x20);
                                    if (rs) rs = this->read_mercury(data, 0);
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
                                                            if (this->rasknt > 0) {
                                                                //if (debug>2) ULOGW("[mer] fl=[%f]",fl);
                                                                fl *= this->rasknt;
                                                                //if (debug>2) ULOGW("[mer] fl=[%f]",fl);
                                                                if (this->A > 0) fl = fl / (this->A);
                                                                //if (debug>2) ULOGW("[mer] fl=[%f]",fl);
                                                                //if (this->knt) fl*=this->knt;
                                                            }
                                                            sprintf(this->lastdate, "%04d%02d%02d%02d0000",
                                                                    2000 + BCD(data2[3]), BCD(data2[2]), BCD(data2[1]),
                                                                    BCD(data2[0]));
                                                            if (debug > 2)
                                                                ULOGW("[mer] T [%d][%d:00 %02d-%02d-%d] (%f) [%x %x]",
                                                                      j, BCD(data2[0]), BCD(data2[1]), BCD(data2[2]),
                                                                      BCD(data2[3]), fl, data2[8], data2[9]);
                                                            //if (debug>2) ULOGW("[mer] P+[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] (%d)",data2[8]&0x7f,data2[9],data2[10],data2[11],data2[12],data2[13],data2[14],data2[15],data2[8]&0x7f*256+data2[9]);
                                                            //if (debug>2) ULOGW("[mer] P+[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] (%d)",data2[16]&0x7f,data2[17],data2[18],data2[19],data2[20],data2[21],data2[22],data2[23],data2[16]&0x7f*256+data2[17]);
                                                            chan = GetChannelNum(dbase, 14, 0, this->device);
                                                            if (fl > 0)
                                                                StoreData(dbase, this->device, 14, 0, 1, 0, fl,
                                                                          this->lastdate, 0, chan);
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

    if (this->type == TYPE_MERCURY230) rs = send_mercury(READ_DATA_230, 0x50, 0x0, 0);
    else rs = send_mercury(READ_DATA_230, 0x50, 0x0, 0);
    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        UpdateThreads(dbase, TYPE_MERCURY230, 1, 1, 14, this->device, 1, 1, (CHAR *) "");
        tim = time(&tim);
        tim -= 3600 * 24;
        localtime_r(&tim, &tt);
        if (this->type == TYPE_SET_4TM)
            fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                  (float) (data[3]));
        else
            fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                  (float) (data[2]));
        // fl=fl/1000;
        if (this->rasknt > 0) {
            fl *= this->rasknt;
            if (this->A > 0) fl = fl / (2 * this->A);
            if (this->knt) fl *= this->knt;
        }
        sprintf(this->lastdate, "%04d%02d%02d000000", tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        if (debug > 2)
            ULOGW("[mer][2][0x%x 0x%x 0x%x 0x%x] [%f][%s][%f]", data[0], data[1], data[2], data[3], fl, this->lastdate,
                  this->knt);
        chan = GetChannelNum(dbase, 14, 0, this->device);
        StoreData(dbase, this->device, 14, 0, 2, 0, fl, this->lastdate, 0, chan);
    }

    //rs=send_mercury (READ_DATA_230, 0x84, 0x0, 0);
    if (this->type == TYPE_MERCURY230) rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);
    else rs = send_mercury(READ_DATA_230, 0x0, 0x0, 0);
    if (rs) rs = this->read_mercury(data, 0);
    if (rs) {
        tim = time(&tim);
        tim -= 3600 * 24;
        localtime_r(&tim, &tt);
        if (this->type == TYPE_SET_4TM)
            fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                  (float) (data[3]));
//            fl=((float)(data[0]*256*256)+(float)(data[1]*256)+(float)(data[2]));
        else
            fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                  (float) (data[2]));
//	 fl=fl/1000;
        if (this->rasknt > 0) {
            fl *= this->rasknt;
            if (this->A > 0) fl = fl / (2 * this->A);
            if (this->knt) fl *= this->knt;
        }
        //fl=((float)(data[0]&0x3f)+(float)data[1]*256+(float)data[2]*256*256+(float)data[3]*256*256*256); fl=fl;
        sprintf(this->lastdate, "%04d%02d%02d000000", tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        if (debug > 2)
            ULOGW("[mer][0x%x 0x%x 0x%x 0x%x] [%f][%s]", data[0], data[1], data[2], data[3], fl, this->lastdate);
        chan = GetChannelNum(dbase, 14, 2, this->device);
        StoreData(dbase, this->device, 14, 2, 2, 0, fl, this->lastdate, 0, chan);
    }
/*
 tim=time(&tim);
 localtime_r(&tim,&tt);
 for (int i=0; i<5; i++)
    {
     if (tt.tm_mday==0) { if (tt.tm_mon>1) { tt.tm_mon--; tt.tm_mday=30; } else break; }
     rs=send_mercury (READ_DATA_230L, 0x6, tt.tm_mday, 0);
     if (rs)  rs = this->read_mercury(data, 0);
     if (rs)
	    {
    	     if (this->type==TYPE_SET_4TM) 
	        fl=((float)(data[0]*256*256*256)+(float)(data[1]*256*256)+(float)(data[2]*256)+(float)(data[3]));
	     else   fl=((float)(data[1]*256*256*256)+(float)(data[0]*256*256)+(float)(data[3]*256)+(float)(data[2]));
	     fl=fl/1000;
	     sprintf (this->lastdate,"%04d%02d%02d000000",tt.tm_year+1900,tt.tm_mon+1,tt.tm_mday); 
	     if (debug>2) ULOGW ("[mer!!][0x%x 0x%x 0x%x 0x%x] [%f] [%s]",data[0],data[1],data[2],data[3],fl,this->lastdate);
	     //UpdateThreads (dbase, TYPE_MERCURY230-1, 1, 1, 14, this->device, 1, 1, this->lastdate);
	     //UpdateThreads (dbase, TYPE_MERCURY230, 1, 1, 14, this->device, 1, 1, (CHAR *)"");
	     //StoreData (dbase, this->device, 14, 0, 4, 0, fl,this->lastdate, 0, chan);
	    }
     tt.tm_mday--;
    }
*/

    tim = time(&tim);
    localtime_r(&tim, &tt);
    for (int i = 0; i < 10; i++) {
        if (tt.tm_mon == 0) {
            tt.tm_mon = 12;
            tt.tm_year--;
        }
        rs = send_mercury(READ_DATA_230, 0x30 + tt.tm_mon, 0x0, 0);
        if (rs) rs = this->read_mercury(data, 0);
        if (rs) {
            if (this->type == TYPE_SET_4TM)
                fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) + (float) (data[2] * 256) +
                      (float) (data[3]));
            else
                fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) + (float) (data[3] * 256) +
                      (float) (data[2]));
            //fl=fl/1000;
            if (this->rasknt > 0) {
                fl *= this->rasknt;
                if (this->A > 0) fl = fl / (2 * this->A);
                if (this->knt) fl *= this->knt;
            }
            sprintf(this->lastdate, "%04d%02d01000000", tt.tm_year + 1900, tt.tm_mon + 1);
            chan = GetChannelNum(dbase, 14, 0, this->device);
            if (debug > 2)
                ULOGW("[mer][4][%d][0x%x 0x%x 0x%x 0x%x] [%f] [%s]", chan, data[0], data[1], data[2], data[3], fl,
                      this->lastdate);
            //UpdateThreads (dbase, TYPE_MERCURY230-1, 1, 1, 14, this->device, 1, 1, this->lastdate);
            //UpdateThreads (dbase, TYPE_MERCURY230, 1, 1, 14, this->device, 1, 1, (CHAR *)"");
            StoreData(dbase, this->device, 14, 0, 4, 0, fl, this->lastdate, 0, chan);
        }
        if (1)
//     if (this->type!=TYPE_SET_4TM) 
        {
            rs = send_mercury(READ_DATA_230, 0x00 + tt.tm_mon, 0x0, 0);
            if (rs) rs = this->read_mercury(data, 0);
            if (rs) {
                if (this->type == TYPE_SET_4TM)
                    fl = ((float) (data[0] * 256 * 256 * 256) + (float) (data[1] * 256 * 256) +
                          (float) (data[2] * 256) + (float) (data[3]));
                else
                    fl = ((float) (data[1] * 256 * 256 * 256) + (float) (data[0] * 256 * 256) +
                          (float) (data[3] * 256) + (float) (data[2]));
                //fl=fl/1000;
                if (this->rasknt > 0) {
                    fl *= this->rasknt;
                    if (this->A > 0) fl = fl / (2 * this->A);
                    if (this->knt) fl *= this->knt;
                }
                sprintf(this->lastdate, "%04d%02d01000000", tt.tm_year + 1900, tt.tm_mon + 1);
                chan = GetChannelNum(dbase, 14, 2, this->device);
                if (debug > 2)
                    ULOGW("[mer][4][%d][0x%x 0x%x 0x%x 0x%x] [%f] [%s]", chan, data[0], data[1], data[2], data[3], fl,
                          this->lastdate);
                StoreData(dbase, this->device, 14, 2, 4, 0, fl, this->lastdate, 0, chan);
            }
        }
        tt.tm_mon--;
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool DeviceMER::send_mercury(u_int8_t op, u_int8_t prm, u_int8_t frame, u_int8_t index)
    {
     UINT       crc=0;          //(* CRC checksum *)
     UINT       nbytes = 0;     //(* number of bytes in send packet *)
     BYTE       data[100];      //(* send sequence *)
     UINT	ht=0,len=0,nr=0;
     CHAR	path[100]={0};

     if (op==0x1) // open/close session
	{
         if (this->type==TYPE_MERCURY230) len=11;
         if (this->type==TYPE_SET_4TM) len=10;
	}
     if (op==0x4) // time
	len=5;
     if (op==0x5 || op==0x8 || op==0x11 || op==0x14) // nak
	{
         if (index<0xff)  len=6;
	 else len=5;
	 if (frame==5) len=7;
	}
     if (op==0x6)
	len=8;

     if (this->protocol==13)
	{
	 for (ht=0;ht<8;ht++)
	    if (!this->adrr[ht]) break;
        else nr++;

	 data[0]=1;	// redunancy
	 data[1]=6+3*nr+len;
	 data[2]=16+(nr-1);
	 data[3]=0; data[4]=0; data[5]=0;	// comp address
	 for (ht=0;ht<nr;ht++)
	     {
	      data[6+ht*3]=this->adrr[ht]/0x10000; data[7+ht*3]=(this->adrr[ht]&0xff00)/0x100; data[8+ht*3]=this->adrr[ht]&0xff;	// PPL-N address
	      sprintf (path,"%s[%d.%d.%d]",path,data[6+ht*3],data[7+ht*3],data[8+ht*3]);
	     }
	 if (frame==0 || frame==5 || op==6) data[6+nr*3]=0x40;
	    else data[6+nr*3]=0xf0;			// commands
	 ht=7+nr*3;
    	 if (debug>3) ULOGW("[mer] [ht](%d) open channel [%s] wr[0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x]",ht,path,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12]);
	}
     data[ht+0]=this->adr&0xff; 
     data[ht+1]=op;
     
     if (op==0x1 || op==0x2) // open/close session
        {
	 if (this->type==TYPE_MERCURY230)
		{
		 if (prm==0)
		    {
         		 data[ht+2]=2;
		     	 data[ht+3]=2; data[ht+4]=2; data[ht+5]=2; data[ht+6]=2; data[ht+7]=2; data[ht+8]=2;
		    }
		 else
		    {
         	      data[ht+2]=1; data[ht+3]=1; data[ht+4]=1; data[ht+5]=1; data[ht+6]=1; data[ht+7]=1; data[ht+8]=1;
		    }
	         crc=CRC (data+ht, 9, 0);
    		 data[ht+9]=crc/256;
		 data[ht+10]=crc%256;
        	 if (debug>2) ULOGW("[mer][%d][%s] open channel [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",this->device,path,op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10]);

    		 if (this->protocol==13)
		    {
		     crc=Crc16 (data+1, ht+10);
        	     data[ht+11]=crc%256;
		     data[ht+12]=crc/256;
		     //for (len=0; len<=ht+12;len++) ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
		     //if (debug>2) ULOGW("[mer] wr crc [0x%x,0x%x] [%d]",data[ht+11],data[ht+12],ht);
		     ht+=2;
		    }
	         //if (debug>2) ULOGW("[mer] wr [%d]",11+ht);
        	 write (fd,&data,11+ht);	 
		}
	 if (this->type==TYPE_SET_4TM)
		{
	     	 data[ht+2]=0x30; data[ht+3]=0x30; data[ht+4]=0x30; data[ht+5]=0x30; data[ht+6]=0x30; data[ht+7]=0x30;
	         crc=CRC (data+ht, 8, 0);
    		 data[ht+8]=crc/256;
		 data[ht+9]=crc%256;
        	 if (debug>2) ULOGW("[set] open channel [%s] [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",path,op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10]);
    		 if (this->protocol==13)
		    {
		     crc=Crc16 (data+1, 9+ht);
        	     data[ht+10]=crc%256;
		     data[ht+11]=crc/256;
		     //for (len=0; len<=ht+11;len++)   ULOGW("[set] %d=%x",len,data[len]);
        	     if (debug>3) ULOGW("[set] wr crc [0x%x,0x%x]",data[ht+11],data[ht+12]);
		     ht+=2;
		    }
        	 write (fd,&data,10+ht);
		}
        }     
     if (op==0x4) // time
        {
	 data[ht+2]=0x0;
	 crc=CRC (data+ht, 3, 0);
         data[ht+3]=crc/256;
	 data[ht+4]=crc%256;
         if (debug>2) ULOGW("[mer] read time [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4]);
	 if (this->protocol==13)
	    {
	     crc=Crc16 (data+1, 4+ht);
    	     data[ht+5]=crc%256;
	     data[ht+6]=crc/256;
    	     //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);
	     //for (len=0; len<=ht+6;len++) ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
	     ht+=2;
	    }
         write (fd,&data,5+ht);
	}

     if (frame!=5)
     if (op==0x5 || op==0x8 || op==0x11 || op==0x14) // nak
        {
	 data[ht+2]=prm;
	 if (index<0xff) 
	    {
	     data[ht+3]=index;
	     crc=CRC (data+ht, 4, 0);
             data[ht+4]=crc/256;
	     data[ht+5]=crc%256;
             if (debug>2) ULOGW("[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
	     if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 5+ht);
    	         data[ht+6]=crc%256;
	         data[ht+7]=crc/256;
    	         //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
    	         //for (len=0; len<=ht+7;len++)   ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
		 ht+=2;
		}
             write (fd,&data,6+ht);
	    }
	 else
	    {
	     data[ht+2]=prm;
	     crc=CRC (data+ht, 3, 0);
             data[ht+3]=crc/256;
	     data[ht+4]=crc%256;
             if (debug>2) ULOGW("[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4]);
	     if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 4+ht);
    	         data[ht+5]=crc%256;
	         data[ht+6]=crc/256;
    	         //for (len=0; len<=ht+6;len++)		    ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
    	         //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);
		 ht+=2;
		}
             write (fd,&data,5+ht);
	    }
	}

     if (op==0x6)
        {
	 data[ht+2]=prm;
         data[ht+3]=index/256;
         data[ht+4]=index%256;
         data[ht+5]=frame;
         crc=CRC (data+ht, 6, 0);
         data[ht+6]=crc/256;
         data[ht+7]=crc%256;
         if (debug>2) ULOGW("[mer] read parametrs!! [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 7+ht);
    	         data[ht+8]=crc%256;
	         data[ht+9]=crc/256;
    	         //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
    	         //for (len=0; len<=ht+9;len++)   ULOGW("[mer-s] %d=%x(%d)",len,data[len],data[len]);
		 ht+=2;
		}
         write (fd,&data,8+ht);
	}

     if (op==READ_DATA_230L)
        {
	 data[ht+2]=prm;
         data[ht+3]=index;
         data[ht+4]=0;
         data[ht+5]=1;
         data[ht+6]=0;
         crc=CRC (data+ht, 7, 0);
         data[ht+7]=crc/256;
         data[ht+8]=crc%256;
         if (debug>2) ULOGW("[mer] read long [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 8+ht);
    	         data[ht+9]=crc%256;
	         data[ht+10]=crc/256;
    	         //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+6],data[ht+7]);
    	         //for (len=0; len<=ht+7;len++)   ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
		 ht+=2;
		}
         write (fd,&data,9+ht);
	}

     if (frame==5)
     if (op==0x5 || op==0x8 || op==0x11 || op==0x14) // nak
        {
	 data[ht+2]=prm;
         data[ht+3]=1;
         data[ht+4]=index;
         crc=CRC (data+ht, 5, 0);
	 data[ht+5]=crc/256;
	 data[ht+6]=crc%256;
         if (debug>2) ULOGW("[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6]);
	 if (this->protocol==13)
	    {
	     crc=Crc16 (data+1, 6+ht);
    	     data[ht+7]=crc%256;
	     data[ht+8]=crc/256;
    	     //for (len=0; len<=ht+8;len++)		    ULOGW("[mer] %d=%x(%d)",len,data[len],data[len]);
    	     //if (debug>3) ULOGW("[mer] wr crc [0x%x,0x%x]",data[ht+5],data[ht+6]);

	     ht+=2;
	    }
         write (fd,&data,7+ht);
        }

     if (0 && op==0x8) // parametrs
        {
	 data[ht+2]=prm;
	 crc=CRC (data, 3, 0);
         data[ht+3]=crc/256;
	 data[ht+4]=crc%256;
         if (debug>2) ULOGW("[mer] read parametrs [%d][%d] wr[0x%x,0x%x,0x%x,0x%x,0x%x]",op,prm,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4]);
         write (fd,&data,5);
	}
     return true;     
    }

//-----------------------------------------------------------------------------    
u_int16_t DeviceMER::read_mercury(u_int8_t *dat, u_int8_t type)
    {
     UINT       crc=0;		//(* CRC checksum *)
     INT        nbytes = 0;     //(* number of bytes in recieve packet *)
     INT        bytes = 0;      //(* number of bytes in packet *)
     uint8_t       data[500];      //(* recieve sequence *)
     UINT       i=0;            //(* current position *)
     CHAR       op=0;           //(* operation *)
     UINT       crc_in=0;	//(* CRC checksum *)

//     sleep(1);
     usleep (500000);
     ioctl (fd,FIONREAD,&nbytes); 
     nbytes=read (fd, &data, 75);
     usleep (200000);
     ioctl (fd,FIONREAD,&bytes);  
     
     if (bytes>0 && nbytes>0 && nbytes<50) 
        {
         //if (debug>2) ULOGW("[set] bytes=%d fd=%d adr=%d",bytes,fd,&data+nbytes);
         bytes=read (fd, &data+nbytes, bytes);
         //if (debug>2) ULOGW("[set] bytes=%d",bytes);
         nbytes+=bytes;
         //for (i=0; i<nbytes;i++)	    ULOGW("[set] %d=%x",i,data[i]);
        }

     if (nbytes>=4)
        {
	 if (this->protocol==14)
		 crc=CRC (data+1, nbytes-3, 0);
	 else	 crc=Crc16 (data+1, nbytes-3);
         if (debug>2) ULOGW("[mer] READ [%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x][crc][0x%x,0x%x]",nbytes,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15],data[16],data[17],data[18],crc/256,crc%256);  
	 //for (UINT rr=0; rr<nbytes;rr++) if (debug>2) ULOGW("[mer] [%d][0x%x]",rr,data[rr]);
	 if (this->protocol==14)
		crc_in=data[nbytes-2]*256+data[nbytes-1];
	 else	crc_in=data[nbytes-2]+data[nbytes-1]*256;
	 //if (debug>2) ULOGW("[mer] [%d]c[0x%x,0x%x]i[0x%x,0x%x] [%x %x]",nbytes,crc%256,crc/256,data[nbytes-2],data[nbytes-1],crc,crc_in);
	 if (this->protocol==13 && (crc!=crc_in || nbytes<3))
		nbytes=0;
	 else
	    {
	     if (this->protocol==14)
        	memcpy (dat,data+1,nbytes-3);
	     if (this->protocol==13)
		{
	         if (nbytes>16)
		    {
		     if (nbytes>50) // memory
        	        memcpy (dat,data+10,nbytes-10);
		     else
        	    	memcpy (dat,data+11,nbytes-11);
	             return nbytes;
		    }
	         else 
		    {
		     if (nbytes==12 && (data[9]==0xf0 || data[9]==0x40))
			{
			 ULOGW("[mer] HTC answer: no counter answer present [%d]",data[9]);
			 Events (dbase, 2, this->device, 2, 0, 2, 0);
			}
		     if (nbytes==16 && (data[11]&0xf==0x5))
			 ULOGW("[mer] channel not open correctly [%d]",data[11]);
		     memcpy (dat,data+11,nbytes-11);
		     //nbytes=0;
		    }
		}
	    }
         return nbytes;
        }
     else
	{
	 //Events (dbase, 1, this->device, 1, 0, 3, 0);
	}
     return 0;
    }

//-----------------------------------------------------------------------------
BOOL DeviceMER::send_ce (UINT op, UINT prm, CHAR* request, UINT frame)
    {
     UINT       crc=0;          //(* CRC checksum *)
     UINT       ht=0,nr=0,len=0,nbytes = 0;     //(* number of bytes in send packet *)
     BYTE      	data[100];      //(* send sequence *)
     CHAR	path[100]={0};
     CHAR	adr[10]={0};

     if (op==SN) // open/close session
	    len=8;
     if (op==0x4) // time
    	    len=13;
     if (op==OPEN_PREV)
	    len=6;
     if (op==OPEN_CHANNEL)
	    len=14;
     if (op==0x88)
	    len=18;
     if (op==0x89)
	    len=5;

     if (op==READ_DATE || op==READ_TIME || op==READ_PARAMETERS)
	    len=13;
     if (op==ARCH_MONTH || op==ARCH_DAYS)
	    len=strlen(request)+6;

     if (this->protocol==13)
	{	 
	 for (ht=0;ht<8;ht++)
	    if (!this->adrr[ht]) break;
	    else nr++;

	 data[0]=1;	// redunancy
	 data[1]=6+3*nr+len;
	 data[2]=16+(nr-1);
	 data[3]=0; data[4]=0; data[5]=0;	// comp address
	 for (ht=0;ht<nr;ht++)
	     {
	      data[6+ht*3]=this->adrr[ht]/0x10000; data[7+ht*3]=(this->adrr[ht]&0xff00)/0x100; data[8+ht*3]=this->adrr[ht]&0xff;	// PPL-N address
	      sprintf (path,"%s[%d.%d.%d]",path,data[6+ht*3],data[7+ht*3],data[8+ht*3]);
	     }
	 data[6+nr*3]=0x40;
	 //data[6+nr*3]=0xf0;			// commands
	 ht=7+nr*3;
    	 if (debug>2) ULOGW("[303] [ht](%d) open channel [%s] wr[0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x, 0x%x]",ht,path,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12]);
	}

//     data[ht+0]=this->adr&0xff; 
//     data[ht+1]=op;

     if (op==SN)
        {
         data[ht+0]=START;
	 data[ht+1]=REQUEST;
	 sprintf (adr,"%d",prm);
	 data[ht+2]=adr[0];
	 data[ht+3]=adr[1];
	 data[ht+4]=adr[2];
	 data[ht+5]=0x21;
	 data[ht+6]=CR;
	 data[ht+7]=LF;
         //if (debug>2) ULOGW("[303][SN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] [0x%x][0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8]);
	 for (len=0; len<=ht+7;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][SN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x] [0x%x][0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 7+ht);
    	         data[ht+8]=crc%256;
	         data[ht+9]=crc/256;
		}
         for (len=0; len<=ht+9;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,10+ht);
	}

     if (op==OPEN_PREV)
        {
         data[ht+0]=0x6;
	 data[ht+1]=0x30;
	 data[ht+2]=0x35;
	 data[ht+3]=0x31;
	 data[ht+4]=CR;
	 data[ht+5]=LF;
         //if (debug>2) ULOGW("[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
	 for (len=0; len<=5;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 5+ht);
    	         data[ht+6]=crc%256;
	         data[ht+7]=crc/256;
		}
         for (len=0; len<ht+8;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,8+ht);
	}         

     if (op==0x88)
        {
         data[ht+0]=0x1;
	 data[ht+1]=0x52;
	 data[ht+2]=0x31;
	 data[ht+3]=STX;
	 data[ht+4]=0x45; // ENMPE
	 data[ht+5]=0x4E;
	 data[ht+6]=0x4D;
	 data[ht+7]=0x50;
	 data[ht+8]=0x45;
	 data[ht+9]=0x28;
	 data[ht+10]=0x31;
	 data[ht+11]=0x32;
	 data[ht+12]=0x2E;
	 data[ht+13]=0x31;
	 data[ht+14]=0x34;
	 data[ht+15]=0x29;
	 data[ht+16]=ETX;
	 data[ht+17]=CRC_CE (data+ht+1, 16, 1);
         //if (debug>2) ULOGW("[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
	 for (len=0; len<ht+18;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][SNUM] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 17+ht);
    	         data[ht+18]=crc%256;
	         data[ht+19]=crc/256;
		}
         for (len=0; len<ht+20;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,20+ht);
	}         

     if (op==0x89 && 0)
        {
         data[ht+0]=0x1;
	 data[ht+1]=0x42;
	 data[ht+2]=0x30;
	 data[ht+3]=0x3;
	 data[ht+4]=0x75;
	 for (len=0; len<5;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][HZ] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5]);
         if (this->protocol==13)
		{
	         crc=Crc16 (data+1, 4+ht);
    	         data[ht+5]=crc%256;
	         data[ht+6]=crc/256;
		}
         for (len=0; len<ht+7;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,7+ht);
	}         

     if (op==OPEN_CHANNEL_CE)
        {
         data[ht+0]=0x1;
	 data[ht+1]=0x50;
	 data[ht+2]=0x31;
	 data[ht+3]=STX;
	 data[ht+4]=0x28;
	 // default password
	 if (prm==0)
	    {
    	     data[ht+5]=0x37; data[ht+6]=0x37; data[ht+7]=0x37;
	     data[ht+8]=0x37; data[ht+9]=0x37; data[ht+10]=0x37;
	    }
	 else
	    {
    	     data[ht+5]=0x31; data[ht+6]=0x31; data[ht+7]=0x31;
	     data[ht+8]=0x31; data[ht+9]=0x31; data[ht+10]=0x31;
	    }
	 // true password
	 //memcpy (data+ht+5,this->pass,6); !!!!!
	 data[ht+11]=0x29;
	 data[ht+12]=ETX;
         data[ht+13]=CRC_CE (data+ht+1, 12, 1);
	 for (len=0; len<=13;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][OPEN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10],data[ht+11],data[ht+12],data[ht+13]);
         crc=Crc16 (data+1, 13+ht);
         data[ht+14]=crc%256;
         data[ht+15]=crc/256;
         for (len=0; len<ht+16;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,ht+16);
	}         

     if (op==READ_DATE || op==READ_TIME)
        {
         data[ht+0]=0x1;
	 data[ht+1]=0x52; data[ht+2]=0x31; data[ht+3]=STX;
	 if (op==READ_DATE) sprintf ((char *)data+ht+4,"DATE_()");
	 if (op==READ_TIME) sprintf ((char *)data+ht+4,"TIME_()");
	 data[ht+11]=ETX;
         data[ht+12]=CRC_CE (data+ht+1, 11, 1);
	 for (len=0; len<=ht+12;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][DATE|TIME] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10],data[ht+11],data[ht+12]);
         crc=Crc16 (data+1, 12+ht);
         data[ht+13]=crc%256;
         data[ht+14]=crc/256;
         for (len=0; len<ht+15;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,ht+15);
	}         
     if (op==READ_PARAMETERS)
        {
	 data[ht+0]=0x1;
	 data[ht+1]=0x52; data[ht+2]=0x31; data[ht+3]=STX;

	 if (prm==CURRENT_W) sprintf ((char *)data+ht+4,"POWPP()");
	 //if (op==CURRENT_I) sprintf (data+4,"CURRE()");
	 if (prm==CURRENT_I) sprintf ((char *)data+ht+4,"ET0PE()");
	 if (prm==CURRENT_F) sprintf ((char *)data+ht+4,"FREQU()");
	 if (prm==CURRENT_U) sprintf ((char *)data+ht+4,"VOLTA()");
	 if (prm==30) sprintf ((char *)data+ht+4,"CONDI()");
	 if (prm==31) sprintf ((char *)data+ht+4,"SNUMB()");
	 if (prm==32) sprintf ((char *)data+ht+4,"CURRE()");
	 if (prm==33) sprintf ((char *)data+ht+4,"VOLTA()");

	 data[ht+11]=ETX;
         data[ht+12]=CRC_CE (data+ht+1, 11, 1);
	 for (len=0; len<=12;len++) data[ht+len]=Ct(data[ht+len]);

         if (debug>2) ULOGW("[303][CURR] wr[%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",13,data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10],data[ht+11],data[ht+12]);
         crc=Crc16 (data+1, 12+ht);
         data[ht+13]=crc%256;
         data[ht+14]=crc/256;
         for (len=0; len<ht+15;len++)   ULOGW("[303] %d=%x(%c)",len,data[len]&0x7f,data[len]&0x7f);
         write (fd,&data,ht+15);
	}         
     if (op==ARCH_MONTH || op==ARCH_DAYS)
        {
         data[ht+0]=0x1;
	 data[ht+1]=0x52; data[ht+2]=0x31; data[ht+3]=STX;
	 sprintf ((char *)data+ht+4,"%s",request);
	 len=strlen(request)+3;
	 data[ht+len+1]=ETX;
         data[ht+len+2]=CRC_CE (data+ht, len+2, 1);
         //crc=CRC (data+ht, 7, 0);
	 for (len=0; len<=ht+len+2;len++) data[ht+len]=Ct(data[ht+len]);
         if (debug>2) ULOGW("[303][ARCH] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",data[ht+0],data[ht+1],data[ht+2],data[ht+3],data[ht+4],data[ht+5],data[ht+6],data[ht+7],data[ht+8],data[ht+9],data[ht+10],data[ht+11],data[ht+12],data[ht+13],data[ht+14],data[ht+15],data[ht+16],data[ht+17]);
         crc=Crc16 (data+1, len+2+ht);
         data[ht+len+3]=crc%256;
         data[ht+len+4]=crc/256;
         write (fd,&data,ht+len+5);
	}         

     return true;     
    }
//-----------------------------------------------------------------------------    
UINT  DeviceMER::read_ce (BYTE* dat, BYTE type)
    {
     UINT       crc_in=0,crc=0;		//(* CRC checksum *)
     INT        nbytes = 0;     //(* number of bytes in recieve packet *)
     INT        bytes = 0;      //(* number of bytes in packet *)
     BYTE       data[500];      //(* recieve sequence *)
     BYTE       data2[500];      //(* recieve sequence *)
     UINT       i=0;            //(* current position *)
     UCHAR      ok=0xFF;        //(* flajochek *)
     CHAR       op=0;           //(* operation *)
     UINT rr=0;

     usleep (700000);
     ioctl (fd,FIONREAD,&nbytes); 
     if (debug>2) ULOGW("[303] nbytes=%d",nbytes);
     nbytes=read (fd, &data, 100);
//     if (debug>2) ULOGW("[303] nbytes=%d",nbytes);
     //if (nbytes>0)for (rr=0; rr<nbytes;rr++) if (debug>2) ULOGW("[ce] [%d][0x%x] (%c)",rr,data[rr]&0x7f,data[rr]&0x7f);

     //data[0]=0x2;
     //sprintf (data+1,"POWEP(%.5f)",8.14561);
     //data[13]=0x3; data[14]=CRC (data+1,13,1); nbytes=15;
     //if (debug>2) ULOGW("[303] nbytes=%d %x",nbytes,data[0]);
     //tcsetattr (fd,TCSAFLUSH,&tio);
     //usleep (200000);
     //ioctl (fd,FIONREAD,&bytes);
     //if (debug>2) ULOGW("[303] bytes=%d",bytes);
     //bytes=read (fd, &data+nbytes, 125);
     //if (debug>2) ULOGW("[303] bytes=%d",nbytes);

     if (nbytes==1)
	{
         if (debug>2) ULOGW("[ce] [%d][0x%x]",nbytes,data[0]);
         dat[0]=data[0];
         return nbytes;
        }
     if (nbytes>5)
        {
//	 crc=CRC (data+1, nbytes-2, 1);
	 //crc=CRC (data+1, nbytes-3, 0);
	 crc=Crc16 (data+1, nbytes-3);
	 crc_in=data[nbytes-2]+data[nbytes-1]*256;
    
         if (debug>2) ULOGW("[ce] [%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x][crc][0x%x,0x%x]",nbytes,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15],data[16],data[17],data[nbytes-1],crc);
	 for (UINT rr=0; rr<nbytes;rr++) data[rr]=data[rr]&0x7f;
	 for (UINT rr=0; rr<nbytes;rr++) if (debug>2) ULOGW("[ce] [%d][0x%x] (%c)",rr,data[rr]&0x7f,data[rr]&0x7f);
	// if (data[nbytes-1]!=0xa && data[nbytes-2]!=0xd)
    	//    if (crc!=crc_in) nbytes=0;

	 if (nbytes<100 && nbytes>7)
	    {
             if (nbytes>16)
        	    memcpy (dat,data+11,nbytes-11);
             else 
		    {
		     if (nbytes==12 && (data[9]==0xf0 || data[9]==0x40))
			{
			 ULOGW("[303] HTC answer: no counter answer present [%d]",data[9]);
			 Events (dbase, 2, this->device, 2, 0, 2, 0);
			}
		     if (nbytes==16 && (data[11]&0xf==0x5))
			 ULOGW("[303] channel not open correctly [%d]",data[11]);
		     memcpy (dat,data+11,nbytes-11);
		    }

             //memcpy (dat,data+1,nbytes-3);
             //dat[nbytes-3]=0;
            }
         else dat[0]=0;
         return nbytes;
        }
     return 0;
    }


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

}
