#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

#include "mock_relay.h"
#include "../src/dhcp4relay_stats.h"

using namespace swss;

// Test fixture
class DHCPCounterTableTest : public ::testing::Test {
protected:
    std::unique_ptr<DHCPCounterTable> counterTable;

    void SetUp() override {
        counterTable = std::make_unique<DHCPCounterTable>();
    }

    void TearDown() override {
        counterTable.reset();
    }
};

// Test the delta calculation function
TEST(CalculateDeltaTest, CalculatesDeltaCorrectly) {
    // Normal case
    EXPECT_EQ(calculate_delta(10, 5), 5);

    // Zero delta
    EXPECT_EQ(calculate_delta(5, 5), 0);

    // Handle overflow case
    uint64_t max_val = std::numeric_limits<uint64_t>::max();
    uint64_t old_val = max_val - 5;
    uint64_t new_val = 10;

    EXPECT_EQ(calculate_delta(new_val, old_val), 16);
}

// Test interface initialization
TEST_F(DHCPCounterTableTest, InitializeInterface) {
    const std::string interface = "Ethernet0";

    // Initialize the interface
    counterTable->initializeInterface(interface);

    // Increment counters
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    counterTable->incrementCounter(interface, "TX", DHCPv4_MESSAGE_TYPE_OFFER);

    // Verify Incremented counters from table
    std::unordered_map<std::string, DHCPCounters> interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 1);
    EXPECT_EQ(interfacesCntrTable[interface].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_OFFER)->second], 1);

    // If initialization failed, the above would likely crash
    SUCCEED();
}

// Test counter incrementation
TEST_F(DHCPCounterTableTest, IncrementCounter) {
    const std::string interface = "Ethernet0";

    // Initialize and increment counters
    counterTable->initializeInterface(interface);

    // Increment RX counter multiple times
    for (int i = 0; i < 5; i++) {
        counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    }

    // Increment TX counter once
    counterTable->incrementCounter(interface, "TX", DHCPv4_MESSAGE_TYPE_ACK);

    // Verify Incremented counters from table
    std::unordered_map<std::string, DHCPCounters> interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 5);
    EXPECT_EQ(interfacesCntrTable[interface].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_ACK)->second], 1);

    // Negative case - other counters should NOT have incremented
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_REQUEST)->second], 0);
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_OFFER)->second], 0);
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_ACK)->second], 0);
    EXPECT_EQ(interfacesCntrTable[interface].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_DECLINE)->second], 0);
    EXPECT_EQ(interfacesCntrTable[interface].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_INFORM)->second], 0);
   
    SUCCEED();
}

// Test with uninitialized interface
TEST_F(DHCPCounterTableTest, AutoInitializeInterface) {
    const std::string interface = "Ethernet1";

    // Try to increment without explicitly initializing
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);

    // If auto-initialization works, this should succeed
    counterTable->incrementCounter(interface, "TX", DHCPv4_MESSAGE_TYPE_ACK);

    // Verify Incremented counters from table
    std::unordered_map<std::string, DHCPCounters> interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 1);
    EXPECT_EQ(interfacesCntrTable[interface].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_ACK)->second], 1);

    SUCCEED();
}

// Test interface removal
TEST_F(DHCPCounterTableTest, RemoveInterface) {
    const std::string interface = "Ethernet0";

    // Initialize and increment counters
    counterTable->initializeInterface(interface);
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    
    // Verify Incremented counters from table
    std::unordered_map<std::string, DHCPCounters> interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 1);

    // Remove the interface
    counterTable->removeInterface(interface);

    // Now add it again - if removal worked, this should reinitialize from scratch
    // and should not crash
    counterTable->initializeInterface(interface);

    // Increment the same counter and verify
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    // verify counter
    interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interface].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 1);

    SUCCEED();
}

