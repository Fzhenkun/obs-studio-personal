/******************************************************************************
    Copyright (C) 2026 OBS Studio contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
******************************************************************************/

#include "ThreadHangMonitor.hpp"

#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace OBS {

namespace {

int64_t now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

} // namespace

ThreadHeartbeat::ThreadHeartbeat(std::string threadName) : name(std::move(threadName)), lastBeat(now())
{
#ifdef _WIN32
	nativeThreadId = GetCurrentThreadId();
#endif
}

ThreadHangMonitor::ThreadHangMonitor(Config config, HangCallback callback)
	: config_(config), callback_(std::move(callback))
{
	config_.checkInterval = std::max(config_.checkInterval, std::chrono::milliseconds(1));
	config_.hangThreshold = std::max(config_.hangThreshold, config_.checkInterval);
}

ThreadHangMonitor::~ThreadHangMonitor()
{
	stop();
}

ThreadHangMonitor::Heartbeat ThreadHangMonitor::registerThread(std::string name)
{
	auto heartbeat = std::make_shared<ThreadHeartbeat>(std::move(name));
	std::lock_guard lock(heartbeatsMutex_);
	heartbeats_.emplace_back(heartbeat);
	return heartbeat;
}

void ThreadHangMonitor::beat(const Heartbeat &heartbeat)
{
	if (!heartbeat) {
		return;
	}

	heartbeat->lastBeat.store(now(), std::memory_order_release);
	heartbeat->hangReported.store(false, std::memory_order_release);
}

void ThreadHangMonitor::start()
{
	std::lock_guard lock(stateMutex_);
	if (thread_.joinable()) {
		return;
	}

	stopping_ = false;
	thread_ = std::thread(&ThreadHangMonitor::run, this);
}

void ThreadHangMonitor::stop()
{
	{
		std::lock_guard lock(stateMutex_);
		if (!thread_.joinable()) {
			return;
		}
		stopping_ = true;
	}
	stateChanged_.notify_one();
	thread_.join();
}

void ThreadHangMonitor::run()
{
	std::unique_lock lock(stateMutex_);
	while (!stopping_) {
		stateChanged_.wait_for(lock, config_.checkInterval, [this] { return stopping_; });
		if (stopping_) {
			break;
		}

		const int64_t currentTime = now();
		std::vector<Heartbeat> activeHeartbeats;
		{
			std::lock_guard heartbeatLock(heartbeatsMutex_);
			heartbeats_.erase(std::remove_if(heartbeats_.begin(), heartbeats_.end(),
							 [](const auto &heartbeat) { return heartbeat.expired(); }),
					 heartbeats_.end());
			for (const auto &heartbeat : heartbeats_) {
				if (auto active = heartbeat.lock()) {
					activeHeartbeats.emplace_back(std::move(active));
				}
			}
		}

		lock.unlock();
		for (const auto &heartbeat : activeHeartbeats) {
			const auto elapsed =
				std::chrono::milliseconds(currentTime - heartbeat->lastBeat.load(std::memory_order_acquire));
			if (elapsed < config_.hangThreshold || heartbeat->hangReported.exchange(true, std::memory_order_acq_rel)) {
				continue;
			}

			if (callback_) {
				callback_(*heartbeat, elapsed);
			}
		}
		lock.lock();
	}
}

} // namespace OBS
