#include <catch2/catch_test_macros.hpp>

#include "core/types/Host.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/HostGroupRepository.hpp"

#include <chrono>
#include <filesystem>

using namespace netpulse::infra;
using namespace netpulse::core;

namespace {

class TestDatabase {
public:
    TestDatabase() : dbPath_(std::filesystem::temp_directory_path() / "netpulse_hostrepo_test.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
    }

    ~TestDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

Host createTestHost(const std::string& name, const std::string& address) {
    Host host;
    host.name = name;
    host.address = address;
    host.pingIntervalSeconds = 30;
    host.warningThresholdMs = 100;
    host.criticalThresholdMs = 500;
    host.status = HostStatus::Unknown;
    host.enabled = true;
    host.createdAt = std::chrono::system_clock::now();
    return host;
}

} // namespace

TEST_CASE("HostRepository insert operations", "[HostRepository][CRUD]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("Insert host with minimal fields") {
        Host host;
        host.name = "Minimal Host";
        host.address = "192.168.1.1";
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);
        REQUIRE(id > 0);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Minimal Host");
        REQUIRE(retrieved->address == "192.168.1.1");
        REQUIRE(retrieved->enabled == true);
        REQUIRE(retrieved->status == HostStatus::Unknown);
    }

    SECTION("Insert host with all fields") {
        Host host;
        host.name = "Full Host";
        host.address = "10.0.0.1";
        host.pingIntervalSeconds = 60;
        host.warningThresholdMs = 200;
        host.criticalThresholdMs = 1000;
        host.status = HostStatus::Up;
        host.enabled = false;
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);
        REQUIRE(id > 0);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Full Host");
        REQUIRE(retrieved->address == "10.0.0.1");
        REQUIRE(retrieved->pingIntervalSeconds == 60);
        REQUIRE(retrieved->warningThresholdMs == 200);
        REQUIRE(retrieved->criticalThresholdMs == 1000);
        REQUIRE(retrieved->status == HostStatus::Up);
        REQUIRE(retrieved->enabled == false);
    }

    SECTION("Insert host with group assignment") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group;
        group.name = "Test Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host = createTestHost("Grouped Host", "172.16.0.1");
        host.groupId = groupId;

        int64_t hostId = repo.insert(host);
        auto retrieved = repo.findById(hostId);

        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->groupId.has_value());
        REQUIRE(*retrieved->groupId == groupId);
    }

    SECTION("Insert multiple hosts returns unique IDs") {
        Host host1 = createTestHost("Host 1", "1.1.1.1");
        Host host2 = createTestHost("Host 2", "2.2.2.2");
        Host host3 = createTestHost("Host 3", "3.3.3.3");

        int64_t id1 = repo.insert(host1);
        int64_t id2 = repo.insert(host2);
        int64_t id3 = repo.insert(host3);

        REQUIRE(id1 > 0);
        REQUIRE(id2 > id1);
        REQUIRE(id3 > id2);
    }
}

