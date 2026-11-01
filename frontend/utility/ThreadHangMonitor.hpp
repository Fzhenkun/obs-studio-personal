/******************************************************************************
    Copyright (C) 2026 OBS Studio contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
******************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace OBS {

struct ThreadHeartbeat {
	explicit ThreadHeartbeat(std::string threadName);

	std::string name;
	uint64_t nativeThreadId = 0;
	std::atomic<int64_t> lastBeat;
	std::atomic_bool hangReported = false;
};

class ThreadHangMonitor {
public:
	struct Config {
		std::chrono::milliseconds checkInterval{500};
		std::chrono::milliseconds hangThreshold{5000};
	};

	using Heartbeat = std::shared_ptr<ThreadHeartbeat>;
	using HangCallback = std::function<void(const ThreadHeartbeat &, std::chrono::milliseconds)>;

	ThreadHangMonitor(Config config, HangCallback callback);
	~ThreadHangMonitor();

	ThreadHangMonitor(const ThreadHangMonitor &) = delete;
	ThreadHangMonitor &operator=(const ThreadHangMonitor &) = delete;

	Heartbeat registerThread(std::string name);
	static void beat(const Heartbeat &heartbeat);

	void start();
	void stop();

private:
	void run();

	Config config_;
	HangCallback callback_;
	std::mutex heartbeatsMutex_;
	std::vector<std::weak_ptr<ThreadHeartbeat>> heartbeats_;
	std::mutex stateMutex_;
	std::condition_variable stateChanged_;
	std::thread thread_;
	bool stopping_ = false;
};

} // namespace OBS
