#ifndef USBASYNC_H
#define USBASYNC_H
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "usb.h"

class UsbAsync : public Usb
{
public:
    enum State {
        STATE_NONE = 0,
        STATE_RUN,
        STATE_TERMINATE
    };
    enum Type {
        TYPE_BULK = 0,
        TYPE_INTERRUPT,
        TYPE_CONTROL
    };
    using FnProcess = std::function<void(unsigned char*, std::size_t)>;
protected:
    std::thread eventThread;
    std::mutex mutex;
    std::condition_variable condit;
    int state;
    FnProcess process;
protected:
    void handleTransferEvent();
    void recv();
    static void writeHandler(libusb_transfer *transfer);
    static void readHandler(libusb_transfer *transfer);
public:
    UsbAsync();
    ~UsbAsync();
    void registerProcess(const FnProcess &func) {process = func;}
    int start(unsigned short vendorID, unsigned short productID);
    void stop();
    int write(unsigned char* data, std::size_t size);
    int read(unsigned char* &data, std::size_t &size);

};

#endif // USBASYNC_H