TEST_CASE("HostRepository read operations", "[HostRepository][CRUD]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("findById returns host when exists") {
        Host host = createTestHost("FindById Host", "192.168.1.100");
        int64_t id = repo.insert(host);

        auto found = repo.findById(id);
        REQUIRE(found.has_value());
        REQUIRE(found->id == id);
        REQUIRE(found->name == "FindById Host");
    }

    SECTION("findById returns nullopt when not exists") {
        auto notFound = repo.findById(99999);
        REQUIRE_FALSE(notFound.has_value());
    }

    SECTION("findByAddress returns host when exists") {
        Host host = createTestHost("Address Host", "10.20.30.40");
        repo.insert(host);

        auto found = repo.findByAddress("10.20.30.40");
        REQUIRE(found.has_value());
        REQUIRE(found->address == "10.20.30.40");
        REQUIRE(found->name == "Address Host");
    }

    SECTION("findByAddress returns nullopt when not exists") {
        auto notFound = repo.findByAddress("255.255.255.255");
        REQUIRE_FALSE(notFound.has_value());
    }

    SECTION("findAll returns all hosts ordered by name") {
        Host hostC = createTestHost("C Host", "3.3.3.3");
        Host hostA = createTestHost("A Host", "1.1.1.1");
        Host hostB = createTestHost("B Host", "2.2.2.2");

        repo.insert(hostC);
        repo.insert(hostA);
        repo.insert(hostB);

        auto all = repo.findAll();
        REQUIRE(all.size() == 3);
        REQUIRE(all[0].name == "A Host");
        REQUIRE(all[1].name == "B Host");
        REQUIRE(all[2].name == "C Host");
    }

    SECTION("findAll returns empty vector when no hosts") {
        auto all = repo.findAll();
        REQUIRE(all.empty());
    }

    SECTION("findEnabled returns only enabled hosts") {
        Host enabled1 = createTestHost("Enabled 1", "1.1.1.1");
        enabled1.enabled = true;

        Host enabled2 = createTestHost("Enabled 2", "2.2.2.2");
        enabled2.enabled = true;

        Host disabled = createTestHost("Disabled", "3.3.3.3");
        disabled.enabled = false;

        repo.insert(enabled1);
        repo.insert(disabled);
        repo.insert(enabled2);

        auto enabledHosts = repo.findEnabled();
        REQUIRE(enabledHosts.size() == 2);
        for (const auto& host : enabledHosts) {
            REQUIRE(host.enabled == true);
        }
    }

    SECTION("findByGroupId returns hosts in specified group") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group1;
        group1.name = "Group 1";
        group1.createdAt = std::chrono::system_clock::now();
        int64_t groupId1 = groupRepo.insert(group1);

        HostGroup group2;
        group2.name = "Group 2";
        group2.createdAt = std::chrono::system_clock::now();
        int64_t groupId2 = groupRepo.insert(group2);

        Host hostInGroup1 = createTestHost("In Group 1", "1.1.1.1");
        hostInGroup1.groupId = groupId1;
        repo.insert(hostInGroup1);

        Host hostInGroup2 = createTestHost("In Group 2", "2.2.2.2");
        hostInGroup2.groupId = groupId2;
        repo.insert(hostInGroup2);

        Host ungrouped = createTestHost("Ungrouped", "3.3.3.3");
        repo.insert(ungrouped);

        auto group1Hosts = repo.findByGroupId(groupId1);
        REQUIRE(group1Hosts.size() == 1);
        REQUIRE(group1Hosts[0].name == "In Group 1");

        auto group2Hosts = repo.findByGroupId(groupId2);
        REQUIRE(group2Hosts.size() == 1);
        REQUIRE(group2Hosts[0].name == "In Group 2");
    }

    SECTION("findUngrouped returns hosts without group") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group;
        group.name = "Test Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host grouped = createTestHost("Grouped", "1.1.1.1");
        grouped.groupId = groupId;
        repo.insert(grouped);

        Host ungrouped1 = createTestHost("Ungrouped 1", "2.2.2.2");
        repo.insert(ungrouped1);

        Host ungrouped2 = createTestHost("Ungrouped 2", "3.3.3.3");
        repo.insert(ungrouped2);

        auto ungroupedHosts = repo.findUngrouped();
        REQUIRE(ungroupedHosts.size() == 2);
        for (const auto& host : ungroupedHosts) {
            REQUIRE_FALSE(host.groupId.has_value());
        }
    }

    SECTION("count returns correct number of hosts") {
        REQUIRE(repo.count() == 0);

        repo.insert(createTestHost("Host 1", "1.1.1.1"));
        REQUIRE(repo.count() == 1);

        repo.insert(createTestHost("Host 2", "2.2.2.2"));
        REQUIRE(repo.count() == 2);

        repo.insert(createTestHost("Host 3", "3.3.3.3"));
        REQUIRE(repo.count() == 3);
    }
}

