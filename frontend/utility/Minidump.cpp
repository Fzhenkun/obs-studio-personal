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

} // namespace OBS

#else

namespace OBS {

MinidumpResult CaptureCurrentProcessMinidump(const std::filesystem::path & /*path*/)
{
	return {MinidumpStatus::Unsupported, 0};
}

} // namespace OBS

#endif
