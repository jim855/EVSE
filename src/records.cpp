#include <Arduino.h>
#include <SPIFFS.h>
#include "records.h"

LocalRecords::LocalRecords() {
    // Constructor
}

void LocalRecords::init() {
    SPIFFS.begin(true);
    if (!SPIFFS.exists("/records")) {
        log_e("[Record] Init empty records file");
        File recordFile = SPIFFS.open("/records", "wb+");
        if (!recordFile) {
            log_e("[Record] fail on open record file to init");
            return;
        }
        recordFile.write((byte*) &_records, sizeof(_records));
        recordFile.close();
    }
    SPIFFS.end();
}

void LocalRecords::append(ChargeRecord rec) {
    ChargeRecord newRecords[LOCAL_RECORDS_SIZE];
    newRecords[0] = rec;
    for (int x=LOCAL_RECORDS_SIZE-2; x>-1; x--) {
        _records[x+1]=_records[x];
    }
    _records[0] = rec;
}

void LocalRecords::clear() {
    memset(_records, 0, sizeof(_records));
    writeFile();
}

bool LocalRecords::isFull() {
    int cnt=0;
    for (int x=0; x<LOCAL_RECORDS_SIZE; x++) {
        if ( _records[x].cardId.length() > 0) {
            cnt++;
        }
    }
    if (cnt == LOCAL_RECORDS_SIZE)
    {
        return true;
    }
    return false;
}
void LocalRecords::dumpRecords() {
    log_e("[DebugDump] ================= DUMP ======================");
    for (int x=0; x<LOCAL_RECORDS_SIZE; x++) {
        log_e("[DebugDump] Record-%d: CARD=%s, startAt:%d, stopAt:%d, watts:%lf", 
            x, 
            _records[x].cardId, _records[x].chargeStartAt,
            _records[x].chargeStopAt, _records[x].usedWatt
        );
    }
    log_e("[DebugDump] ================== END ======================");
}

String LocalRecords::getJsonRecords() {
    String result = "[";
    for (int x=LOCAL_RECORDS_SIZE-1; x>=0; x--) {
        if (_records[x].cardId.length()>0) {
            result = result + "{";
            result = result + "\"card\":\"" +  _records[x].cardId + "\",";
            result = result + "\"startAt\":" + String(_records[x].chargeStartAt) + ",";
            result = result + "\"stopAt\":" + String(_records[x].chargeStopAt) + ",";
            result = result + "\"watts\":" + String(_records[x].usedWatt);
            result = result + "}";
            if (x!=0) {
                result = result + ",";
            }
        }
    }
    result = result + "]";
    clear();
    return result;
}

void LocalRecords::readFile() {
    SPIFFS.begin(true);
    File recordFile = SPIFFS.open("/records", "rb");
    if (!recordFile) {
        log_e("[Record] fail on open record file to read");
        return;
    }
    recordFile.readBytes((char*) &_records, sizeof(_records));
    SPIFFS.end();
}

void LocalRecords::writeFile() {
    SPIFFS.begin(true);
    File recordFile = SPIFFS.open("/records", "wb+");
    if (!recordFile) {
        log_e("[Record] fail on open record file to write");
        return;
    }
    SPIFFS.end();
}