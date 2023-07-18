#include "hid.h"

Hid::Init Hid::init;

void Hid::recv()
{
    while (1) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            condit.wait(locker, [=]()->bool{
                return state == STATE_OPENED || state == STATE_TERMINATE || state == STATE_RUN;
            });
            if (state == STATE_TERMINATE) {
                break;
            }
            if (state == STATE_CLOSED) {
                int ret = 0;
                if (specifiedUsage == false) {
                    ret = openDevice(vendorID, productID);
                } else {
                    ret = openDevice(vendorID, productID, usagePage, usage);
                }
                if (ret != HID_SUCCESS) {
                    continue;
                } else {
                    state = STATE_RUN;
                    notify(true);
                }
            }
        }

        int len = hid_read(handle, recvCache, max_recv_size);
        if (len < 0) {
            notify(false);
            handle = nullptr;
            std::unique_lock<std::mutex> locker(mutex);
            state = STATE_CLOSED;
            condit.notify_all();
            continue;
        }
        if (len == 0) {
            continue;
        }

        process(recvCache, len);
    }
    return;
}

Hid::Hid():
    handle(nullptr),
    state(STATE_NONE),
    specifiedUsage(false),
    recvCache(nullptr)
{
    recvCache = new unsigned char[max_recv_size];
    process = [](unsigned char*, std::size_t){};
    notify = [](bool){};
}

Hid::~Hid()
{
    if (recvCache != nullptr) {
        delete [] recvCache;
        recvCache = nullptr;
    }
    if (state == STATE_RUN) {
        stop();
        recvThread.join();
    }
}

std::vector<iHid> Hid::enumerate()
{
    std::vector<iHid> devices;
    struct hid_device_info *devs = nullptr;
    struct hid_device_info *curDevice = nullptr;

    devs = hid_enumerate(0x0, 0x0);
    curDevice = devs;
    while (curDevice != nullptr) {
        iHid dev;
        dev.vendorID = curDevice->vendor_id;
        dev.productID = curDevice->product_id;
        dev.usagePage = curDevice->usage_page;
        dev.usage = curDevice->usage;
        devices.push_back(dev);
        curDevice = curDevice->next;
    }
    hid_free_enumeration(devs);
    return devices;
}

int Hid::openDevice(unsigned short vid, unsigned short pid)
{
    if (handle != nullptr) {
        return HID_SUCCESS;
    }
    handle = hid_open(vid, pid, nullptr);
    if (handle == nullptr) {
        return HID_OPEN_FAILED;
    }
    vendorID = vid;
    productID = pid;
    return HID_SUCCESS;
}

int Hid::openDevice(unsigned short vid, unsigned short pid, unsigned short usagePage, unsigned short usage)
{
    if (handle != nullptr) {
        return HID_SUCCESS;
    }
    struct hid_device_info *devs = nullptr;
    struct hid_device_info *dev = nullptr;
    const char *devPath = nullptr;
    devs = hid_enumerate(vid, pid);
    dev = devs;
    while (dev != nullptr) {
        if (dev->vendor_id == vid && dev->product_id == pid &&
                dev->usage == usage && dev->usage_page == usagePage) {
            devPath = dev->path;
            break;
        }
        dev = dev->next;
    }

    if (devPath != nullptr) {
        /* Open the device */
        handle = hid_open_path(devPath);
    } else {
        hid_free_enumeration(devs);
        return HID_OPEN_FAILED;
    }
    hid_free_enumeration(devs);
    specifiedUsage = true;
    return HID_SUCCESS;
}

void Hid::closeDevice()
{
    if (handle != nullptr) {
        hid_close(handle);
    }
    return;
}

void Hid::setNonBlock(bool on)
{
    if (handle == nullptr) {
        return;
    }
    hid_set_nonblocking(handle, on);
    return;
}

void Hid::registerProcess(const Hid::FnProcess &func)
{
    process = func;
    return;
}

void Hid::registerNotify(const Hid::FnNotify &func)
{
    notify = func;
    return;
}

int Hid::write(const unsigned char *data, std::size_t datasize)
{
    if (handle == nullptr) {
        return HID_OPEN_FAILED;
    }
    std::size_t pos = 0;
    unsigned char buffer[max_send_size] = {0};
#if WIN32
    while (pos < datasize) {
        memset(buffer, 0, max_send_size);
        if (datasize - pos > max_send_size - 1) {
            memcpy(buffer + 1, data + pos, max_send_size - 1);
        } else {
            memcpy(buffer + 1, data + pos, datasize - pos);
        }
        int len = hid_write(handle, buffer, max_send_size);
        if (len < 0) {
            return HID_WRITE_FAILED;
        }
        pos += len + 1;
    }
#else
    while (pos < datasize) {
        memset(buffer, 0, max_send_size);
        if (datasize - pos > max_send_size) {
            memcpy(buffer, data + pos, max_send_size);
        } else {
            memcpy(buffer, data + pos, datasize - pos);
        }
        int len = hid_write(handle, buffer, max_send_size);
        if (len < 0) {
            return HID_WRITE_FAILED;
        }
        pos += len;
    }

#endif

    return HID_SUCCESS;
}

int Hid::write(const std::string &data)
{
    return Hid::write((unsigned char*)data.c_str(), data.size());
}

int Hid::read(unsigned char *&data, size_t &datasize)
{
    if (handle == nullptr) {
        return HID_OPEN_FAILED;
    }

    int len = hid_read(handle, data, datasize);
    if (len < 0) {
        return HID_READ_FAILED;
    }
    datasize = len;
    return HID_SUCCESS;
}

int Hid::sendFeatureReport(const unsigned char *data, size_t datasize)
{
    if (handle == nullptr) {
        return HID_OPEN_FAILED;
    }
    int len = hid_send_feature_report(handle, data, datasize);
    if (len < 0) {
        return HID_SEND_FEATURE_REPORT_FAILED;
    }
    return HID_SUCCESS;
}

int Hid::recvFeatureReport(unsigned char *&data, size_t &datasize)
{
    if (handle == nullptr) {
        return HID_OPEN_FAILED;
    }

    int len = hid_get_feature_report(handle, data, datasize);
    if (len < 0) {
        return HID_SEND_FEATURE_REPORT_FAILED;
    }
    datasize = len;
    return HID_SUCCESS;
}

int Hid::start(unsigned short vid, unsigned short pid)
{
    if (state != STATE_NONE) {
        return HID_SUCCESS;
    }
    int ret = openDevice(vid, pid);
    if (ret != HID_SUCCESS) {
        return ret;
    }
    state = STATE_OPENED;
    recvThread = std::thread(&Hid::recv, this);
    return HID_SUCCESS;
}

int Hid::start(unsigned short vid, unsigned short pid, unsigned short usagePage, unsigned short usage)
{
    if (state != STATE_NONE) {
        return HID_SUCCESS;
    }
    int ret = openDevice(vid, pid, usagePage, usage);
    if (ret != HID_SUCCESS) {
        return ret;
    }
    state = STATE_OPENED;
    recvThread = std::thread(&Hid::recv, this);
    return HID_SUCCESS;
}

void Hid::stop()
{
    std::unique_lock<std::mutex> locker(mutex);
    state = STATE_TERMINATE;
    condit.notify_all();
    return;
}