TEST_CASE("HostRepository update operations", "[HostRepository][CRUD]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("update modifies host fields") {
        Host host = createTestHost("Original Name", "192.168.1.1");
        int64_t id = repo.insert(host);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());

        retrieved->name = "Updated Name";
        retrieved->address = "192.168.1.2";
        retrieved->pingIntervalSeconds = 120;
        retrieved->warningThresholdMs = 300;
        retrieved->criticalThresholdMs = 1500;
        retrieved->enabled = false;
        repo.update(*retrieved);

        auto updated = repo.findById(id);
        REQUIRE(updated.has_value());
        REQUIRE(updated->name == "Updated Name");
        REQUIRE(updated->address == "192.168.1.2");
        REQUIRE(updated->pingIntervalSeconds == 120);
        REQUIRE(updated->warningThresholdMs == 300);
        REQUIRE(updated->criticalThresholdMs == 1500);
        REQUIRE(updated->enabled == false);
    }

    SECTION("update preserves host ID") {
        Host host = createTestHost("Test Host", "10.0.0.1");
        int64_t originalId = repo.insert(host);

        auto retrieved = repo.findById(originalId);
        retrieved->name = "New Name";
        repo.update(*retrieved);

        auto updated = repo.findById(originalId);
        REQUIRE(updated.has_value());
        REQUIRE(updated->id == originalId);
    }

    SECTION("updateStatus changes only status") {
        Host host = createTestHost("Status Host", "192.168.1.1");
        host.status = HostStatus::Unknown;
        int64_t id = repo.insert(host);

        repo.updateStatus(id, HostStatus::Up);
        auto updated = repo.findById(id);
        REQUIRE(updated->status == HostStatus::Up);

        repo.updateStatus(id, HostStatus::Warning);
        updated = repo.findById(id);
        REQUIRE(updated->status == HostStatus::Warning);

        repo.updateStatus(id, HostStatus::Down);
        updated = repo.findById(id);
        REQUIRE(updated->status == HostStatus::Down);
    }

    SECTION("updateLastChecked sets timestamp") {
        Host host = createTestHost("LastChecked Host", "192.168.1.1");
        int64_t id = repo.insert(host);

        auto beforeUpdate = repo.findById(id);
        REQUIRE_FALSE(beforeUpdate->lastChecked.has_value());

        repo.updateLastChecked(id);

        auto afterUpdate = repo.findById(id);
        REQUIRE(afterUpdate->lastChecked.has_value());
    }

    SECTION("setHostGroup assigns host to group") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group;
        group.name = "New Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host = createTestHost("Ungrouped Host", "10.0.0.1");
        int64_t hostId = repo.insert(host);

        auto beforeAssign = repo.findById(hostId);
        REQUIRE_FALSE(beforeAssign->groupId.has_value());

        repo.setHostGroup(hostId, groupId);

        auto afterAssign = repo.findById(hostId);
        REQUIRE(afterAssign->groupId.has_value());
        REQUIRE(*afterAssign->groupId == groupId);
    }

    SECTION("setHostGroup removes host from group with nullopt") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group;
        group.name = "Temp Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host = createTestHost("Grouped Host", "10.0.0.1");
        host.groupId = groupId;
        int64_t hostId = repo.insert(host);

        auto beforeRemove = repo.findById(hostId);
        REQUIRE(beforeRemove->groupId.has_value());

        repo.setHostGroup(hostId, std::nullopt);

        auto afterRemove = repo.findById(hostId);
        REQUIRE_FALSE(afterRemove->groupId.has_value());
    }

    SECTION("setHostGroup moves host between groups") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group1;
        group1.name = "Group 1";
        group1.createdAt = std::chrono::system_clock::now();
        int64_t groupId1 = groupRepo.insert(group1);

        HostGroup group2;
        group2.name = "Group 2";
        group2.createdAt = std::chrono::system_clock::now();
        int64_t groupId2 = groupRepo.insert(group2);

        Host host = createTestHost("Moving Host", "10.0.0.1");
        host.groupId = groupId1;
        int64_t hostId = repo.insert(host);

        auto inGroup1 = repo.findById(hostId);
        REQUIRE(*inGroup1->groupId == groupId1);

        repo.setHostGroup(hostId, groupId2);

        auto inGroup2 = repo.findById(hostId);
        REQUIRE(*inGroup2->groupId == groupId2);
    }

    SECTION("update can change group via full update") {
        HostGroupRepository groupRepo(testDb.get());

        HostGroup group;
        group.name = "Target Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host = createTestHost("Update Group Host", "10.0.0.1");
        int64_t hostId = repo.insert(host);

        auto retrieved = repo.findById(hostId);
        retrieved->groupId = groupId;
        repo.update(*retrieved);

        auto updated = repo.findById(hostId);
        REQUIRE(updated->groupId.has_value());
        REQUIRE(*updated->groupId == groupId);
    }
}

