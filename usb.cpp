#include "usb.h"

Usb::Context Usb::context;

int Usb::attach(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *userdata)
{
    if (userdata == nullptr) {
        return -1;
    }
    Usb *usb = static_cast<Usb*>(userdata);
    usb->_openDevice();
    return 0;
}

int Usb::detach(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *userdata)
{
    if (userdata == nullptr) {
        return -1;
    }
    Usb *usb = static_cast<Usb*>(userdata);
    usb->closeDevice();
    return 0;
}

void Usb::handleEvent()
{
    while (isHandleEvent.load()) {
        struct timeval val;
        val.tv_sec = 3;
        val.tv_usec = 0;
        libusb_handle_events_timeout(Usb::context.get(), &val);
        //libusb_handle_events_completed(Usb::context.get(), nullptr);
    }
    return;
}

Usb::Usb():
    handle(nullptr),
    interfaceNum(1),
    isHandleEvent(false),
    eventThread(nullptr)
{
    attachNotify = [](){};
    detachNotify = [](){};
}

Usb::~Usb()
{
    stopHandleEvent();
}

int Usb::findEndpoint(libusb_device *dev, unsigned char &inEndpoint, unsigned char &outEndpoint)
{
    libusb_config_descriptor *configDesc = nullptr;
    int ret = libusb_get_config_descriptor(dev, 0, &configDesc);
    if (ret != LIBUSB_SUCCESS) {
        LOG_INFO("fail to get config descriptor", ret);
        return ret;
    }
    /* find endpoint */
     for(int i = 0; i < configDesc->bNumInterfaces; i++){
         for (int j = 0; j < configDesc->interface[i].num_altsetting; j++) {
             for (int k = 0; k < configDesc->interface[i].altsetting[j].bNumEndpoints; k++) {
                 const struct libusb_endpoint_descriptor *endpoint = &(configDesc->interface[i].altsetting[j].endpoint[k]);
                 if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                     inEndpoint = endpoint->bEndpointAddress;
                 } else {
                     outEndpoint = endpoint->bEndpointAddress;
                 }
             }
         }
     }
     libusb_free_config_descriptor(configDesc);
     return 0;
}

std::vector<iDevice> Usb::enumerate()
{
    std::vector<iDevice> devices;
    libusb_device **devs;
    ssize_t deviceNum = libusb_get_device_list(nullptr, &devs);
    if (deviceNum < 0) {
        return devices;
    }
    for (int i = 0; i < deviceNum; i++) {
        libusb_device* dev = devs[i];
        if (dev == nullptr) {
            continue;
        }
        struct libusb_device_descriptor desc;
        int ret = libusb_get_device_descriptor(dev, &desc);
        if (ret < 0) {
            continue;
        }
        Usb::iDevice device;
        device.vendorID = desc.idVendor;
        device.productID = desc.iProduct;
        Usb::findEndpoint(dev, device.inEndpoint, device.outEndpoint);
		devices.push_back(device);
    }
    libusb_free_device_list(devs, 1);
    return devices;
}

int Usb::findDevice(unsigned short vendorID, unsigned short productID, libusb_device_handle* &handle, unsigned char &inEndpoint, unsigned char &outEndpoint)
{
    /* get device's list */
    libusb_device** devList = nullptr;
    ssize_t deviceNum = libusb_get_device_list(Usb::context.get(), &devList);
    if (deviceNum <= 0) {
        LOG_INFO("fail to get device list", cnt);
        return LIBUSB_ERROR_NO_DEVICE;
    }
    int ret = 0;
    /* get device's handle */
    for (int index = 0; index < deviceNum; index++) {
        libusb_device* dev = devList[index];
        if (dev == nullptr) {
            continue;
        }
        /* get device's descriptor */
        libusb_device_descriptor desc;
        memset((void*)&desc, 0, sizeof(libusb_device_descriptor));
        ret = libusb_get_device_descriptor(dev, &desc);
        if (ret < 0) {
            LOG_INFO("fail to get device descriptor", ret);
            break;
        }
        if (desc.idVendor != vendorID || desc.idProduct != productID) {
            ret = LIBUSB_ERROR_NOT_FOUND;
            continue;
        } else {
            ret = findEndpoint(dev, inEndpoint, outEndpoint);
            if (ret != 0) {
                continue;
            }
            /* get handle */
            ret = libusb_open(dev, &handle);
            if (ret != LIBUSB_SUCCESS) {
                continue;
            }
            break;
        }
    }
    libusb_free_device_list(devList, 1);
    return ret;
}

int Usb::openDevice(unsigned short vendorID, unsigned short productID)
{
    this->vendorID = vendorID;
    this->productID = productID;
    return _openDevice();
}

