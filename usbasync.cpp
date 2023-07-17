#include "usbasync.h"

void UsbAsync::handleTransferEvent()
{
    while (1) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            condit.wait(locker, [this]()->bool{
                            return state == STATE_RUN || state == STATE_TERMINATE;
                        });
            if (state == STATE_TERMINATE) {
                break;
            }
        }
        int ret = libusb_handle_events(context.get());
        if (ret < 0) {
            break;
        }
    }
    return;
}

void UsbAsync::recv()
{
    while (1) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            condit.wait(locker, [this]()->bool{
                            return state == STATE_RUN || state == STATE_TERMINATE;
                        });
            if (state == STATE_TERMINATE) {
                break;
            }
        }
        libusb_transfer* transfer = libusb_alloc_transfer(0);
        unsigned char *buf = new unsigned char[64];
        memset(buf, 0, 64);
        transfer->actual_length = 0;
        int ret = libusb_submit_transfer(transfer);
        if (ret < 0) {
            libusb_cancel_transfer(transfer);
            libusb_free_transfer(transfer);
            transfer = nullptr;
            continue;
        }
    }
    return;
}

void UsbAsync::writeHandler(libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        int ret = libusb_submit_transfer(transfer);
        if (ret < 0) {
           libusb_cancel_transfer(transfer);
        }
    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {

    }
    libusb_free_transfer(transfer);
    return;
}

void UsbAsync::readHandler(libusb_transfer *transfer)
{
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        if (transfer->actual_length > 0) {
            UsbAsync* this_ = static_cast<UsbAsync*>(transfer->user_data);
            this_->process(transfer->buffer, transfer->actual_length);
        }
        int ret = libusb_submit_transfer(transfer);
        if (ret < 0) {
            printf("error libusb_submit_transfer : %s\n", libusb_strerror(libusb_error(ret)));
            libusb_cancel_transfer(transfer);
        }
    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        libusb_free_transfer(transfer);
    }
    return;
}

UsbAsync::UsbAsync()
{

}

UsbAsync::~UsbAsync()
{

}

int UsbAsync::start(unsigned short vendorID, unsigned short productID)
{
    int ret = Usb::openDevice(vendorID, productID);
    if (ret != USB_SUCCESS) {
        return USB_OPEN_FAILED;
    }
    eventThread = std::thread(&UsbAsync::handleHotplugEvent, this);
    return USB_SUCCESS;
}

void UsbAsync::stop()
{
    std::unique_lock<std::mutex> locker(mutex);
    state = STATE_TERMINATE;
    condit.notify_all();
    return;
}

int UsbAsync::write(unsigned char *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return USB_INVALID_PARAM;
    }
    libusb_transfer *inTransfer = libusb_alloc_transfer(0);

    libusb_fill_bulk_transfer(inTransfer, handle, inEndpoint,
                              data, size, UsbAsync::writeHandler, this, timeout_duration);
    inTransfer->type = LIBUSB_TRANSFER_TYPE_BULK;
    int ret = libusb_submit_transfer(inTransfer);
    if (ret != LIBUSB_SUCCESS) {
        return USB_TRANSFER_ERROR;
    }
    return USB_SUCCESS;
}

