#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include "resource.h"
#include <ShObjIdl_core.h>
#include <TlHelp32.h>
#include <vector>
#include <shellapi.h>
#include <windows.h>
#include <unordered_map>
#include <functional>
#include <wininet.h>
#include <filesystem>
#include <urlmon.h>
#include <winsvc.h>
#include <ShlObj.h>

namespace fs = std::filesystem;
WCHAR szFolderPath[MAX_PATH + 1];
auto currentPath = fs::current_path();
std::vector<std::wstring> v(158);
MSG msg;
int cb = 0;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

class LimitSingleInstance
{
protected:
	volatile HANDLE Mutex;

public:
	explicit LimitSingleInstance(const std::wstring& strMutexName)
		: Mutex(nullptr)
	{
		Mutex = CreateMutex(nullptr, FALSE, strMutexName.c_str());
	}

	~LimitSingleInstance()
	{
		if (Mutex)
		{
			ReleaseMutex(Mutex);
			CloseHandle(Mutex);
			Mutex = nullptr;
		}
	}

	LimitSingleInstance(const LimitSingleInstance&) = delete;
	LimitSingleInstance& operator=(const LimitSingleInstance&) = delete;

	static BOOL AnotherInstanceRunning()
	{
		return (GetLastError() == ERROR_ALREADY_EXISTS);
	}
};

HRESULT FolderBrowser(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath)
{
	v[0].clear();
	IFileDialog* pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (FAILED(hr)) {
		return hr; // Abort if the dialog creation fails
	}

	DWORD dwOptions;
	hr = pfd->GetOptions(&dwOptions);
	if (FAILED(hr)) {
		pfd->Release(); // Clean up before aborting
		return hr;
	}

	hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
	if (FAILED(hr)) {
		pfd->Release(); // Clean up before aborting
		return hr;
	}

	hr = pfd->Show(hwndOwner);
	if (FAILED(hr)) {
		pfd->Release(); // Clean up before aborting
		return hr;
	}

	IShellItem* psi = nullptr;
	hr = pfd->GetResult(&psi);
	if (FAILED(hr)) {
		pfd->Release(); // Clean up before aborting
		return hr;
	}

	PWSTR pszPath = nullptr;
	hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
	if (FAILED(hr) || pszPath == nullptr) {
		// Clean up and abort if the path is null
		psi->Release();
		pfd->Release();
		return E_ABORT;
	}

	// Copy the folder path and clean up
	wcsncpy_s(pszFolderPath, cchFolderPath, pszPath, _TRUNCATE);
	CoTaskMemFree(pszPath);

	// Release resources
	psi->Release();
	pfd->Release();

	// Update the global variable and return the result
	v[0] = pszFolderPath;
	return hr;
}



std::wstring JoinPath(const int index, const std::wstring& add)
{
	return (fs::path(v[index]) / add).wstring();
}

void AppendPath(const int index, const std::wstring& add)
{
	v[index] = JoinPath(index, add);
}

void CombinePath(const int destIndex, const int srcIndex, const std::wstring& add)
{
	v[destIndex] = JoinPath(srcIndex, add);
}

void Start(const std::wstring& lpFile, const std::wstring& lpParameters, bool wait, bool highPriority = true)
{
	SHELLEXECUTEINFO info = {};
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.nShow = SW_SHOWNORMAL;
	info.lpFile = lpFile.c_str();
	info.lpParameters = lpParameters.c_str();
	if (!ShellExecuteEx(&info))
	{
		return;
	}
	if (wait && info.hProcess != nullptr)
	{
		WaitForSingleObject(info.hProcess, INFINITE);
	}

	if (info.hProcess != nullptr)
	{
		if (highPriority)
		{
			SetPriorityClass(info.hProcess, HIGH_PRIORITY_CLASS);
		}
		CloseHandle(info.hProcess);
	}
}

