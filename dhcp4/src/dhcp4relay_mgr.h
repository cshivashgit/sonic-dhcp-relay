#pragma once

#include <atomic>
#include <boost/thread.hpp>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "dbconnector.h"
#include "dhcp4relay.h"
#include "select.h"
#include "subscriberstatetable.h"
#include "table.h"

class DHCPMgr {
   private:
    std::atomic<bool> stopThread;

   public:
    DHCPMgr() : stopThread(false) {}
    ~DHCPMgr();

    void initialize_config_listner();
    void handleSwssNotification();
    void processRelayNotification(std::deque<swss::KeyOpFieldsValuesTuple> &entries);
    void processInterfaceNotification(std::deque<swss::KeyOpFieldsValuesTuple> &entries);
    void processDeviceMetadataNotification(std::deque<swss::KeyOpFieldsValuesTuple> &entries);
};