TEST_CASE("HostRepository delete operations", "[HostRepository][CRUD]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("remove deletes host by ID") {
        Host host = createTestHost("To Delete", "192.168.1.1");
        int64_t id = repo.insert(host);

        REQUIRE(repo.findById(id).has_value());
        REQUIRE(repo.count() == 1);

        repo.remove(id);

        REQUIRE_FALSE(repo.findById(id).has_value());
        REQUIRE(repo.count() == 0);
    }

    SECTION("remove non-existent host does not throw") {
        REQUIRE_NOTHROW(repo.remove(99999));
    }

    SECTION("remove one host does not affect others") {
        Host host1 = createTestHost("Host 1", "1.1.1.1");
        Host host2 = createTestHost("Host 2", "2.2.2.2");
        Host host3 = createTestHost("Host 3", "3.3.3.3");

        int64_t id1 = repo.insert(host1);
        int64_t id2 = repo.insert(host2);
        int64_t id3 = repo.insert(host3);

        REQUIRE(repo.count() == 3);

        repo.remove(id2);

        REQUIRE(repo.count() == 2);
        REQUIRE(repo.findById(id1).has_value());
        REQUIRE_FALSE(repo.findById(id2).has_value());
        REQUIRE(repo.findById(id3).has_value());
    }
}

TEST_CASE("HostRepository edge cases", "[HostRepository][CRUD]") {
    TestDatabase testDb;
    HostRepository repo(testDb.get());

    SECTION("Host with empty name can be inserted") {
        Host host;
        host.name = "";
        host.address = "192.168.1.1";
        host.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name.empty());
    }

    SECTION("Host with special characters in name") {
        Host host = createTestHost("Test's \"Host\" <with> Special & Characters", "192.168.1.1");

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Test's \"Host\" <with> Special & Characters");
    }

    SECTION("Host with IPv6 address") {
        Host host = createTestHost("IPv6 Host", "2001:db8::1");

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->address == "2001:db8::1");
    }

    SECTION("Host with hostname instead of IP") {
        Host host = createTestHost("DNS Host", "example.com");

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->address == "example.com");
    }

    SECTION("Host with zero thresholds") {
        Host host = createTestHost("Zero Threshold Host", "192.168.1.1");
        host.pingIntervalSeconds = 0;
        host.warningThresholdMs = 0;
        host.criticalThresholdMs = 0;

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->pingIntervalSeconds == 0);
        REQUIRE(retrieved->warningThresholdMs == 0);
        REQUIRE(retrieved->criticalThresholdMs == 0);
    }

    SECTION("Host with large thresholds") {
        Host host = createTestHost("Large Threshold Host", "192.168.1.1");
        host.pingIntervalSeconds = 86400;
        host.warningThresholdMs = 100000;
        host.criticalThresholdMs = 500000;

        int64_t id = repo.insert(host);
        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->pingIntervalSeconds == 86400);
        REQUIRE(retrieved->warningThresholdMs == 100000);
        REQUIRE(retrieved->criticalThresholdMs == 500000);
    }

    SECTION("Multiple hosts with same name but different addresses") {
        Host host1 = createTestHost("Same Name", "1.1.1.1");
        Host host2 = createTestHost("Same Name", "2.2.2.2");

        int64_t id1 = repo.insert(host1);
        int64_t id2 = repo.insert(host2);

        REQUIRE(id1 != id2);
        REQUIRE(repo.count() == 2);

        auto retrieved1 = repo.findById(id1);
        auto retrieved2 = repo.findById(id2);
        REQUIRE(retrieved1->name == retrieved2->name);
        REQUIRE(retrieved1->address != retrieved2->address);
    }

    SECTION("All HostStatus values can be stored and retrieved") {
        Host hostUnknown = createTestHost("Unknown Host", "1.1.1.1");
        hostUnknown.status = HostStatus::Unknown;
        int64_t idUnknown = repo.insert(hostUnknown);

        Host hostUp = createTestHost("Up Host", "2.2.2.2");
        hostUp.status = HostStatus::Up;
        int64_t idUp = repo.insert(hostUp);

        Host hostWarning = createTestHost("Warning Host", "3.3.3.3");
        hostWarning.status = HostStatus::Warning;
        int64_t idWarning = repo.insert(hostWarning);

        Host hostDown = createTestHost("Down Host", "4.4.4.4");
        hostDown.status = HostStatus::Down;
        int64_t idDown = repo.insert(hostDown);

        REQUIRE(repo.findById(idUnknown)->status == HostStatus::Unknown);
        REQUIRE(repo.findById(idUp)->status == HostStatus::Up);
        REQUIRE(repo.findById(idWarning)->status == HostStatus::Warning);
        REQUIRE(repo.findById(idDown)->status == HostStatus::Down);
    }
}
