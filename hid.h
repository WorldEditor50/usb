#ifndef HID_H
#define HID_H
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include "hidapi/hidapi.h"

struct iDevice {
    unsigned short vendorID;
    unsigned short productID;
    unsigned short usagePage;
    unsigned short usage;
};

class Hid : public iDevice
{
public:
    using FnProcess = std::function<void(unsigned char*, std::size_t)>;
    using FnNotify = std::function<void(bool)>;
    enum State {
        STATE_NONE = 0,
        STATE_RUN,
        STATE_OPENED,
        STATE_CLOSED,
        STATE_PAUSE,
        STATE_TERMINATE
    };
    enum Code {
        HID_SUCCESS = 0,
        HID_OPEN_FAILED,
        HID_WRITE_FAILED,
        HID_READ_FAILED,
        HID_SEND_FEATURE_REPORT_FAILED,
        HID_RECV_FEATURE_REPORT_FAILED

    };
    class Init
    {
    public:
        Init() { hid_init(); }
        ~Init() { hid_exit(); }
    };
    constexpr static std::size_t max_recv_size = 1024;
    constexpr static std::size_t max_send_size = 1024;
    static Init init;
protected:
    hid_device *handle;
    std::thread recvThread;
    std::mutex mutex;
    std::condition_variable condit;
    FnProcess process;
    FnNotify notify;
    int state;
    bool specificUsage;
    unsigned char* recvCache;
protected:
    void recv();
public:
    Hid();
    ~Hid();
    static std::vector<iDevice> enumerate();
    int openDevice(unsigned short vid, unsigned short pid);
    int openDevice(unsigned short vid, unsigned short pid, unsigned short usagePage, unsigned short usage);
    void closeDevice();
    void setNonBlock(bool on);
    void registerProcess(const FnProcess &func);
    void registerNotify(const FnNotify &func);
    int write(const unsigned char *data, std::size_t datasize);
    int write(const std::string &data);
    int read(unsigned char* &data, std::size_t & datasize);
    int sendFeatureReport(const unsigned char* data, std::size_t datasize);
    int recvFeatureReport(unsigned char* &data, std::size_t &datasize);
    int start(unsigned short vid, unsigned short pid);
    int start(unsigned short vid, unsigned short pid, unsigned short usagePage, unsigned short usage);
    void stop();

};

#endif // HID_H
