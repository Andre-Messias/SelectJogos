#pragma once

#include <unordered_set>
#include <random>
#include <mutex>

/// @brief Thread-safe generator for unique random IDs within [1000, 999999].
class IdGenerator {
public:
    /// @brief Constructs an IdGenerator with a random seed.
    IdGenerator();

    /// @brief Generates and reserves a unique random ID.
    /// @return A valid unique ID, or -1 if the generator reached maximum capacity.
    int AcquireId();

    /// @brief Releases a previously acquired ID back to the pool.
    /// @param id The ID to release.
    void ReleaseId(int id);

private:
    /// @brief Defines the minimum valid ID for generation.
    static constexpr int MIN_ID = 1000;
    /// @brief Defines the maximum valid ID for generation.
    static constexpr int MAX_ID = 999999;
    /// @brief Defines the maximum number of unique IDs that can be generated.
    static constexpr size_t MAX_CAPACITY = static_cast<size_t>(MAX_ID - MIN_ID + 1);

    /// @brief Random number generator for producing candidate IDs.
    std::mt19937 _rng;
    /// @brief Set of currently active IDs to ensure uniqueness.
    std::unordered_set<int> _active_ids;
    /// @brief Mutex to protect access to the active ID set, ensuring thread safety.
    std::mutex _id_mutex;
};