void Term(const std::wstring& process_name)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;

	PROCESSENTRY32 processEntry = { sizeof(PROCESSENTRY32) };
	for (BOOL hasProcess = Process32First(snapshot, &processEntry); hasProcess; hasProcess = Process32Next(snapshot, &processEntry))
	{
		if (wcscmp(processEntry.szExeFile, process_name.c_str()) == 0)
		{
			HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, processEntry.th32ProcessID);
			if (processHandle)
			{
				TerminateProcess(processHandle, 0);
				CloseHandle(processHandle);
			}
			break;
		}
	}
	CloseHandle(snapshot);
}
static bool ProccessIs64Bit() {
	BOOL isWow64 = FALSE;
	HMODULE hModule = GetModuleHandle(TEXT("kernel32"));
	if (!hModule) {
		return false;
	}

	USHORT processMachine = 0;
	USHORT nativeMachine = 0;

	auto fnIsWow64Process2 = reinterpret_cast<decltype(&IsWow64Process2)>(
		GetProcAddress(hModule, "IsWow64Process2"));

	if (fnIsWow64Process2) {
		if (!fnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)) {
			return false;
		}
		return processMachine != IMAGE_FILE_MACHINE_UNKNOWN;
	}
	else {
		using LPFN_ISWOW64PROCESS = BOOL(WINAPI*)(HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fnIsWow64Process = reinterpret_cast<LPFN_ISWOW64PROCESS>(
			GetProcAddress(hModule, "IsWow64Process"));

		if (fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &isWow64)) {
			return isWow64;
		}
		return false;
	}
}

void unblock(const std::wstring& file)
{
	if (fs::exists(file + L":Zone.Identifier"))
	{
		fs::remove(file + L":Zone.Identifier");
	}
}

void dl(const std::wstring& url, int j, bool serv)
{
	std::wstring Url = serv ? L"https://lolsuite.org/files/" + url : url;
	DeleteUrlCacheEntry(Url.c_str());
	URLDownloadToFile(nullptr, Url.c_str(), v[j].c_str(), 0, nullptr);
	unblock(v[j]);
}

void manageGame(const std::wstring& game, bool restore)
{
	if (game == L"leagueoflegends") {
		MessageBoxEx(nullptr, L"Select: C:\\Riot Games", L"LoLSuite", MB_OK, 0);
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));

		const std::vector<std::wstring> processes = {
			L"LeagueClient.exe",
			L"LeagueClientUx.exe",
			L"LeagueClientUxRender.exe",
			L"League of Legends.exe",
			L"Riot Client.exe",
			L"RiotClientServices.exe",
			L"RiotClientCrashHandler.exe",
			L"LeagueCrashHandler64.exe"
		};

		for (const auto& process : processes) {
			Term(process);
		}

		CombinePath(56, 0, L"Riot Client\\RiotClientElectron");
		AppendPath(0, L"League of Legends");

		const std::vector<std::pair<int, std::wstring>> baseFiles = {
			{42, L"concrt140.dll"}, {43, L"d3dcompiler_47.dll"}, {44, L"msvcp140.dll"},
			{45, L"msvcp140_1.dll"}, {46, L"msvcp140_2.dll"}, {47, L"msvcp140_codecvt_ids.dll"},
			{48, L"ucrtbase.dll"}, {49, L"vcruntime140.dll"}, {50, L"vcruntime140_1.dll"}
		};

		for (const auto& [index, file] : baseFiles) {
			CombinePath(index, 0, file);
			dl(restore ? (L"r/lol/" + file).c_str() : file.c_str(), index, true);
		}

		CombinePath(51, 0, L"Game");
		unblock(JoinPath(51, L"League of Legends.exe"));
		unblock(JoinPath(56, L"Riot Client.exe"));
		CombinePath(53, 51, L"D3DCompiler_47.dll");
		CombinePath(55, 51, L"tbb.dll");
		CombinePath(54, 0, L"d3dcompiler_47.dll");

		if (restore) {
			fs::remove(v[55]);
		}
		else {
			dl(L"tbb.dll", 55, true);
		}

		const std::wstring d3dcompilerPath = restore ? L"r/lol/D3DCompiler_47.dll" : (ProccessIs64Bit() ? L"D3DCompiler_47.dll_x64" : L"D3DCompiler_47.dll");
		dl(d3dcompilerPath.c_str(), 53, true);
		dl(d3dcompilerPath.c_str(), 54, true);

		Start(JoinPath(56, L"Riot Client.exe"), L"", false);
	}
	else if (game == L"dota2") {
		MessageBoxEx(nullptr, L"Select: C:\\Program Files (x86)\\Steam\\steamapps\\common\\dota 2 beta", L"LoLSuite", MB_OK, 0);
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
		Term(L"dota2.exe");

		AppendPath(0, L"game\\bin\\win64");
		CombinePath(1, 0, L"embree3.dll");

		unblock(JoinPath(0, L"dota2.exe"));
		dl(restore ? L"r/dota2/embree3.dll" : L"embree4.dll", 1, true);

		Start(L"steam://rungameid/570//-high/", L"", false);
	}
	else if (game == L"smite2") {
		MessageBoxEx(nullptr, L"Select: C:\\Program Files (x86)\\Steam\\steamapps\\common\\SMITE 2", L"LoLSuite", MB_OK, 0);
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
		Term(L"Hemingway.exe");
		Term(L"Hemingway-Win64-Shipping.exe");

		CombinePath(8, 0, L"Windows\\Engine\\Binaries\\Win64");
		CombinePath(7, 0, L"Windows\\Hemingway\\Binaries\\Win64");

		CombinePath(1, 8, L"tbb.dll");
		dl(restore ? L"r/smite2/tbb.dll" : L"tbb.dll", 1, true);

		CombinePath(2, 8, L"tbbmalloc.dll");
		dl(restore ? L"r/smite2/tbbmalloc.dll" : L"tbbmalloc.dll", 2, true);

		CombinePath(4, 7, L"tbb.dll");
		CombinePath(5, 7, L"tbb12.dll");
		CombinePath(6, 7, L"tbbmalloc.dll");
		dl(restore ? L"r/smite2/tbb.dll" : L"tbb.dll", 4, true);
		dl(restore ? L"r/smite2/tbb12.dll" : L"tbb.dll", 5, true);
		dl(restore ? L"r/smite2/tbbmalloc.dll" : L"tbbmalloc.dll", 6, true);

		Start(L"steam://rungameid/2437170", L"", false);
	}
}

