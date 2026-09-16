#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_zvec_embedding_index.hpp"

namespace {

void vqec_vision_ai_unit_zvitst_check_private_storage(
    const std::string& _volatile_root,
    const vqec::vision::ai::embedding_index_config& _config, unsigned& _failures) {
    using namespace vqec::vision::ai;
    const auto check = [&_failures](bool _condition) {
        if (!_condition) {
            ++_failures;
        }
    };
    std::string pattern = _volatile_root + "/vqec_vision_zvec_private_XXXXXX";
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    const char* created = ::mkdtemp(writable.data());
    if (created == nullptr) {
        ++_failures;
        return;
    }
    const std::filesystem::path parent(created);
    const auto collection = parent / "collection";
    std::filesystem::permissions(parent, std::filesystem::perms::owner_all |
        std::filesystem::perms::group_read | std::filesystem::perms::group_exec);
    {
        zvec_embedding_index exposed(collection.string());
        check(exposed.vqec_vision_ai_ports_emidx_configure(_config).code_ ==
            status_code::unauthorized);
        check(!std::filesystem::exists(collection));
    }
    std::filesystem::permissions(parent, std::filesystem::perms::owner_all);
    const auto alias = parent / "symlink";
    std::filesystem::create_directory_symlink(parent, alias);
    {
        zvec_embedding_index symlink_parent((alias / "collection").string());
        check(symlink_parent.vqec_vision_ai_ports_emidx_configure(_config).code_ ==
            status_code::unauthorized);
        zvec_embedding_index symlink_leaf(alias.string());
        check(symlink_leaf.vqec_vision_ai_ports_emidx_configure(_config).code_ ==
            status_code::unauthorized);
        zvec_embedding_index relative("relative_collection");
        check(relative.vqec_vision_ai_ports_emidx_configure(_config).code_ ==
            status_code::invalid_argument);
    }
    std::filesystem::remove(alias);
    const auto moved_parent = std::filesystem::path(parent.string() + "_moved");
    {
        zvec_embedding_index private_index(collection.string());
        const auto configured = private_index.vqec_vision_ai_ports_emidx_configure(_config);
        check(configured.code_ == status_code::ok);
        if (configured.code_ == status_code::ok) {
            check(std::filesystem::status(collection).permissions() ==
                std::filesystem::perms::owner_all);
            // The vendor path must continue to address the pinned original directory.
            std::filesystem::rename(parent, moved_parent);
            check(private_index.vqec_vision_ai_ports_emidx_upsert(
                {1, "synthetic_private", {1.0F, 0.0F}}, 1, 2).code_ == status_code::ok);
            embedding_result query;
            query.model_id_ = _config.model_id_;
            query.model_version_ = _config.model_version_;
            query.values_ = {1.0F, 0.0F};
            query.is_l2_normalized_ = true;
            embedding_search_result result;
            result.matches_.reserve(_config.max_results_);
            check(private_index.vqec_vision_ai_ports_emidx_search(
                query, 2, 1, 0.5F, result).code_ == status_code::ok);
            check(result.matches_.size() == 1);
        }
    }
    check(!std::filesystem::exists(moved_parent / "collection"));
    std::filesystem::remove_all(parent);
    std::filesystem::remove_all(moved_parent);
}

}  // namespace

