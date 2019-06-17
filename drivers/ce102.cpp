//-----------------------------------------------------------------------------
#include <fcntl.h>
#include <sys/termios.h>
#include <pthread.h>
#include <cstdio>
#include <mysql/mysql.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/ioctl.h>
#include <zconf.h>
#include "errors.h"
#include "main.h"
#include "dbase.h"
#include "TypeThread.h"
#include "kernel.h"
#include "function.h"
#include "ce102.h"

//-----------------------------------------------------------------------------
static MYSQL_RES *res;
static MYSQL_ROW row;
static char query[500];
static int fd = 0;
bool rs = true;

DeviceCE deviceCE;
static DBase dBase;
auto &currentKernelInstance = Kernel::Instance();

bool OpenCom(char *block, uint16_t speed, uint16_t parity);

uint8_t CRC(const uint8_t *Data, const uint8_t DataSize);
uint16_t Crc16(uint8_t *Data, uint8_t DataSize);

//-----------------------------------------------------------------------------
void *ceDeviceThread(void *pth) {
    TypeThread thread = *((TypeThread *) pth);
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] mercury device thread started");
    if (dBase.openConnection() == OK) {
        while (true) {
            sprintf(query, "SELECT * FROM device WHERE thread=%d", thread.id);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] (%s)", query);
            res = dBase.sqlexec(query);
            u_long nRow = mysql_num_rows(res);
            for (u_long r = 0; r < nRow; r++) {
                row = mysql_fetch_row(res);
                if (row) {
                    strncpy(deviceCE.uuid, row[1], 40);
                    strncpy(deviceCE.address, row[3], 10);
                    strncpy(deviceCE.port, row[10], 20);
                }
                if (!fd) {
                    rs = OpenCom(deviceCE.port, thread.speed, 1);
                    if (rs) {
                        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadInfo (%s)", deviceCE.address);
                        UpdateThreads(dBase, thread.id, 0, 1);
                        rs = deviceCE.ReadInfoCE();
                        if (rs) {
                            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataCurrent (%d)", deviceCE.id);
                            UpdateThreads(dBase, thread.id, 0, 1);
                            deviceCE.ReadDataCurrentCE();
                            if (currentKernelInstance.current_time->tm_min > 5) {
                                UpdateThreads(dBase, thread.id, 1, 1);
                                currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] ReadDataArchive (%d)",
                                                                deviceCE.id);
                                deviceCE.ReadAllArchiveCE(5);
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


//-----------------------------------------------------------------------------
bool DeviceCE::ReadInfoCE() {
    u_int16_t res=0, serial, soft;
    bool rs;
    uint8_t data[500];
    char date[20] = {0};
    unsigned char time[20] = {0};

    rs = send_ce(SN, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (res)
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [serial=%s]", data);

    rs = send_ce(OPEN_PREV, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (res)
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [open channel prev: %d]",data[0]);

    rs = send_ce(WATCH, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (res)
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [watch: %s]",data);

//    rs = send_ce(OPEN_CHANNEL_CE, 0, date, 0);
//    if (rs) res = this->read_ce(data, 0);
//    if (res)
//        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [open channel answer: %d]", data[0]);

    rs = send_ce(READ_DATE, 0, date, 0);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        memcpy(date, data + 9, 8);
        rs = send_ce(READ_TIME, 0, date, 0);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            memcpy(time, data + 6, 8);
            sprintf((char *) data, "%s %s", date, time);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [date=%s]", data);
            sprintf(this->dev_time, "20%c%c%c%c%c%c%c%c%c%c%c%c", data[6], data[7], data[3], data[4], data[0], data[1],
                    data[9], data[10], data[12], data[13], data[15], data[16]);
            sprintf(query, "UPDATE device SET last_date=NULL,dev_time=%s WHERE id=%d", this->dev_time, this->id);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "%s", query);
            dBase.sqlexec(query);
            //if (res) mysql_free_result(res);
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
int DeviceCE::ReadDataCurrentCE() {
    uint16_t res=0;
    bool rs;
    char chan[40];
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
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_W), 1, this->uuid), 40);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s]", chan);
	    if (strlen(chan)>0)
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        }
    }
    rs = send_ce(CURRENT_U, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "VOLTA(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] V=[%f]", param, fl);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_U), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        }
    }
    rs = send_ce(CURRENT_F, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "FREQU(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] F=[%f]", param, fl);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_F), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        }
    }
    rs = send_ce(CURRENT_I, 0, date, READ_PARAMETERS);
    if (rs) res = this->read_ce(data, 0);
    if (res) {
        res = static_cast<uint16_t>(sscanf((const char *) data, "ET0PE(%s)", param));
        if (rs) {
            fl = static_cast<float>(atof(param));
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][%s] F=[%f]", param, fl);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(MEASURE_ENERGY), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            dBase.StoreData(TYPE_CURRENTS, 0, fl, nullptr, chan);
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
int DeviceCE::ReadAllArchiveCE(uint16_t tp) {
    bool rs;
    char chan[40];
    uint16_t res=0;
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
        sprintf(date, "EAMPE(%02d.%02d)", tt->tm_mon + 1, tt->tm_year - 100);        // ddMMGGtt
        rs = send_ce(ARCH_MONTH, 0, date, 1);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            rs = static_cast<bool>(sscanf((const char *) data, "EAMPE(%s)", param));
            fl = static_cast<float>(atof(param));
            sprintf(date, "%04d%02d01000000", tt->tm_year + 1900, tt->tm_mon);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                            data[0], data[1], data[2], data[3], fl, date);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_W), 1, this->uuid), 40);
	    if (strlen(chan)>0)
    	        dBase.StoreData(TYPE_MONTH, 0, fl, date, chan);
        }

        sprintf(date, "ENMPE(%02d.%02d)", tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_MONTH, 0, date, 1);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            res = static_cast<uint16_t>(sscanf((const char *) data, "ENMPE(%s)", param));
            fl = static_cast<float>(atof(param));
            sprintf(date, "%04d%02d01000000", tt->tm_year + 1900, tt->tm_mon);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                            data[0], data[1], data[2], data[3], fl, date);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_W), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            dBase.StoreData(TYPE_MONTH, 0, fl, date, chan);
        }
        tt->tm_mon--;
    }

    tim = time(&tim);
    localtime_r(&tim, tt);
    for (int i = 0; i < tp; i++) {
        sprintf(date, "EADPE(%02d.%02d.%02d)", tt->tm_mday, tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_DAYS, 0, date, 1);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            fl = ((float) data[0] + (float) data[1] * 256 + (float) data[2] * 256 * 256) / 100;
            sprintf(date, "%04d%02d%02d000000", tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                            data[0], data[1], data[2], data[3], fl, date);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_W), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            if (fl > 0) dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
        }
        sprintf(date, "ENDPE(%02d.%02d.%02d)", tt->tm_mday, tt->tm_mon + 1, tt->tm_year - 100);    // ddMMGGtt
        rs = send_ce(ARCH_DAYS, 0, date, 1);
        if (rs) res = this->read_ce(data, 0);
        if (res) {
            fl = ((float) data[0] + (float) data[1] * 256 + (float) data[2] * 256 * 256) / 100;
            sprintf(date, "%04d%02d%02d000000", tt->tm_year + 1900, tt->tm_mon + 1, tt->tm_mday);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] [0x%x 0x%x 0x%x 0x%x] [%f] [%s]",
                                            data[0], data[1], data[2], data[3], fl, date);
            strncpy(chan, dBase.GetChannel(const_cast<char *>(CHANNEL_W), 1, this->uuid), 40);
	    if (strlen(chan)>0)
            if (fl > 0) dBase.StoreData(TYPE_DAYS, 0, fl, date, chan);
        }
        tim -= 3600 * i * 24;
        localtime_r(&tim, tt);
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
    currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303] attempt open com-port %s on speed %d", dev_pointer, speed);
    fd = open(dev_pointer, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303] error open com-port %s", dev_pointer);
        return false;
    } else
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303] open com-port success");
    tcflush(fd, TCIOFLUSH);    //Clear send & receive buffers
    tcgetattr(fd, &tio);
    cfsetospeed(&tio, baudrate(speed));

    tio.c_cflag = CREAD | CLOCAL | baudrate(speed) | PARENB | CS7;

    //tio.c_lflag = 0;
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    //tio.c_iflag = 0;
    //tio.c_iflag |= IGNPAR|ISTRIP;

    //tio.c_iflag &= ~(INLCR | IGNCR | ICRNL);
    //tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    //tio.c_oflag &= ~(ONLCR);

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10; //Time out in 10e-1 sec
    cfsetispeed(&tio, baudrate(speed));
    fcntl(fd, F_SETFL, FNDELAY);
    //fcntl(fd, F_SETFL, 0);
    tcsetattr(fd, TCSANOW, &tio);
    tcsetattr(fd, TCSAFLUSH, &tio);

    return true;
}

