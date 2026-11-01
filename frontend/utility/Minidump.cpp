/******************************************************************************
    Copyright (C) 2026 OBS Studio contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "Minidump.hpp"

#include <util/base.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dbghelp.h>

#include <mutex>

namespace {

using MiniDumpWriteDumpFunction = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
									PMINIDUMP_EXCEPTION_INFORMATION,
									PMINIDUMP_USER_STREAM_INFORMATION,
									PMINIDUMP_CALLBACK_INFORMATION);

constexpr MINIDUMP_TYPE kDumpType = static_cast<MINIDUMP_TYPE>(
	MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo);

std::mutex minidumpMutex;

struct ThreadSuspendGuard {
	HANDLE handle = nullptr;
	bool suspended = false;

	~ThreadSuspendGuard()
	{
		if (suspended) {
			if (ResumeThread(handle) == static_cast<DWORD>(-1)) {
				blog(LOG_WARNING, "Failed to resume thread after jank minidump (error %lu)", GetLastError());
			}
		}
		if (handle) {
			CloseHandle(handle);
		}
	}
};

} // namespace

namespace OBS {

MinidumpResult CaptureCurrentProcessMinidump(const std::filesystem::path &path)
{
	HMODULE dbghelp = LoadLibraryW(L"DbgHelp.dll");
	if (!dbghelp) {
		return {MinidumpStatus::LoaderUnavailable, GetLastError()};
	}

	auto writeDump = reinterpret_cast<MiniDumpWriteDumpFunction>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
	if (!writeDump) {
		const DWORD errorCode = GetLastError();
		FreeLibrary(dbghelp);
		return {MinidumpStatus::LoaderUnavailable, errorCode};
	}

	HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
					 FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		const DWORD errorCode = GetLastError();
		FreeLibrary(dbghelp);
		return {MinidumpStatus::FileOpenFailed, errorCode};
	}

	BOOL result = FALSE;
	{
		std::lock_guard<std::mutex> lock(minidumpMutex);
		result = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, kDumpType, nullptr, nullptr, nullptr);
	}

	const DWORD errorCode = result ? ERROR_SUCCESS : GetLastError();
	CloseHandle(file);
	FreeLibrary(dbghelp);

	return result ? MinidumpResult{MinidumpStatus::Success, ERROR_SUCCESS}
			      : MinidumpResult{MinidumpStatus::WriteFailed, errorCode};
}

MinidumpResult CaptureThreadMinidump(const std::filesystem::path &path, uint64_t nativeThreadId)
{
	ThreadSuspendGuard suspendGuard;
	suspendGuard.handle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, static_cast<DWORD>(nativeThreadId));
	if (!suspendGuard.handle) {
		blog(LOG_WARNING, "Unable to open thread for jank minidump (error %lu)", GetLastError());
	} else if (SuspendThread(suspendGuard.handle) == static_cast<DWORD>(-1)) {
		blog(LOG_WARNING, "Unable to suspend thread for jank minidump (error %lu)", GetLastError());
		CloseHandle(suspendGuard.handle);
		suspendGuard.handle = nullptr;
	} else {
		suspendGuard.suspended = true;
	}

	return CaptureCurrentProcessMinidump(path);
}

} // namespace OBS

#else

namespace OBS {

MinidumpResult CaptureCurrentProcessMinidump(const std::filesystem::path & /*path*/)
{
	return {MinidumpStatus::Unsupported, 0};
}

MinidumpResult CaptureThreadMinidump(const std::filesystem::path &path, uint64_t /*nativeThreadId*/)
{
	return CaptureCurrentProcessMinidump(path);
}

} // namespace OBS

#endif