// Independent synthetic vectors verify backend distance conversion and mutation visibility.
// The runner supplies an unused collection path; never use a real gallery for this test.
int main(int _argc, char** _argv) {
    using namespace vqec::vision::ai;
    if (_argc != 3 || std::filesystem::exists(_argv[1])) {
        return 2;
    }
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    embedding_index_config config;
    config.model_id_ = "synthetic_embedding";
    config.model_version_ = "1";
    config.dimensions_ = 2;
    config.capacity_ = 3;
    config.max_results_ = 3;
    config.initial_revision_ = 1;
    vqec_vision_ai_unit_zvitst_check_private_storage(_argv[2], config, failures);
    {
        zvec_embedding_index unqualified(_argv[1]);
        check(unqualified.vqec_vision_ai_ports_emidx_configure(config).code_ ==
            status_code::unauthorized);
        check(!std::filesystem::exists(_argv[1]));
    }
    {
        // Production recovery starts with a fresh derived collection.
        zvec_embedding_index initial(_argv[1],
            zvec_existing_collection_policy::rebuild,
            zvec_storage_policy::synthetic_filesystem_fixture);
        const auto opened = initial.vqec_vision_ai_ports_emidx_configure(config);
        if (opened.code_ != status_code::ok) {
            std::cerr << "Zvec fresh rebuild configure failed: "
                      << opened.message_ << '\n';
        }
        check(opened.code_ == status_code::ok);
    }
    std::filesystem::remove_all(_argv[1]);
    {
        zvec_embedding_index index(_argv[1], zvec_existing_collection_policy::reject,
            zvec_storage_policy::synthetic_filesystem_fixture);
        const auto configured = index.vqec_vision_ai_ports_emidx_configure(config);
        check(configured.code_ == status_code::ok);
        if (configured.code_ == status_code::ok) {
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {1, "synthetic_a", {1.0F, 0.0F}}, 1, 2).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {2, "synthetic_b", {0.0F, 1.0F}}, 2, 3).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_upsert(
                {3, "synthetic_c", {-1.0F, 0.0F}}, 3, 4).code_ == status_code::ok);
            embedding_result query;
            query.model_id_ = config.model_id_;
            query.model_version_ = config.model_version_;
            query.values_ = {1.0F, 0.0F};
            query.is_l2_normalized_ = true;
            embedding_search_result result;
            result.matches_.reserve(config.max_results_);
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 4, 3, 0.5F, result).code_ == status_code::ok);
            check(result.matches_.size() == 1);
            if (result.matches_.size() == 1) {
                check(result.matches_[0].record_id_ == 1);
                check(result.matches_[0].subject_ref_ == "synthetic_a");
                check(result.matches_[0].similarity_ > 0.99F);
            }
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 3, 3, 0.5F, result).code_ == status_code::invalid_state);
            check(index.vqec_vision_ai_ports_emidx_remove(1, 4, 5).code_ == status_code::ok);
            check(index.vqec_vision_ai_ports_emidx_search(
                query, 5, 3, 0.5F, result).code_ == status_code::ok);
            check(result.matches_.empty());
        }
    }
    {
        zvec_embedding_index rebuilt(_argv[1],
            zvec_existing_collection_policy::rebuild,
            zvec_storage_policy::synthetic_filesystem_fixture);
        embedding_index_config config;
        config.model_id_ = "synthetic_embedding";
        config.model_version_ = "1";
        config.dimensions_ = 2;
        config.capacity_ = 3;
        config.max_results_ = 3;
        config.initial_revision_ = 9;
        const auto rebuilt_status =
            rebuilt.vqec_vision_ai_ports_emidx_configure(config);
        if (rebuilt_status.code_ != status_code::ok) {
            std::cerr << "Zvec rebuild configure failed: "
                      << rebuilt_status.message_ << '\n';
        }
        check(rebuilt_status.code_ == status_code::ok);
        embedding_result query;
        query.model_id_ = config.model_id_;
        query.model_version_ = config.model_version_;
        query.values_ = {1.0F, 0.0F};
        query.is_l2_normalized_ = true;
        embedding_search_result result;
        result.matches_.reserve(config.max_results_);
        const auto search_status = rebuilt.vqec_vision_ai_ports_emidx_search(
            query, 9, 1, -1.0F, result);
        if (search_status.code_ != status_code::ok) {
            std::cerr << "Zvec rebuilt search failed: "
                      << search_status.message_ << '\n';
        }
        check(search_status.code_ == status_code::ok);
        check(result.matches_.empty());
    }
    // This directory was absent before this test and contains only synthetic records.
    std::filesystem::remove_all(_argv[1]);
    std::cout << "Zvec integration failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