int Usb::_openDevice()
{
    if (handle != nullptr) {
        return USB_SUCCESS;
    }
    if (context.get() == nullptr) {
        return USB_INVALID_CONTEXT;
    }
    int ret = findDevice(vendorID, productID, handle, inEndpoint, outEndpoint);
    if (ret != LIBUSB_SUCCESS) {
        return USB_OPEN_FAILED;
    }
    /* kernel driver */
    ret = libusb_detach_kernel_driver(handle, interfaceNum);
    if (ret != LIBUSB_SUCCESS) {

    }
    /* claim interface */
    ret = libusb_claim_interface(handle, interfaceNum);
    if (ret != LIBUSB_SUCCESS) {
        LOG_INFO("fail to claim interface", ret);
    }

    /* set config */
    ret = libusb_set_configuration(handle, 1);
    if (ret != LIBUSB_SUCCESS) {
        LOG_INFO("fail to set configuration", ret);
    }
    /* notify */
    attachNotify();
    return USB_SUCCESS;
}

void Usb::closeDevice()
{
    if (handle != nullptr) {
        libusb_release_interface(handle, interfaceNum);
        libusb_close(handle);
        handle = nullptr;
        /* notify */
        detachNotify();
    }
    return;
}

int Usb::sendBulk(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    int ret = 0;
    for (int i = 0; i < max_retry_count; i++) {
        int actualSize = 0;
        ret = libusb_bulk_transfer(handle, outEndpoint, data, size, &actualSize, timeout_duration);
        if (ret == LIBUSB_ERROR_PIPE) {
            continue;
        } else {
            break;
        }
    }
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::recvBulk(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    int ret = 0;
    for (int i = 0; i < max_retry_count; i++) {
        int actualSize = 0;
        ret = libusb_bulk_transfer(handle, inEndpoint, data, size, &actualSize, timeout_duration);
        if (ret == LIBUSB_ERROR_PIPE) {
            continue;
        } else {
            break;
        }
    }
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::sendInterrupt(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    int ret = 0;
    for (int i = 0; i < max_retry_count; i++) {
        int actualSize = 0;
        ret = libusb_interrupt_transfer(handle, outEndpoint, data, size, &actualSize, timeout_duration);
        if (ret == LIBUSB_ERROR_PIPE) {
            continue;
        } else {
            break;
        }
    }
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::recvInterrupt(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    int ret = 0;
    for (int i = 0; i < max_retry_count; i++) {
        int actualSize = 0;
        ret = libusb_interrupt_transfer(handle, inEndpoint, data, size, &actualSize, timeout_duration);
        if (ret == LIBUSB_ERROR_PIPE) {
            continue;
        } else {
            break;
        }
    }
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::sendControl(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    unsigned char requestType;
    unsigned char bRequest;
    unsigned short value;
    unsigned short index;
    int ret = libusb_control_transfer(handle, requestType, bRequest, value, index,
                                      data, size, timeout_duration);
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::recvControl(unsigned char *data, size_t size)
{
    if (handle == nullptr) {
        return USB_INVALID_PARAM;
    }
    unsigned char requestType;
    unsigned char bRequest;
    unsigned short value;
    unsigned short index;
    int ret = libusb_control_transfer(handle, requestType, bRequest, value, index,
                                      data, size, timeout_duration);
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

int Usb::registerAttach(Usb::FnHotplugHandler attachHandler)
{
    /* support */
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        return USB_UNSUPPORT;
    }
    /* attach */
    int ret = libusb_hotplug_register_callback(Usb::context.get(),
                                               LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                               LIBUSB_HOTPLUG_NO_FLAGS,
                                               vendorID,
                                               productID,
                                               LIBUSB_HOTPLUG_MATCH_ANY,
                                               &Usb::attach,
                                               this,
                                               &attachHandler);
    if (ret != LIBUSB_SUCCESS) {
        return USB_REGISTER_FAILED;
    }
    return USB_SUCCESS;

}

int Usb::registerDetach(Usb::FnHotplugHandler detachHandler)
{
    /* capability */
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        return USB_UNSUPPORT;
    }
    /* detach */
    int ret = libusb_hotplug_register_callback(Usb::context.get(),
                                               LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                               LIBUSB_HOTPLUG_NO_FLAGS,
                                               vendorID,
                                               productID,
                                               LIBUSB_HOTPLUG_MATCH_ANY,
                                               &Usb::detach,
                                               this,
                                               &detachHandler);
    if (ret != LIBUSB_SUCCESS) {
        return USB_REGISTER_FAILED;
    }
    return LIBUSB_SUCCESS;
}

int Usb::startHandleEvent()
{
    /* capability */
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        return USB_UNSUPPORT;
    }
    if (eventThread != nullptr) {
        return USB_SUCCESS;
    }
    isHandleEvent = true;
    eventThread = std::make_shared<std::thread>(&Usb::handleEvent, this);
    return USB_SUCCESS;
}

int Usb::stopHandleEvent()
{
    isHandleEvent.store(false);
    if (eventThread != nullptr) {
        eventThread->join();
    }
    return USB_SUCCESS;
}

void Usb::registerAttachNotify(const Usb::FnAttachNotify &notify)
{
    attachNotify = notify;
    return;
}

void Usb::registerDetachNotify(const Usb::FnDetachNotify &notify)
{
    detachNotify = notify;
    return;
}
