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
int cb = 0;
WCHAR szFolderPath[MAX_PATH + 1];
auto currentPath = fs::current_path();
std::vector<std::wstring> v(158);
MSG msg;
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

const wchar_t* box[4] = {
	L"League of Legends",
	L"Dota 2",
	L"SMITE 2",
	L"WinTweaks"
};

HRESULT FolderBrowser(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath)
{
	v[0].clear();
	IFileDialog* pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	DWORD dwOptions;
	hr = pfd->GetOptions(&dwOptions);
	hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
	hr = pfd->Show(hwndOwner);
	IShellItem* psi = nullptr;
	hr = pfd->GetResult(&psi);
	PWSTR pszPath = nullptr;
	hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
	wcsncpy_s(pszFolderPath, cchFolderPath, pszPath, _TRUNCATE);
	CoTaskMemFree(pszPath);
	psi->Release();
	pfd->Release();
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

void manageTasks(const std::wstring& task)
{

	if (task == L"support")
	{
		MessageBoxEx(
			nullptr,
			L"Please wait until finish! (Press OK)",
			L"LoLSuite",
			MB_OK,
			0);

		const std::vector<std::wstring> processes = { L"cmd.exe", L"pwsh.exe",L"powershell.exe", L"WindowsTerminal.exe", L"OpenConsole.exe", L"DXSETUP.exe", L"Battle.net.exe", L"steam.exe", L"Origin.exe", L"EADesktop.exe", L"EpicGamesLauncher.exe" };
		for (const auto& process : processes) Term(process);
		ManageService(L"W32Time", true);

		executeCommands({
			L"w32tm /resync",
			L"powercfg -restoredefaultschemes",
			L"powercfg /h off",
			L"Clear-DnsClientCache",
			L"Add-WindowsCapability -Online -Name NetFx3~~~~",
			L"winget source update",
			L"winget uninstall Valve.Steam --purge -h",
			L"winget uninstall ElectronicArts.EADesktop --purge -h",
			L"winget uninstall ElectronicArts.Origin --purge -h",
			L"winget uninstall EpicGames.EpicGamesLauncher --purge -h",
			L"winget uninstall Blizzard.BattleNet --purge -h",
			L"winget uninstall Microsoft.WindowsTerminal --purge -h",
			L"winget uninstall Microsoft.DirectX --purge -h",
			L"winget uninstall Microsoft.PowerShell --purge -h",
			L"winget uninstall Microsoft.EdgeWebView2Runtime --purge -h",
			L"winget uninstall 9NQPSL29BFFF --purge -h",
			L"winget uninstall 9PB0TRCNRHFX --purge -h",
			L"winget uninstall 9N95Q1ZZPMH4 --purge -h",
			L"winget uninstall 9NCTDW2W1BH8 --purge -h",
			L"winget uninstall 9MVZQVXJBQ9V --purge -h",
			L"winget uninstall 9PMMSR1CGPWG --purge -h",
			L"winget uninstall 9N4D0MSMP0PT --purge -h",
			L"winget uninstall 9PG2DK419DRG --purge -h",
			L"winget uninstall 9N5TDP8VCMHS --purge -h",
			L"winget uninstall 9PCSD6N03BKV --purge -h",
			L"winget uninstall Microsoft.VCRedist.2005.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2008.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2010.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2012.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2013.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2015+.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2005.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2008.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2010.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2012.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2013.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2015+.x64 --purge -h",
			L"winget install Microsoft.WindowsTerminal --accept-package-agreements",
			L"winget install Microsoft.PowerShell --accept-package-agreements",
			L"winget install Microsoft.EdgeWebView2Runtime --accept-package-agreement",
			L"winget install 9NQPSL29BFFF --accept-package-agreements",
			L"winget install 9N95Q1ZZPMH4 --accept-package-agreements",
			L"winget install 9NCTDW2W1BH8 --accept-package-agreements",
			L"winget install 9MVZQVXJBQ9V --accept-package-agreements",
			L"winget install 9PMMSR1CGPWG --accept-package-agreements",
			L"winget install 9N4D0MSMP0PT --accept-package-agreements",
			L"winget install 9PG2DK419DRG --accept-package-agreements",
			L"winget install 9PB0TRCNRHFX --accept-package-agreements",
			L"winget install 9N5TDP8VCMHS --accept-package-agreements",
			L"winget install 9PCSD6N03BKV --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2005.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2015+.x86 --accept-package-agreements",
			L"winget install ElectronicArts.EADesktop --accept-package-agreements",
			L"winget install EpicGames.EpicGamesLauncher --accept-package-agreements",
			L"winget install Valve.Steam --accept-package-agreements",
			L"winget install Blizzard.BattleNet --location \"C:\\Battle.Net\" --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2005.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2015+.x64 --accept-package-agreements"
			});

		AddCommandToRunOnce(L"PowerCfgDuplicateScheme", L"cmd.exe /c powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61");

		if (ShowYesNoMessageBox(L"Do you wish to install DirectX9", L"Confirmation") == IDYES)
		{
			const std::vector<std::wstring> dxx86_cab = {
						L"Apr2005_d3dx9_25_x86.cab", L"Apr2006_d3dx9_30_x86.cab", L"Apr2006_MDX1_x86.cab", L"Apr2006_MDX1_x86_Archive.cab", L"Apr2006_XACT_x86.cab",
						L"Apr2006_xinput_x86.cab", L"APR2007_d3dx9_33_x86.cab", L"APR2007_d3dx10_33_x86.cab", L"APR2007_XACT_x86.cab", L"APR2007_xinput_x86.cab",
						L"Aug2005_d3dx9_27_x86.cab", L"AUG2006_XACT_x86.cab", L"AUG2006_xinput_x86.cab", L"AUG2007_d3dx9_35_x86.cab", L"AUG2007_d3dx10_35_x86.cab",
						L"AUG2007_XACT_x86.cab", L"Aug2008_d3dx9_39_x86.cab", L"Aug2008_d3dx10_39_x86.cab", L"Aug2008_XACT_x86.cab", L"Aug2008_XAudio_x86.cab",
						L"Aug2009_D3DCompiler_42_x86.cab", L"Aug2009_d3dcsx_42_x86.cab", L"Aug2009_d3dx9_42_x86.cab", L"Aug2009_d3dx10_42_x86.cab",
						L"Aug2009_d3dx11_42_x86.cab", L"Aug2009_XACT_x86.cab", L"Aug2009_XAudio_x86.cab", L"Dec2005_d3dx9_28_x86.cab",
						L"DEC2006_d3dx9_32_x86.cab", L"DEC2006_d3dx10_00_x86.cab", L"DEC2006_XACT_x86.cab", L"Feb2005_d3dx9_24_x86.cab", L"Feb2006_d3dx9_29_x86.cab",
						L"Feb2006_XACT_x86.cab", L"FEB2007_XACT_x86.cab", L"Feb2010_X3DAudio_x86.cab", L"Feb2010_XACT_x86.cab", L"Feb2010_XAudio_x86.cab",
						L"Jun2005_d3dx9_26_x86.cab", L"JUN2006_XACT_x86.cab", L"JUN2007_d3dx9_34_x86.cab", L"JUN2007_d3dx10_34_x86.cab", L"JUN2007_XACT_x86.cab",
						L"JUN2008_d3dx9_38_x86.cab", L"JUN2008_d3dx10_38_x86.cab", L"JUN2008_X3DAudio_x86.cab", L"JUN2008_XACT_x86.cab", L"JUN2008_XAudio_x86.cab",
						L"Jun2010_D3DCompiler_43_x86.cab", L"Jun2010_d3dcsx_43_x86.cab", L"Jun2010_d3dx9_43_x86.cab", L"Jun2010_d3dx10_43_x86.cab",
						L"Jun2010_d3dx11_43_x86.cab", L"Jun2010_XACT_x86.cab", L"Jun2010_XAudio_x86.cab", L"Mar2008_d3dx9_37_x86.cab", L"Mar2008_d3dx10_37_x86.cab",
						L"Mar2008_X3DAudio_x86.cab", L"Mar2008_XACT_x86.cab", L"Mar2008_XAudio_x86.cab", L"Mar2009_d3dx9_41_x86.cab", L"Mar2009_d3dx10_41_x86.cab",
						L"Mar2009_X3DAudio_x86.cab", L"Mar2009_XACT_x86.cab", L"Mar2009_XAudio_x86.cab", L"Nov2007_d3dx9_36_x86.cab", L"Nov2007_d3dx10_36_x86.cab",
						L"NOV2007_X3DAudio_x86.cab", L"NOV2007_XACT_x86.cab", L"Nov2008_d3dx9_40_x86.cab", L"Nov2008_d3dx10_40_x86.cab", L"Nov2008_X3DAudio_x86.cab",
						L"Nov2008_XACT_x86.cab", L"Nov2008_XAudio_x86.cab", L"Oct2005_xinput_x86.cab", L"OCT2006_d3dx9_31_x86.cab", L"OCT2006_XACT_x86.cab"
			};

			const std::vector<std::wstring> dxx64_cab = {
				L"Apr2005_d3dx9_25_x64.cab", L"Apr2006_d3dx9_30_x64.cab", L"Apr2006_XACT_x64.cab", L"Apr2006_xinput_x64.cab",
				L"APR2007_d3dx9_33_x64.cab", L"APR2007_d3dx10_33_x64.cab", L"APR2007_XACT_x64.cab", L"APR2007_xinput_x64.cab",
				L"Aug2005_d3dx9_27_x64.cab", L"AUG2006_XACT_x64.cab", L"AUG2006_xinput_x64.cab", L"AUG2007_d3dx9_35_x64.cab",
				L"AUG2007_d3dx10_35_x64.cab", L"AUG2007_XACT_x64.cab", L"Aug2008_d3dx9_39_x64.cab", L"Aug2008_d3dx10_39_x64.cab",
				L"Aug2008_XACT_x64.cab", L"Aug2008_XAudio_x64.cab", L"Aug2009_D3DCompiler_42_x64.cab", L"Aug2009_d3dcsx_42_x64.cab",
				L"Aug2009_d3dx9_42_x64.cab", L"Aug2009_d3dx10_42_x64.cab", L"Aug2009_d3dx11_42_x64.cab", L"Aug2009_XACT_x64.cab",
				L"Aug2009_XAudio_x64.cab", L"Dec2005_d3dx9_28_x64.cab", L"DEC2006_d3dx9_32_x64.cab", L"DEC2006_d3dx10_00_x64.cab",
				L"DEC2006_XACT_x64.cab", L"Feb2005_d3dx9_24_x64.cab", L"Feb2006_d3dx9_29_x64.cab", L"Feb2006_XACT_x64.cab",
				L"FEB2007_XACT_x64.cab", L"Feb2010_X3DAudio_x64.cab", L"Feb2010_XACT_x64.cab", L"Feb2010_XAudio_x64.cab",
				L"Jun2005_d3dx9_26_x64.cab", L"JUN2006_XACT_x64.cab", L"JUN2007_d3dx9_34_x64.cab", L"JUN2007_d3dx10_34_x64.cab",
				L"JUN2007_XACT_x64.cab", L"JUN2008_d3dx9_38_x64.cab", L"JUN2008_d3dx10_38_x64.cab", L"JUN2008_X3DAudio_x64.cab",
				L"JUN2008_XACT_x64.cab", L"JUN2008_XAudio_x64.cab", L"Jun2010_D3DCompiler_43_x64.cab", L"Jun2010_d3dcsx_43_x64.cab",
				L"Jun2010_d3dx9_43_x64.cab", L"Jun2010_d3dx10_43_x64.cab", L"Jun2010_d3dx11_43_x64.cab", L"Jun2010_XACT_x64.cab",
				L"Jun2010_XAudio_x64.cab", L"Mar2008_d3dx9_37_x64.cab", L"Mar2008_d3dx10_37_x64.cab", L"Mar2008_X3DAudio_x64.cab",
				L"Mar2008_XACT_x64.cab", L"Mar2008_XAudio_x64.cab", L"Mar2009_d3dx9_41_x64.cab", L"Mar2009_d3dx10_41_x64.cab",
				L"Mar2009_X3DAudio_x64.cab", L"Mar2009_XACT_x64.cab", L"Mar2009_XAudio_x64.cab", L"Nov2007_d3dx9_36_x64.cab",
				L"Nov2007_d3dx10_36_x64.cab", L"NOV2007_X3DAudio_x64.cab", L"NOV2007_XACT_x64.cab", L"Nov2008_d3dx9_40_x64.cab",
				L"Nov2008_d3dx10_40_x64.cab", L"Nov2008_X3DAudio_x64.cab", L"Nov2008_XACT_x64.cab", L"Nov2008_XAudio_x64.cab",
				L"Oct2005_xinput_x64.cab", L"OCT2006_d3dx9_31_x64.cab", L"OCT2006_XACT_x64.cab"
			};

			const std::vector<std::wstring> dxsetup_files = { L"DSETUP.dll", L"dsetup32.dll", L"dxdllreg_x86.cab", L"DXSETUP.exe", L"dxupdate.cab" };

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
		}

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

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	LimitSingleInstance GUID(L"{3025d31f-c76e-435c-a4b48-9d084fa9f5ea}");
	if (LimitSingleInstance::AnotherInstanceRunning())
		return 0;

	WCHAR szWindowClass[] = L"LoLSuite";

	WNDCLASSEXW wcex = {
		sizeof(WNDCLASSEXW),
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON)),
		LoadCursorW(nullptr, IDC_ARROW),
		reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1)),
		nullptr,
		szWindowClass,
		LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON))
	};
	RegisterClassEx(&wcex);

	HWND hWnd = CreateWindowExW(
		0, szWindowClass, L"LoLSuite", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT, 400, 100,
		nullptr, nullptr, hInstance, nullptr
	);
	if (!hWnd) return FALSE;

	std::vector<std::tuple<DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HMENU>> controls = {
		{WS_EX_TOOLWINDOW, L"BUTTON", L"Patch", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 20, 60, 30, reinterpret_cast<HMENU>(1)},
		{0, L"BUTTON", L"Restore", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 75, 20, 60, 30, reinterpret_cast<HMENU>(2)}
	};

	for (const auto& [dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hMenu] : controls) {
		if (!CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr))
			return FALSE;
	}

	HWND combobox = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWN | WS_CHILD | WS_VISIBLE, 150, 20, 200, 300, hWnd, NULL, hInstance, NULL);

	for (const auto& str : box) {
		SendMessage(combobox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	}
	SendMessage(combobox, CB_SETCURSEL, 0, 0);

	ClearWindowsUpdateCache();

	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return static_cast<int>(msg.wParam);
}