void InvokePowerShellCommand(const std::wstring& command)
{
	std::wstring fullCommand = L"powershell.exe -Command \"" + command + L"\"";
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.lpVerb = L"open";
	sei.lpFile = L"powershell.exe";
	sei.lpParameters = fullCommand.c_str();
	sei.nShow = SW_HIDE;
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;

	if (ShellExecuteEx(&sei))
	{
		WaitForSingleObject(sei.hProcess, INFINITE);
		CloseHandle(sei.hProcess);
	}
}

auto executeCommands = [](const std::vector<std::wstring>& commands)
	{
		for (const auto& cmd : commands)
		{
			InvokePowerShellCommand(cmd);
		}
	};

void AddCommandToRunOnce(const std::wstring& commandName, const std::wstring& command)
{
	HKEY hKey;
	LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0, KEY_SET_VALUE, &hKey);
	if (result == ERROR_SUCCESS)
	{
		result = RegSetValueEx(hKey, commandName.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), (command.size() + 1) * sizeof(wchar_t));
		RegCloseKey(hKey);
	}
}

int ShowYesNoMessageBox(const std::wstring& text, const std::wstring& caption)
{
	return MessageBoxEx(nullptr, text.c_str(), caption.c_str(), MB_YESNO | MB_ICONQUESTION, 0);
}

