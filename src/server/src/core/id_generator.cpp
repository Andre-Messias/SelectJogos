#include "id_generator.hpp"

IdGenerator::IdGenerator() : _rng(std::random_device{}()) {}

int IdGenerator::AcquireId() {
    std::lock_guard<std::mutex> lock(_id_mutex);

    if (_active_ids.size() >= MAX_CAPACITY) {
        return -1;
    }

    std::uniform_int_distribution<int> dist(MIN_ID, MAX_ID);

    for (int attempt = 0; attempt < 100; ++attempt) {
        int candidate_id = dist(_rng);
        if (_active_ids.find(candidate_id) == _active_ids.end()) {
            _active_ids.insert(candidate_id);
            return candidate_id;
        }
    }

    int start_id = dist(_rng);
    for (size_t i = 0; i < MAX_CAPACITY; ++i) {
        int candidate_id = MIN_ID + static_cast<int>((start_id - MIN_ID + i) % MAX_CAPACITY);
        if (_active_ids.find(candidate_id) == _active_ids.end()) {
            _active_ids.insert(candidate_id);
            return candidate_id;
        }
    }

    return -1;
}

void IdGenerator::ReleaseId(int id) {
    std::lock_guard<std::mutex> lock(_id_mutex);
    _active_ids.erase(id);
}