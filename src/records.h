#include <Arduino.h>

#define LOCAL_RECORDS_SIZE 5

struct ChargeRecord {
  String cardId;
  unsigned long chargeStartAt;
  unsigned long chargeStopAt;
  double usedWatt;
};

class LocalRecords {
    public:
        LocalRecords();
        void init();
        void append(ChargeRecord rec);
        void clear();
        bool isFull();
        void readFile();
        void writeFile();
        String getJsonRecords();
        void dumpRecords();

    private:
        ChargeRecord _records[LOCAL_RECORDS_SIZE];

};