void ManageService(const std::wstring& serviceName, bool start)
{
	SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!schSCManager)
		return;

	SC_HANDLE schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_START | SERVICE_STOP);
	if (!schService)
	{
		CloseServiceHandle(schSCManager);
		return;
	}

	if (start)
	{
		StartService(schService, 0, nullptr);
	}
	else
	{
		SERVICE_STATUS status;
		ControlService(schService, SERVICE_CONTROL_STOP, &status);
		while (status.dwCurrentState != SERVICE_STOPPED)
		{
			Sleep(1000);
			if (!QueryServiceStatus(schService, &status))
			{
				break;
			}
		}
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

// Helper function to generate file names dynamically
std::vector<std::wstring> generateFiles(const std::vector<std::wstring>& prefixes, const std::wstring& suffix) {
	std::vector<std::wstring> files;
	for (const auto& prefix : prefixes) {
		files.push_back(prefix + suffix);
	}
	return files;
}

void manageTasks(const std::wstring& task)
{

	if (task == L"support")
	{
		const std::vector<std::wstring> processes = {
			L"cmd.exe",                          // Command Prompt
			L"pwsh.exe",                         // PowerShell Core/Modern
			L"powershell.exe",                   // Windows PowerShell
			L"WindowsTerminal.exe",              // Windows Terminal
			L"OpenConsole.exe",                  // Legacy console processor used by Windows Terminal
			L"wt.exe",                           // Shortcut for Windows Terminal
			L"DXSETUP.exe",                      // DirectX Setup
			L"Battle.net.exe",                   // Blizzard Battle.net Launcher
			L"steam.exe",                        // Steam Launcher
			L"Origin.exe",                       // EA's Legacy Origin Launcher
			L"EADesktop.exe",                    // EA Desktop App
			L"EpicGamesLauncher.exe"             // Epic Games Launcher
		};

		for (const auto& process : processes) {
			Term(process);
		}

		// Common prefixes for the files
		std::vector<std::wstring> dates = {
			L"Apr2005_d3dx9_25", L"Apr2006_d3dx9_30",
			L"Apr2006_XACT", L"Apr2006_xinput", L"APR2007_d3dx9_33", L"APR2007_d3dx10_33",
			L"APR2007_XACT", L"APR2007_xinput", L"Aug2005_d3dx9_27", L"AUG2006_XACT",
			L"AUG2006_xinput", L"AUG2007_d3dx9_35", L"AUG2007_d3dx10_35", L"AUG2007_XACT",
			L"Aug2008_d3dx9_39", L"Aug2008_d3dx10_39", L"Aug2008_XACT", L"Aug2008_XAudio",
			L"Aug2009_D3DCompiler_42", L"Aug2009_d3dcsx_42", L"Aug2009_d3dx9_42", L"Aug2009_d3dx10_42",
			L"Aug2009_d3dx11_42", L"Aug2009_XACT", L"Aug2009_XAudio", L"Dec2005_d3dx9_28",
			L"DEC2006_d3dx9_32", L"DEC2006_d3dx10_00", L"DEC2006_XACT", L"Feb2005_d3dx9_24",
			L"Feb2006_d3dx9_29", L"Feb2006_XACT", L"FEB2007_XACT", L"Feb2010_X3DAudio",
			L"Feb2010_XACT", L"Feb2010_XAudio", L"Jun2005_d3dx9_26", L"JUN2006_XACT",
			L"JUN2007_d3dx9_34", L"JUN2007_d3dx10_34", L"JUN2007_XACT", L"JUN2008_d3dx9_38",
			L"JUN2008_d3dx10_38", L"JUN2008_X3DAudio", L"JUN2008_XACT", L"JUN2008_XAudio",
			L"Jun2010_D3DCompiler_43", L"Jun2010_d3dcsx_43", L"Jun2010_d3dx9_43", L"Jun2010_d3dx10_43",
			L"Jun2010_d3dx11_43", L"Jun2010_XACT", L"Jun2010_XAudio", L"Mar2008_d3dx9_37",
			L"Mar2008_d3dx10_37", L"Mar2008_X3DAudio", L"Mar2008_XACT", L"Mar2008_XAudio",
			L"Mar2009_d3dx9_41", L"Mar2009_d3dx10_41", L"Mar2009_X3DAudio", L"Mar2009_XACT",
			L"Mar2009_XAudio", L"Nov2007_d3dx9_36", L"Nov2007_d3dx10_36", L"NOV2007_X3DAudio",
			L"NOV2007_XACT", L"Nov2008_d3dx9_40", L"Nov2008_d3dx10_40", L"Nov2008_X3DAudio",
			L"Nov2008_XACT", L"Nov2008_XAudio", L"Oct2005_xinput", L"OCT2006_d3dx9_31",
			L"OCT2006_XACT"
		};

		// Generate x86 and x64 file lists
		std::vector<std::wstring> dxx86_cab = generateFiles(dates, L"_x86.cab");
		std::vector<std::wstring> dxx64_cab = generateFiles(dates, L"_x64.cab");

		// Misc setup files
		const std::vector<std::wstring> dxsetup_files = {
			L"DSETUP.dll", L"dsetup32.dll", L"dxdllreg_x86.cab", L"DXSETUP.exe", L"dxupdate.cab", L"Apr2006_MDX1_x86_Archive.cab", L"Apr2006_MDX1_x86.cab"
		};

		v[82].clear();
		AppendPath(82, currentPath);
		AppendPath(82, L"tmp");
		fs::create_directory(v[82]);

		auto download_files = [&](const std::vector<std::wstring>& files) {
			for (size_t i = 0; i < files.size(); ++i) {
				v[i].clear();
				CombinePath(i, 82, files[i]);
				dl(L"dx9/" + files[i], i, true);
			}
			};

		download_files(dxx86_cab);
		download_files(dxx64_cab);
		download_files(dxsetup_files);

		Start(JoinPath(82, L"DXSETUP.exe"), L"/silent", true); // Wait for finish
		fs::remove_all(v[82]);

		ManageService(L"W32Time", true);

		executeCommands({
			L"w32tm /resync",
			L"powercfg -restoredefaultschemes",
			L"powercfg /h off",
			L"wsreset -i"
			L"Clear-DnsClientCache",
			L"Add-WindowsCapability -Online -Name NetFx3~~~~",
			L"Update-Help -Force -ErrorAction SilentlyContinue"
			});

		// Define common prefixes and suffixes
		std::wstring uninstallCommand = L"winget uninstall ";
		std::wstring installCommand = L"winget install ";
		std::wstring optionsPurge = L" --purge -h";
		std::wstring optionsAccept = L" --accept-package-agreements";

		// List of application IDs for uninstall
		std::vector<std::wstring> uninstallApps = {
			L"Valve.Steam", L"ElectronicArts.EADesktop", L"ElectronicArts.Origin", L"Microsoft.WindowsTerminal.Preview",
			L"EpicGames.EpicGamesLauncher", L"Blizzard.BattleNet", L"Microsoft.WindowsTerminal", L"9N0DX20HK701"
			L"Microsoft.DirectX", L"Microsoft.PowerShell", L"Microsoft.EdgeWebView2Runtime", L"9N8G5RFZ9XK3"
			L"9NQPSL29BFFF", L"9PB0TRCNRHFX", L"9N95Q1ZZPMH4", L"9NCTDW2W1BH8", L"9MVZQVXJBQ9V",
			L"9PMMSR1CGPWG", L"9N4D0MSMP0PT", L"9PG2DK419DRG", L"9N5TDP8VCMHS", L"9PCSD6N03BKV",
			L"Microsoft.VCRedist.2005.x86", L"Microsoft.VCRedist.2008.x86", L"Microsoft.VCRedist.2010.x86",
			L"Microsoft.VCRedist.2012.x86", L"Microsoft.VCRedist.2013.x86", L"Microsoft.VCRedist.2015+.x86",
			L"Microsoft.VCRedist.2005.x64", L"Microsoft.VCRedist.2008.x64", L"Microsoft.VCRedist.2010.x64",
			L"Microsoft.VCRedist.2012.x64", L"Microsoft.VCRedist.2013.x64", L"Microsoft.VCRedist.2015+.x64"
		};

		// List of application IDs for install
		std::vector<std::wstring> installApps = {
			L"Microsoft.PowerShell", L"Microsoft.WindowsTerminal", L"Microsoft.EdgeWebView2Runtime",
			L"9NQPSL29BFFF", L"9PB0TRCNRHFX", L"9N95Q1ZZPMH4", L"9NCTDW2W1BH8", L"9MVZQVXJBQ9V",
			L"9PMMSR1CGPWG", L"9N4D0MSMP0PT", L"9PG2DK419DRG", L"9N5TDP8VCMHS", L"9PCSD6N03BKV",
			L"Microsoft.VCRedist.2005.x86", L"Microsoft.VCRedist.2008.x86", L"Microsoft.VCRedist.2010.x86",
			L"Microsoft.VCRedist.2012.x86", L"Microsoft.VCRedist.2013.x86", L"Microsoft.VCRedist.2015+.x86",
			L"ElectronicArts.EADesktop", L"EpicGames.EpicGamesLauncher", L"Valve.Steam",
			L"Blizzard.BattleNet", L"Microsoft.VCRedist.2005.x64", L"Microsoft.VCRedist.2008.x64",
			L"Microsoft.VCRedist.2010.x64", L"Microsoft.VCRedist.2012.x64", L"Microsoft.VCRedist.2013.x64",
			L"Microsoft.VCRedist.2015+.x64"
		};

		// Generate uninstall commands
		std::vector<std::wstring> commands;
		for (const auto& app : uninstallApps) {
			commands.push_back(uninstallCommand + app + optionsPurge);
		}

		// Generate install commands
		for (const auto& app : installApps) {
			std::wstring command = installCommand + app;
			if (app == L"Blizzard.BattleNet") {
				command += L" --location \"C:\\Battle.Net\""; // Special case for Blizzard
			}
			command += optionsAccept;
			commands.push_back(command);
		}

		// Execute all commands
		executeCommands(commands);
		InvokePowerShellCommand(L"Get-AppxPackage -AllUsers | ForEach-Object { Add-AppxPackage -DisableDevelopmentMode -Register \"$($_.InstallLocation)\\AppxManifest.xml\" }");

		AddCommandToRunOnce(L"PowerCfgDuplicateScheme", L"cmd.exe /c powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61");

		if (ShowYesNoMessageBox(L"Do you wish to install Minecraft Launcher & Latest Java", L"Confirmation") == IDYES)
		{
			const std::vector<std::wstring> processes = { L"Minecraft.exe", L"javaw.exe", L"java.exe", L"Minecraft.Windows.exe" };

			for (const auto& process : processes) {
				Term(process);
			}

			executeCommands({
				L"winget uninstall Mojang.MinecraftLauncher --purge -h",
				L"winget uninstall Oracle.JavaRuntimeEnvironment --purge -h",
				L"winget uninstall Oracle.JDK.23 --purge -h",
				L"winget uninstall Oracle.JDK.22 --purge -h",
				L"winget uninstall Oracle.JDK.21 --purge -h",
				L"winget uninstall Oracle.JDK.20 --purge -h",
				L"winget uninstall Oracle.JDK.19 --purge -h",
				L"winget uninstall Oracle.JDK.18 --purge -h",
				L"winget uninstall Oracle.JDK.17 --purge -h",
				L"winget install Mojang.MinecraftLauncher --accept-package-agreements",
				L"winget install Oracle.JDK.23 --accept-package-agreements"
				});

			MessageBoxEx(
				nullptr,
				L"Minecraft Launcher > Java Edition > Latest Release > More Options > Java Executable > Browse > <drive>:\\Program Files\\Java\\jdk-23\\bin\\javaw.exe",
				L"LoLSuite",
				MB_OK,
				0);
		}
		if (ShowYesNoMessageBox(L"Do you wish to install Discord", L"Confirmation") == IDYES)
		{
			Term(L"Discord.exe");

			executeCommands({
				L"winget uninstall Discord.Discord --purge -h",
				L"winget install Discord.Discord --accept-package-agreements"
				});
		}
	}
}

void handleCommand(int cb, bool flag)
{
	static const std::unordered_map<int, std::function<void()>> commandMap = {
		{0, [flag]() { manageGame(L"leagueoflegends", flag); }},
		{1, [flag]() { manageGame(L"dota2", flag); }},
		{2, [flag]() { manageGame(L"smite2", flag); }},
		{3, []() { manageTasks(L"support"); }}
  };

	auto it = commandMap.find(cb);
	if (it != commandMap.end())
	{
		it->second();
	}
	exit(0);
}

void CleanCacheFiles(const std::wstring& basePath, const std::vector<std::wstring>& patterns) {
	for (const auto& pattern : patterns) {
		std::wstring searchPath = fs::path(basePath) / pattern;
		WIN32_FIND_DATA findFileData;
		HANDLE hFind = FindFirstFile(searchPath.c_str(), &findFileData);

		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				std::wstring filePath = fs::path(basePath) / findFileData.cFileName;
				fs::remove(filePath);
			} while (FindNextFile(hFind, &findFileData) != 0);
			FindClose(hFind);
		}
	}
}

