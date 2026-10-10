#pragma once
#include "stereo_utility_dsp.hpp"
#include <atomic>
#include <mutex>
namespace studiohub::stereo_utility {
// State I/O may lock its own mutex. The audio callback never takes that mutex.
class StateExchange {
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<double> pendingGain_{0},pendingBalance_{0},gain_{0},balance_{0};
    std::atomic<bool> pendingBypass_{false},bypass_{false};
    std::atomic<std::uint64_t> epoch_{0},applied_{0};
    mutable std::mutex stateIo_;
public:
    void publish(State value) {
        std::lock_guard guard(stateIo_);
        const auto epoch=epoch_.load(std::memory_order_relaxed);
        epoch_.store(epoch+1,std::memory_order_seq_cst);
        pendingGain_.store(value.gainDb,std::memory_order_seq_cst);
        pendingBalance_.store(value.balance,std::memory_order_seq_cst);
        pendingBypass_.store(value.bypass,std::memory_order_seq_cst);
        epoch_.store(epoch+2,std::memory_order_seq_cst);
    }
    bool pending(State& value,std::uint64_t& version) noexcept {
        version=epoch_.load(std::memory_order_seq_cst);
        if((version&1)||version==applied_.load(std::memory_order_acquire))return false;
        value={pendingGain_.load(std::memory_order_seq_cst),pendingBalance_.load(std::memory_order_seq_cst),pendingBypass_.load(std::memory_order_seq_cst)};
        return epoch_.load(std::memory_order_seq_cst)==version;
    }
    void acknowledge(State value,std::uint64_t version) noexcept {cache(value);applied_.store(version,std::memory_order_release);}
    void cache(State value) noexcept {
        gain_.store(value.gainDb,std::memory_order_relaxed);balance_.store(value.balance,std::memory_order_relaxed);bypass_.store(value.bypass,std::memory_order_relaxed);
    }
    State requested() const {
        std::lock_guard guard(stateIo_);
        if(epoch_.load(std::memory_order_acquire)!=applied_.load(std::memory_order_acquire))
            return {pendingGain_.load(),pendingBalance_.load(),pendingBypass_.load()};
        return {gain_.load(std::memory_order_relaxed),balance_.load(std::memory_order_relaxed),bypass_.load(std::memory_order_relaxed)};
    }
};
}
