#include <iostream>
#include "usb.h"
#include "hid.h"

void test_enumerate_usb_device()
{
    std::vector<iUsb> devs = Usb::enumerate();
    for (auto &x : devs) {
        std::cout<<"vendorID:"<<x.vendorID<<", productID:"<<x.productID<<std::endl;
    }
    return;
}

void test_enumerate_hid_device()
{
    std::vector<iHid> devs = Hid::enumerate();
    for (auto &x : devs) {
        std::cout<<"vendorID:"<<x.vendorID<<", productID:"<<x.productID<<std::endl;
    }
}

int main()
{
    //test_enumerate_usb_device();
    return 0;
}