void ClearWindowsUpdateCache()
{
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		CloseClipboard();
	}

	const std::vector<std::wstring> services = {
		L"wuauserv", L"BITS", L"CryptSvc"
	};

	for (const auto& service : services)
	{
		ManageService(service, false);
	}

	WCHAR localAppDataPath[MAX_PATH + 1];
	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppDataPath);
	fs::path explorerPath = fs::path(localAppDataPath) / L"Microsoft" / L"Windows" / L"Explorer";
	std::vector<std::wstring> explorerPatterns = { L"thumbcache_*.db", L"iconcache_*.db", L"ExplorerStartupLog*.etl" };
	CleanCacheFiles(explorerPath.wstring(), explorerPatterns);

	wchar_t* allUserProfile = nullptr;
	size_t len = 0;
	_wdupenv_s(&allUserProfile, &len, L"ALLUSERSPROFILE");
	if (allUserProfile) {
		std::wstring bitsPath = fs::path(allUserProfile) / L"Application Data" / L"Microsoft" / L"Network" / L"Downloader";
		std::vector<std::wstring> bitsPatterns = { L"qmgr*.dat" };
		CleanCacheFiles(bitsPath, bitsPatterns);
		free(allUserProfile);
	}

	WCHAR windowsPath[MAX_PATH];
	if (GetWindowsDirectory(windowsPath, MAX_PATH)) {
		fs::path updateCachePath = fs::path(windowsPath) / L"SoftwareDistribution";
		if (fs::exists(updateCachePath)) {
			fs::remove_all(updateCachePath);
		}
	}

	for (const auto& service : services)
	{
		ManageService(service, true);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_COMMAND:
		if (HIWORD(wParam) == CBN_SELCHANGE)
		{
			cb = SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0);
		}
		switch (LOWORD(wParam))
		{
		case 1:
			handleCommand(cb, false);
			break;
		case 2:
			handleCommand(cb, true);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

bool RegisterWindowClass(HINSTANCE hInstance, LPCWSTR className, int iconId) {
	WNDCLASSEXW wcex = {
		sizeof(WNDCLASSEXW),
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0, 0,
		hInstance,
		LoadIconW(hInstance, MAKEINTRESOURCE(iconId)),
		LoadCursorW(nullptr, IDC_ARROW),
		reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
		nullptr,
		className,
		LoadIconW(hInstance, MAKEINTRESOURCE(iconId))
	};
	return RegisterClassExW(&wcex);
}


HWND CreateMainWindow(HINSTANCE hInstance, LPCWSTR className, int width, int height) {
	return CreateWindowExW(
		0,
		className,
		L"LoLSuite",
		WS_EX_LAYERED,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);
}


void InitializeControls(HWND hWnd, HINSTANCE hInstance) {
	std::vector<std::tuple<DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HMENU>> controls = {
		{WS_EX_TOOLWINDOW, L"BUTTON", L"Patch", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 20, 60, 30, reinterpret_cast<HMENU>(1)},
		{0, L"BUTTON", L"Restore", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 75, 20, 60, 30, reinterpret_cast<HMENU>(2)}
	};

	for (const auto& [dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hMenu] : controls) {
		CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr);
	}
}