// Test starting and stopping the DB update thread
TEST_F(DHCPCounterTableTest, StartStopDbUpdates) {
    // Start the DB update thread
    counterTable->startDbUpdates();

    // Sleep briefly to allow thread to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop the thread
    counterTable->stopDbUpdates();

    // If the start/stop mechanisms work correctly, this will complete without hanging
    SUCCEED();
}

// Integration test for database update loop
TEST_F(DHCPCounterTableTest, DBUpdateLoopIntegration) {
    const std::string interface = "Ethernet0";

    // Initialize and increment counters
    counterTable->initializeInterface(interface);

    // Add some counter increments
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    counterTable->incrementCounter(interface, "RX", DHCPv4_MESSAGE_TYPE_REQUEST);
    counterTable->incrementCounter(interface, "TX", DHCPv4_MESSAGE_TYPE_OFFER);
    counterTable->incrementCounter(interface, "TX", DHCPv4_MESSAGE_TYPE_ACK);

    // Start DB updates
    counterTable->startDbUpdates();

    // Let the update thread run briefly (less than the normal interval for test speed)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop the updates
    counterTable->stopDbUpdates();

    std::shared_ptr<swss::DBConnector> state_db = std::make_shared<swss::DBConnector> ("STATE_DB", 0);
    swss::Table cntrTable(state_db.get(), "DHCPV4_COUNTER_TABLE");
    std::vector<swss::FieldValueTuple> existing_rx_fields;
    std::vector<swss::FieldValueTuple> existing_tx_fields;
    cntrTable.get(interface+"|RX", existing_rx_fields);
    cntrTable.get(interface+"|TX", existing_tx_fields);

    //Verify Incremented Rx fields
    for (const auto& field : existing_rx_fields) {
	if ( (fvField(field)) == "Discover" || (fvField(field)) == "Request") {
	    EXPECT_EQ(std::stoi(fvValue(field)), 1);
	}
    }
    //Verify Incremented Tx fields
    for (const auto& field : existing_tx_fields) {
	if ( (fvField(field)) == "Offer" || (fvField(field)) == "Acknowledge") {
	    EXPECT_EQ(std::stoi(fvValue(field)), 1);
	}
    }

    SUCCEED();
}

// Test for handling multiple interfaces
TEST_F(DHCPCounterTableTest, MultipleInterfaces) {
    const std::vector<std::string> interfaces = {"Ethernet0", "Ethernet1", "Ethernet2"};

    // Initialize multiple interfaces
    for (const auto& intf : interfaces) {
        counterTable->initializeInterface(intf);
    }

    // Increment counters for different interfaces
    counterTable->incrementCounter(interfaces[0], "RX", DHCPv4_MESSAGE_TYPE_DISCOVER);
    counterTable->incrementCounter(interfaces[1], "RX", DHCPv4_MESSAGE_TYPE_REQUEST);
    counterTable->incrementCounter(interfaces[2], "TX", DHCPv4_MESSAGE_TYPE_ACK);

    // Verify Incremented counters from table
    std::unordered_map<std::string, DHCPCounters> interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interfaces[0]].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_DISCOVER)->second], 1);
    EXPECT_EQ(interfacesCntrTable[interfaces[1]].RX[counterMap.find(DHCPv4_MESSAGE_TYPE_REQUEST)->second], 1);
    EXPECT_EQ(interfacesCntrTable[interfaces[2]].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_ACK)->second], 1);
    
    // Remove one interface
    counterTable->removeInterface(interfaces[1]);

    // Try incrementing the removed interface (should auto-initialize)
    counterTable->incrementCounter(interfaces[1], "TX", DHCPv4_MESSAGE_TYPE_OFFER);
    // Verify incremented counter
    interfacesCntrTable = counterTable->getCountersData();
    EXPECT_EQ(interfacesCntrTable[interfaces[1]].TX[counterMap.find(DHCPv4_MESSAGE_TYPE_OFFER)->second], 1);

    SUCCEED();
}