uint16_t Crc16(uint8_t *Data, uint8_t DataSize) {
    uint8_t p = 0, w = 0, d = 0, q = 0;
    uint8_t sl = 0, sh = 0;
    for (p = 0; p < DataSize; p++) {
        d = Data[p];
        for (w = 0; w < 8; ++w) {
            q = 0;
            if (d & (uint8_t)1)++q;
            d >>= 1;    // d - байт данных
            if (sl & (uint8_t)1)++q;
            sl >>= 1;    // sl - младший байт контрольной суммы
            if (sl & (uint8_t)8)++q;
            if (sl & (uint8_t)64)++q;
            if (sh & (uint8_t)2)++q;    // sh - старший байт контрольной суммы
            if (sh & (uint8_t)1)sl |= 128;
            sh >>= 1;
            if (q & (uint8_t)1)sh |= 128;
        }
    }
//    sh|=128;
    return static_cast<uint8_t>(sl + sh * 256);
}

uint8_t CRC(const uint8_t *Data, const uint8_t DataSize) {
    uint8_t _CRC = 0;
    for (int i = 0; i < DataSize; i++) {
        if (Data[i] % 2 == 1) _CRC += (Data[i] | (uint8_t)0x80);
        else _CRC += ((Data[i]) & (uint8_t)0x7f);
    }
    if (_CRC > 0x80) _CRC -= 0x80;
    return _CRC;

/*    const unsigned char crc8tab[256] = {
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
    return _CRC;*/
}