HWND CreateComboBox(HWND hWnd, HINSTANCE hInstance, int x, int y, int width, int height) {
	// Create the ComboBox
	return CreateWindowW(
		L"COMBOBOX",
		L"",
		CBS_DROPDOWN | WS_CHILD | WS_VISIBLE,
		x, y, width, height,
		hWnd,
		nullptr,
		hInstance,
		nullptr
	);
}

void PopulateComboBox(HWND comboBox, const wchar_t* items[], size_t itemCount) {
	for (size_t i = 0; i < itemCount; ++i) {
		SendMessageW(comboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(items[i]));
	}
	// Set the first item as the default selected item
	SendMessageW(comboBox, CB_SETCURSEL, 0, 0);
}



int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
) {
	// Ensure only a single instance of the application is running
	LimitSingleInstance GUID(L"{3025d31f-c76e-435c-a4b48-9d084fa9f5ea}");
	if (LimitSingleInstance::AnotherInstanceRunning()) {
		return 0;
	}

	// Register window class
	if (!RegisterWindowClass(hInstance, L"LoLSuite", IDI_ICON)) {
		return FALSE;
	}

	// Create the main window
	HWND hWnd = CreateMainWindow(hInstance, L"LoLSuite", 400, 100); // Adjusted width and height
	if (!hWnd) {
		return FALSE;
	}

	// Initialize controls
	InitializeControls(hWnd, hInstance);

	const wchar_t* box[4] = {
	L"League of Legends",
	L"Dota 2",
	L"SMITE 2",
	L"WinTweaks"
	};

	// Create and populate the ComboBox
	HWND combobox = CreateComboBox(hWnd, hInstance, 150, 20, 200, 300);
	PopulateComboBox(combobox, box, std::size(box)); // Use std::size to calculate the number of items


	// Clear Windows Update cache
	ClearWindowsUpdateCache();
	SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);

	// Show and update the main window
	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return static_cast<int>(msg.wParam);
}
