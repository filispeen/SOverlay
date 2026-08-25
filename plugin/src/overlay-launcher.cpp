#include "overlay-launcher.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <windows.h>
#include <tlhelp32.h>

#include <string>

namespace {

#define OVERLAY_EXE_NAME L"SOverlay.exe"
#define OVERLAY_SUBDIR L"SOverlay"

PROCESS_INFORMATION g_process_info = {};
bool g_process_started = false;

std::wstring get_plugin_dll_dir()
{
	const char *path = obs_get_module_binary_path(obs_current_module());
	if (!path)
		return L"";

	int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
	std::wstring wpath(wlen > 0 ? wlen - 1 : 0, 0);
	if (wlen > 0)
		MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wlen);
	bfree(const_cast<char *>(path));

	size_t slash = wpath.find_last_of(L"\\/");
	if (slash == std::wstring::npos)
		return L"";

	return wpath.substr(0, slash);
}

void kill_existing_overlay_processes()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32W entry;
	entry.dwSize = sizeof(entry);

	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (_wcsicmp(entry.szExeFile, OVERLAY_EXE_NAME) == 0) {
				HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
				if (proc) {
					TerminateProcess(proc, 0);
					CloseHandle(proc);
				}
			}
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);
}

} // namespace

namespace overlay_launcher {

void start()
{
	std::wstring plugin_dir = get_plugin_dll_dir();
	if (plugin_dir.empty()) {
		obs_log(LOG_ERROR, "overlay_launcher: failed to resolve plugin dll path");
		return;
	}

	std::wstring overlay_dir = plugin_dir + L"\\" OVERLAY_SUBDIR;
	std::wstring exe_path = overlay_dir + L"\\" OVERLAY_EXE_NAME;

	if (GetFileAttributesW(exe_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
		obs_log(LOG_WARNING, "overlay_launcher: overlay exe not found, skipping launch");
		return;
	}

	kill_existing_overlay_processes();

	STARTUPINFOW startup_info = {};
	startup_info.cb = sizeof(startup_info);

	PROCESS_INFORMATION process_info = {};

	std::wstring cmdline = L"\"" + exe_path + L"\"";

	BOOL ok = CreateProcessW(exe_path.c_str(), cmdline.data(), nullptr, nullptr, FALSE,
				  CREATE_NEW_PROCESS_GROUP, nullptr, overlay_dir.c_str(), &startup_info,
				  &process_info);

	if (!ok) {
		obs_log(LOG_ERROR, "overlay_launcher: failed to launch overlay, error %lu", GetLastError());
		return;
	}

	CloseHandle(process_info.hThread);
	g_process_info = process_info;
	g_process_started = true;

	obs_log(LOG_INFO, "overlay_launcher: overlay started (pid %lu)", process_info.dwProcessId);
}

void stop()
{
	if (!g_process_started)
		return;

	TerminateProcess(g_process_info.hProcess, 0);
	CloseHandle(g_process_info.hProcess);
	g_process_info = {};
	g_process_started = false;

	obs_log(LOG_INFO, "overlay_launcher: overlay stopped");
}

} // namespace overlay_launcher