bool DeviceCE::send_ce(uint16_t op, uint16_t prm, char *request, uint8_t frame) {
    uint16_t crc = 0;          //(* CRC checksum *)
    uint8_t ht = 0, nr = 0, len = 0, nbytes = 0;     //(* number of bytes in send packet *)
    unsigned char data[100];      //(* send sequence *)

    char path[100] = {0};
    if (op == SN) // open/close session
        len = 5;
    if (op == 0x4) // time
        len = 13;
    if (op == OPEN_PREV)
        len = 6;
    if (op == OPEN_CHANNEL_CE)
        len = 14;
    if (op == READ_DATE || op == READ_TIME || op == READ_PARAMETERS)
        len = 13;
    if (op == ARCH_MONTH || op == ARCH_DAYS)
        len = static_cast<uint8_t>(strlen(request) + 6);

    data[ht + 0] = static_cast<uint8_t>(this->adr & (uint8_t)0xff);
    data[ht + 1] = static_cast<uint8_t>(op);

    if (op == SN) {
        data[ht + 0] = START;
        data[ht + 1] = REQUEST;
        data[ht + 2] = 0x21;
        data[ht + 3] = CR;
        data[ht + 4] = LF;
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303][SN] wr[0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4]);
/*        crc = Crc16(data + 1, static_cast<uint8_t>(4 [D+ ht));
        data[ht + 5] = static_cast<uint8_t>(crc % 256);
        data[ht + 6] = static_cast<uint8_t>(crc / 256);
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303] wr crc [0x%x,0x%x]", data[ht + 5], data[ht + 6]);
        ht += 2;
        for (len = 0; len < ht + 5; len++) {
            currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303] %d=%x(%d)", len, data[len], data[len]);
}*/
        write(fd, &data, 5 + ht);
	sleep(1);
    }

    if (op == OPEN_PREV) {
        data[ht + 0] = 0x6;
        data[ht + 1] = 0x30;
        data[ht + 2] = 0x35;
        data[ht + 3] = 0x31;
        data[ht + 4] = CR;
        data[ht + 5] = LF;
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                        data[ht + 4], data[ht + 5]);
        write(fd, &data, 6 + ht);
    }

    if (op == OPEN_CHANNEL_CE) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x50;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        data[ht + 4] = 0x28;
        // default password
        data[ht + 5] = 0x37;
        data[ht + 6] = 0x37;
        data[ht + 7] = 0x37;
        data[ht + 8] = 0x37;
        data[ht + 9] = 0x37;
        data[ht + 10] = 0x37;
        // true password
        //memcpy(data + 5, "222222", 6);
        data[ht + 11] = 0x29;
        data[ht + 12] = ETX;
        data[ht + 13] = CRC(data + 1, 13);
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[303][OC] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3],
                                        data[ht + 4], data[ht + 5]);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][OPEN] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", data[ht + 0],
                  data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7],
                  data[ht + 8], data[ht + 9], data[ht + 10], data[ht + 11], data[ht + 12], data[ht + 13]);

        crc = Crc16(data + 1, static_cast<uint8_t>(13 + ht));
        data[ht + 14] = static_cast<uint8_t>(crc % 256);
        data[ht + 15] = static_cast<uint8_t>(crc / 256);
        ht += 2;
        write(fd, &data, ht + 14);
    }
    if (op == WATCH) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        sprintf((char *) data + 4, "WATCH()");
        data[ht + 11] = ETX;
        data[ht + 12] = CRC(data + 1, 11);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][WATCH] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", data[ht + 0],
                  data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7],
                  data[ht + 8], data[ht + 9], data[ht + 10], data[ht + 11], data[ht + 12]);
        write(fd, &data, 13);
    }

    if (op == READ_DATE || op == READ_TIME) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        if (op == READ_DATE) sprintf((char *) data + 4, "DATE_()");
        if (op == READ_TIME) sprintf((char *) data + 4, "TIME_()");
        data[ht + 11] = ETX;
        data[ht + 12] = CRC(data + 1, 11);
            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][DATE|TIME] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", data[ht + 0],
                  data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6], data[ht + 7],
                  data[ht + 8], data[ht + 9], data[ht + 10], data[ht + 11], data[ht + 12]);
        write(fd, &data, 13);
    }
    if (frame == READ_PARAMETERS) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        if (op == CURRENT_W) sprintf((char *) data + 4, "POWEP()");
        //if (op==CURRENT_I) sprintf (data+4,"CURRE()");
        if (op == CURRENT_I) sprintf((char *) data + 4, "ET0PE()");
        if (op == CURRENT_F) sprintf((char *) data + 4, "FREQU()");
        if (op == CURRENT_U) sprintf((char *) data + 4, "VOLTA()");
        data[11] = ETX;
        data[12] = CRC(data + 1, 11);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][CURR] wr[%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]", 13,
                  data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6],
                  data[ht + 7], data[ht + 8], data[ht + 9], data[ht + 10], data[ht + 11], data[ht + 12]);
        write(fd, &data, 13);
    }
    if (op == ARCH_MONTH || op == ARCH_DAYS) {
        data[ht + 0] = 0x1;
        data[ht + 1] = 0x52;
        data[ht + 2] = 0x31;
        data[ht + 3] = STX;
        sprintf((char *) data + ht + 4, "%s", request);
        len = static_cast<uint8_t>(strlen(request) + 3);
        data[ht + len + 1] = ETX;
        data[ht + len + 2] = CRC(data + 1, static_cast<const uint8_t>(len + 2));
        //crc=CRC (data+ht, 7, 0);
        sprintf((char *) data + ht + 4, "%s", request);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][ARCH] %s]",request);
