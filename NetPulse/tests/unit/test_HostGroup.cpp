#include <catch2/catch_test_macros.hpp>

#include "core/types/HostGroup.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/HostGroupRepository.hpp"
#include "infrastructure/database/HostRepository.hpp"

#include <filesystem>

using namespace netpulse::infra;
using namespace netpulse::core;

class TestDatabase {
public:
    TestDatabase() : dbPath_(std::filesystem::temp_directory_path() / "netpulse_hostgroup_test.db") {
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

TEST_CASE("HostGroup validation", "[HostGroup]") {
    SECTION("Valid group") {
        HostGroup group;
        group.name = "Production Servers";
        group.description = "All production servers";
        REQUIRE(group.isValid());
    }

    SECTION("Invalid group - empty name") {
        HostGroup group;
        group.name = "";
        group.description = "Description without name";
        REQUIRE_FALSE(group.isValid());
    }

    SECTION("Valid group - no description") {
        HostGroup group;
        group.name = "Test Group";
        REQUIRE(group.isValid());
    }
}

TEST_CASE("HostGroup equality", "[HostGroup]") {
    HostGroup group1;
    group1.id = 1;
    group1.name = "Test";
    group1.description = "Description";

    HostGroup group2 = group1;
    REQUIRE(group1 == group2);

    group2.name = "Different";
    REQUIRE_FALSE(group1 == group2);
}

TEST_CASE("HostGroupRepository operations", "[Database][HostGroupRepository]") {
    TestDatabase testDb;
    HostGroupRepository repo(testDb.get());

    SECTION("Insert and retrieve group") {
        HostGroup group;
        group.name = "Test Group";
        group.description = "Test Description";
        group.createdAt = std::chrono::system_clock::now();

        int64_t id = repo.insert(group);
        REQUIRE(id > 0);

        auto retrieved = repo.findById(id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "Test Group");
        REQUIRE(retrieved->description == "Test Description");
        REQUIRE_FALSE(retrieved->parentId.has_value());
    }

    SECTION("Insert nested groups") {
        HostGroup parent;
        parent.name = "Parent Group";
        parent.createdAt = std::chrono::system_clock::now();
        int64_t parentId = repo.insert(parent);

        HostGroup child;
        child.name = "Child Group";
        child.parentId = parentId;
        child.createdAt = std::chrono::system_clock::now();
        int64_t childId = repo.insert(child);

        auto retrieved = repo.findById(childId);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->parentId.has_value());
        REQUIRE(*retrieved->parentId == parentId);
    }

    SECTION("Update group") {
        HostGroup group;
        group.name = "Original Name";
        group.createdAt = std::chrono::system_clock::now();
        int64_t id = repo.insert(group);

        auto retrieved = repo.findById(id);
        retrieved->name = "Updated Name";
        retrieved->description = "New Description";
        repo.update(*retrieved);

        auto updated = repo.findById(id);
        REQUIRE(updated->name == "Updated Name");
        REQUIRE(updated->description == "New Description");
    }

    SECTION("Remove group") {
        HostGroup group;
        group.name = "To Remove";
        group.createdAt = std::chrono::system_clock::now();
        int64_t id = repo.insert(group);

        REQUIRE(repo.findById(id).has_value());
        repo.remove(id);
        REQUIRE_FALSE(repo.findById(id).has_value());
    }

    SECTION("Find all groups") {
        HostGroup group1;
        group1.name = "Group 1";
        group1.createdAt = std::chrono::system_clock::now();

        HostGroup group2;
        group2.name = "Group 2";
        group2.createdAt = std::chrono::system_clock::now();

        repo.insert(group1);
        repo.insert(group2);

        auto all = repo.findAll();
        REQUIRE(all.size() == 2);
        REQUIRE(repo.count() == 2);
    }

    SECTION("Find root groups") {
        HostGroup root1;
        root1.name = "Root 1";
        root1.createdAt = std::chrono::system_clock::now();
        int64_t rootId = repo.insert(root1);

        HostGroup root2;
        root2.name = "Root 2";
        root2.createdAt = std::chrono::system_clock::now();
        repo.insert(root2);

        HostGroup child;
        child.name = "Child";
        child.parentId = rootId;
        child.createdAt = std::chrono::system_clock::now();
        repo.insert(child);

        auto rootGroups = repo.findRootGroups();
        REQUIRE(rootGroups.size() == 2);
    }

    SECTION("Find child groups") {
        HostGroup parent;
        parent.name = "Parent";
        parent.createdAt = std::chrono::system_clock::now();
        int64_t parentId = repo.insert(parent);

        HostGroup child1;
        child1.name = "Child 1";
        child1.parentId = parentId;
        child1.createdAt = std::chrono::system_clock::now();
        repo.insert(child1);

        HostGroup child2;
        child2.name = "Child 2";
        child2.parentId = parentId;
        child2.createdAt = std::chrono::system_clock::now();
        repo.insert(child2);

        auto children = repo.findByParentId(parentId);
        REQUIRE(children.size() == 2);
    }
}

TEST_CASE("Host-Group relationship", "[Database][HostRepository][HostGroupRepository]") {
    TestDatabase testDb;
    HostRepository hostRepo(testDb.get());
    HostGroupRepository groupRepo(testDb.get());

    SECTION("Assign host to group") {
        // Create a group
        HostGroup group;
        group.name = "Test Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        // Create a host
        Host host;
        host.name = "Test Host";
        host.address = "192.168.1.1";
        host.createdAt = std::chrono::system_clock::now();
        int64_t hostId = hostRepo.insert(host);

        // Initially no group
        auto retrieved = hostRepo.findById(hostId);
        REQUIRE_FALSE(retrieved->groupId.has_value());

        // Assign to group
        hostRepo.setHostGroup(hostId, groupId);
        retrieved = hostRepo.findById(hostId);
        REQUIRE(retrieved->groupId.has_value());
        REQUIRE(*retrieved->groupId == groupId);
    }

    SECTION("Find hosts by group") {
        HostGroup group;
        group.name = "Server Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host1;
        host1.name = "Server 1";
        host1.address = "10.0.0.1";
        host1.groupId = groupId;
        host1.createdAt = std::chrono::system_clock::now();
        hostRepo.insert(host1);

        Host host2;
        host2.name = "Server 2";
        host2.address = "10.0.0.2";
        host2.groupId = groupId;
        host2.createdAt = std::chrono::system_clock::now();
        hostRepo.insert(host2);

        Host ungrouped;
        ungrouped.name = "Standalone";
        ungrouped.address = "10.0.0.3";
        ungrouped.createdAt = std::chrono::system_clock::now();
        hostRepo.insert(ungrouped);

        auto inGroup = hostRepo.findByGroupId(groupId);
        REQUIRE(inGroup.size() == 2);

        auto ungroupedHosts = hostRepo.findUngrouped();
        REQUIRE(ungroupedHosts.size() == 1);
        REQUIRE(ungroupedHosts[0].name == "Standalone");
    }

    SECTION("Remove group clears host group_id") {
        HostGroup group;
        group.name = "To Delete";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host;
        host.name = "Orphan Host";
        host.address = "10.0.0.10";
        host.groupId = groupId;
        host.createdAt = std::chrono::system_clock::now();
        int64_t hostId = hostRepo.insert(host);

        // Delete group (ON DELETE SET NULL)
        groupRepo.remove(groupId);

        // Host should still exist but without group
        auto retrieved = hostRepo.findById(hostId);
        REQUIRE(retrieved.has_value());
        REQUIRE_FALSE(retrieved->groupId.has_value());
    }

    SECTION("Unassign host from group") {
        HostGroup group;
        group.name = "Temp Group";
        group.createdAt = std::chrono::system_clock::now();
        int64_t groupId = groupRepo.insert(group);

        Host host;
        host.name = "Moving Host";
        host.address = "10.0.0.20";
        host.groupId = groupId;
        host.createdAt = std::chrono::system_clock::now();
        int64_t hostId = hostRepo.insert(host);

        // Unassign from group
        hostRepo.setHostGroup(hostId, std::nullopt);

        auto retrieved = hostRepo.findById(hostId);
        REQUIRE_FALSE(retrieved->groupId.has_value());
    }
}
