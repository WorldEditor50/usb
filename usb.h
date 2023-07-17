#ifndef USB_H
#define USB_H
#include <iostream>
#include <fstream>
#include <thread>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <functional>
#include <cstdlib>
#include <cstring>
#include "libusb.h"

#if 0
#define LOG_INFO(message, ret) do { \
    std::cout<<"function: "<<__FUNCTION__<<" line: "<<__LINE__<<" message: "<<(message)<<std::endl; \
    std::cout<<"ret = "<<(ret)<<std::endl; \
    std::cout<<libusb_error_name((ret))<<std::endl; \
} while (0)
#else
#define LOG_INFO(message, ret)
#endif


struct iDevice
{
    unsigned short vendorID;
    unsigned short productID;
    unsigned char inEndpoint;
    unsigned char outEndpoint;
};

class Usb : public iDevice
{
public:
    class Context
    {
    public:
        std::mutex mutex;
        libusb_context *context;
    public:
        Context():context(nullptr){}
        ~Context()
        {
            std::lock_guard<std::mutex> guard(mutex);
            if (context != nullptr) {
                libusb_exit(context);
                context = nullptr;
            }
        }
        libusb_context* get()
        {
            std::lock_guard<std::mutex> guard(mutex);
            if (context == nullptr) {
                libusb_init(&context);
            }
            return context;
        }
    };

    enum Code {
        USB_SUCCESS = 0,
        USB_INVALID_PARAM,
        USB_INVALID_CONTEXT,
        USB_OPEN_FAILED,
        USB_TRANSFER_ERROR,
        USB_UNSUPPORT,
        USB_REGISTER_FAILED
    };

    using FnHotplug = libusb_hotplug_callback_fn;
    using FnHotplugHandler = libusb_hotplug_callback_handle;
    using FnAttachNotify = std::function<void(void)>;
    using FnDetachNotify = std::function<void(void)>;
    constexpr static int timeout_duration = 3000;
    constexpr static int max_retry_count = 3;
protected:
    /* device */
    static Context context;
    libusb_device_handle *handle;
    int interfaceNum;
    /* hotplug */
    std::atomic_bool isHandleEvent;
    std::shared_ptr<std::thread> eventThread;
    /* notify */
    FnAttachNotify  attachNotify;
    FnDetachNotify  detachNotify;
protected:
    /* handle hotplug event */
    static int attach(libusb_context *ctx,
                      libusb_device *dev,
                      libusb_hotplug_event event,
                      void* userdata);
    static int detach(libusb_context *ctx,
                      libusb_device *dev,
                      libusb_hotplug_event event,
                      void* userdata);

    void handleEvent();
public:
    Usb();
    ~Usb();
    /* device */
    static int findEndpoint(libusb_device* dev, unsigned char &endpointIn, unsigned char &endpointOut);
    static std::vector<Usb::iDevice> enumerate();
    static int findDevice(unsigned short vendorID, unsigned short productID, libusb_device_handle* &handle, unsigned char &inEndpoint, unsigned char &outEndpoint);
    int openDevice(unsigned short vendorID, unsigned short productID);
    int _openDevice();
    void closeDevice();
    /* sync transfer */
    int sendBulk(unsigned char *data, std::size_t size);
    int recvBulk(unsigned char *data, std::size_t size);
    int sendInterrupt(unsigned char *data, std::size_t size);
    int recvInterrupt(unsigned char *data, std::size_t size);
    int sendControl(unsigned char *data, std::size_t size);
    int recvControl(unsigned char *data, std::size_t size);
    /* register hotplug */
    int registerAttach(FnHotplugHandler attachHandler);
    int registerDetach(FnHotplugHandler detachHandler);
    /* event */
    int startHandleEvent();
    int stopHandleEvent();
    /* notify */
    void registerAttachNotify(const FnAttachNotify &notify);
    void registerDetachNotify(const FnDetachNotify &notify);
};

#endif // USB_H