/*        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303][ARCH] wr[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]",
                                        data[ht + 0], data[ht + 1], data[ht + 2], data[ht + 3], data[ht + 4], data[ht + 5], data[ht + 6],
                                        data[ht + 7], data[ht + 8], data[ht + 9], data[ht + 10], data[ht + 11], data[ht + 12], data[ht + 13],
                                        data[ht + 14], data[ht + 15], data[ht + 16], data[ht + 17]);
*/
        write(fd, &data, len + 3);
        sleep(1);
    }

    return true;
}

//-----------------------------------------------------------------------------
uint16_t DeviceCE::read_ce(uint8_t *dat, uint8_t type) {
    uint16_t crc = 0;        //(* CRC checksum *)
    int nbytes = 0;     //(* number of bytes in recieve packet *)
    uint16_t bytes = 0;      //(* number of bytes in packet *)
    unsigned char data[500];      //(* recieve sequence *)

    usleep(1200000);
    //ioctl(fd, FIONREAD, &nbytes);
//    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] nbytes=%d", nbytes);
    nbytes = read(fd, &data, 75);
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] nbytes=%d", nbytes);

    //data[0]=0x2;
    //sprintf (data+1,"POWEP(%.5f)",8.14561);
    //data[13]=0x3; data[14]=CRC (data+1,13,1); nbytes=15;
    //if (debug>2) ULOGW("[303] nbytes=%d %x",nbytes,data[0]);
    //usleep(200000);
    //ioctl(fd, FIONREAD, &bytes);

    if (bytes > 0 && nbytes > 0 && nbytes < 50) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] bytes=%d fd=%d adr=%d", bytes, fd, &data + nbytes);
        bytes = static_cast<uint16_t>(read(fd, &data + nbytes, bytes));
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] bytes=%d", bytes);
        nbytes += bytes;
    }
    if (nbytes == 1) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] [%d][0x%x]", nbytes, data[0]);
        dat[0] = data[0];
        return nbytes;
    }
    if (nbytes > 5) {
        crc = CRC(data + 1, nbytes - 2);
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] [%d][0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x][crc][0x%x,0x%x]",
                                        nbytes, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9],
                                            data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17],
					    data[nbytes-1], crc);
	//if (nbytes>55)
	//    for (int ii=0; ii<nbytes; ii++)
	//	printf ("0x%x\n",data[ii]);
        //currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[ce] [%d][0x%x,0x%x,0x%x]",                    nbytes, data[nbytes - 1], data[nbytes - 2], crc);
        if (data[nbytes - 1] != 0xa && data[nbytes - 2] != 0xd && nbytes<50)
            if (crc != data[nbytes - 1] || nbytes < 8) nbytes = 0;

        if (nbytes < 100 && nbytes > 11) {
            if (nbytes > 16)
                memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
            else {
                if (nbytes == 12 && (data[9] == 0xf0 || data[9] == 0x40)) {
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] HTC answer: no counter answer present [%d]", data[9]);
                }
                if (nbytes == 16 && ((data[11] & (uint8_t)0xf) == 0x5))
                    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[303] channel not open correctly [%d]", data[11]);
                memcpy(dat, data + 11, static_cast<size_t>(nbytes - 11));
            }

            memcpy(dat, data + 1, static_cast<size_t>(nbytes - 3));
            dat[nbytes - 3] = 0;
        } else dat[0] = 0;
        return nbytes;
    }
    return 0;
}
