#include "CloudDeleteQueue.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/system/error_code.hpp>

#include "libslic3r/InstanceLock.hpp"
#include "libslic3r/Preset.hpp"

namespace Slic3r {

namespace {

CloudDeleteQueue::Clock::duration retry_wait(int failures)
{
    using namespace std::chrono_literals;
    constexpr auto first   = 1min;
    constexpr auto longest = 60min;
    // Capped before shifting, so a long run of failures can't overflow the shift.
    const int doublings = std::min(failures - 1, 6);
    return std::min<CloudDeleteQueue::Clock::duration>(first * (1 << doublings), longest);
}

// Value of one "key = value" line of a .info file, or "" when the key is missing.
std::string read_info_value(const std::string& info_file_path, const std::string& key)
{
    boost::nowide::ifstream file(info_file_path);
    if (!file.is_open())
        return "";

    std::string line;
    while (std::getline(file, line)) {
        const size_t pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        std::string name = line.substr(0, pos);
        boost::algorithm::trim(name);
        if (name != key)
            continue;
        std::string value = line.substr(pos + 1);
        boost::algorithm::trim(value);
        return value;
    }
    return "";
}

} // namespace

void CloudDeleteQueue::start_session(const std::string& user_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (user_id == m_user_id)
        return;
    m_user_id = user_id;
    ++m_generation;
    m_pending.clear();
}

uint64_t CloudDeleteQueue::generation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generation;
}

bool CloudDeleteQueue::is_current(uint64_t generation) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return generation == m_generation;
}

bool CloudDeleteQueue::add(uint64_t generation, const std::string& setting_id, const std::string& info_path, const std::string& owner)
{
    if (setting_id.empty())
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (generation != m_generation)
        return false;
    m_pending.try_emplace(setting_id, Pending{Entry{setting_id, info_path, owner.empty() ? m_user_id : owner}});
    return true;
}

CloudDeleteQueue::Batch CloudDeleteQueue::due(Clock::time_point now) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Batch batch{m_generation, {}};
    if (m_user_id.empty())
        return batch;
    for (const auto& [setting_id, pending] : m_pending)
        if (pending.entry.owner == m_user_id && pending.not_before <= now)
            batch.entries.push_back(pending.entry);
    return batch;
}

std::optional<std::string> CloudDeleteQueue::finish(uint64_t generation, const std::string& setting_id, bool gone, Clock::time_point now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (generation != m_generation)
        return std::nullopt;
    auto it = m_pending.find(setting_id);
    if (it == m_pending.end())
        return std::nullopt;
    if (gone) {
        std::string info_path = std::move(it->second.entry.info_path);
        m_pending.erase(it);
        return info_path;
    }
    Pending& pending  = it->second;
    pending.not_before = now + retry_wait(++pending.failures);
    return std::nullopt;
}

void CloudDeleteQueue::send_due(const std::function<Clock::time_point()>& now,
                                const std::function<std::string()>&       current_user,
                                const std::function<bool(const Entry&)>&  send,
                                const std::function<void(const Entry&)>&  on_gone)
{
    const Batch batch = due(now());
    for (const Entry& entry : batch.entries) {
        if (!is_current(batch.generation) || current_user() != entry.owner)
            return;
        const bool gone = send(entry);
        if (current_user() != entry.owner)
            return;
        if (finish(batch.generation, entry.setting_id, gone, now()))
            on_gone(entry);
    }
}

size_t CloudDeleteQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pending.size();
}

bool queue_orphaned_info_files(CloudDeleteQueue& queue, uint64_t generation, const std::string& user_dir)
{
    namespace fs = boost::filesystem;
    for (const char* type : {PRESET_PRINT_NAME, PRESET_FILAMENT_NAME, PRESET_PRINTER_NAME}) {
        const fs::path type_dir = fs::path(user_dir) / type;
        if (!fs::exists(type_dir))
            continue;

        // The error_code iterator logs and skips a transient directory-read failure (e.g. macOS readdir
        // returning ENOTSUP) instead of throwing on the background sync thread this runs on.
        boost::system::error_code ec;
        for (fs::directory_iterator it(type_dir, ec), end; !ec && it != end; it.increment(ec)) {
            const fs::path info_file = it->path();
            if (info_file.extension() != ".info")
                continue;
            fs::path preset_file = info_file;
            preset_file.replace_extension(".json");

            // Read under the lock, so a remove_files() in another instance is seen whole or not at all;
            // the queue's own mutex is taken after the lock is released.
            std::string setting_id;
            std::string owner;
            {
                InstanceLock instance_lock(user_presets_lock_path());
                if (!fs::exists(preset_file)) {
                    setting_id = read_info_value(info_file.string(), "setting_id");
                    owner      = read_info_value(info_file.string(), "user_id");
                }
            }
            if (setting_id.empty())
                continue;
            if (!queue.add(generation, setting_id, info_file.string(), owner)) {
                BOOST_LOG_TRIVIAL(info) << "queue_orphaned_info_files: account changed, scan of " << user_dir << " abandoned";
                return false;
            }
            BOOST_LOG_TRIVIAL(info) << "Found orphaned .info file on startup: " << info_file.string();
        }
        if (ec)
            BOOST_LOG_TRIVIAL(warning) << "queue_orphaned_info_files: failed to scan " << type_dir.string() << ": " << ec.message();
    }
    return true;
}

void remove_info_file(const std::string& info_path)
{
    if (info_path.empty() || !boost::filesystem::exists(boost::filesystem::path(info_path)))
        return;
    InstanceLock instance_lock(user_presets_lock_path());
    boost::nowide::remove(info_path.c_str());
    BOOST_LOG_TRIVIAL(info) << "Deleted .info file after cloud confirmation: " << info_path;
}

} // namespace Slic3r
