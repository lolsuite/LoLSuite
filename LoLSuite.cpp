#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include "resource.h"
#include <ShObjIdl_core.h>
#include <ShlObj_core.h>
#include <TlHelp32.h>
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <urlmon.h>
import <filesystem>;
import <vector>;
import <functional>;
import <thread>;
import <fstream>;
import <sstream>;


namespace fs = std::filesystem;

int cb = 0;
auto workdir = fs::current_path();
WCHAR szFolderPath[MAX_PATH + 1];
std::vector<std::wstring> fileBuffer(258);
MSG msg;
typedef BOOL(WINAPI* DnsFlushResolverCacheFuncPtr)();
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

std::vector<std::wstring> commands_minecraft = {
L"winget uninstall Mojang.MinecraftLauncher --purge -h",
L"winget uninstall Oracle.JavaRuntimeEnvironment --purge -h",
L"winget uninstall Oracle.JDK.17 --purge -h",
L"winget uninstall Oracle.JDK.18 --purge -h",
L"winget uninstall Oracle.JDK.19 --purge -h",
L"winget uninstall Oracle.JDK.20 --purge -h",
L"winget uninstall Oracle.JDK.21 --purge -h",
L"winget uninstall Oracle.JDK.22 --purge -h",
L"winget uninstall Oracle.JDK.23 --purge -h",
L"winget install Oracle.JDK.24 --accept-package-agreements",
L"winget install Mojang.MinecraftLauncher --accept-package-agreements"
};

// Define common prefixes and suffixes
std::wstring uninstallCommand = L"winget uninstall ";
std::wstring installCommand = L"winget install ";
std::wstring optionsPurge = L" --purge -h";
std::wstring optionsAccept = L" --accept-package-agreements";

// List of application IDs for uninstall
std::vector<std::wstring> apps_remove = {
	L"Valve.Steam", L"ElectronicArts.EADesktop", L"ElectronicArts.Origin", L"Microsoft.WindowsTerminal.Preview", L"Microsoft.WindowsTerminal",
	L"EpicGames.EpicGamesLauncher", L"Blizzard.BattleNet", L"9N0DX20HK701", L"9P95ZZKTNRN4",
	L"Microsoft.DirectX", L"Microsoft.PowerShell", L"Microsoft.EdgeWebView2Runtime", L"9N8G5RFZ9XK3", L"9MZ1SNWT0N5D",
	L"9NQPSL29BFFF", L"9PB0TRCNRHFX", L"9N95Q1ZZPMH4", L"9NCTDW2W1BH8", L"9MVZQVXJBQ9V",
	L"9PMMSR1CGPWG", L"9N4D0MSMP0PT", L"9PG2DK419DRG", L"9N5TDP8VCMHS", L"9PCSD6N03BKV",
	L"Microsoft.VCRedist.2005.x86", L"Microsoft.VCRedist.2008.x86", L"Microsoft.VCRedist.2010.x86",
	L"Microsoft.VCRedist.2012.x86", L"Microsoft.VCRedist.2013.x86", L"Microsoft.VCRedist.2015+.x86",
	L"Microsoft.VCRedist.2005.x64", L"Microsoft.VCRedist.2008.x64", L"Microsoft.VCRedist.2010.x64",
	L"Microsoft.VCRedist.2012.x64", L"Microsoft.VCRedist.2013.x64", L"Microsoft.VCRedist.2015+.x64"
};

// List of application IDs for install
std::vector<std::wstring> apps_install = {
	L"Microsoft.PowerShell", L"Microsoft.EdgeWebView2Runtime",
	L"9NQPSL29BFFF", L"9PB0TRCNRHFX", L"9N95Q1ZZPMH4", L"9NCTDW2W1BH8", L"9MVZQVXJBQ9V",
	L"9PMMSR1CGPWG", L"9N4D0MSMP0PT", L"9PG2DK419DRG", L"9N5TDP8VCMHS", L"9PCSD6N03BKV",
	L"Microsoft.VCRedist.2005.x86", L"Microsoft.VCRedist.2008.x86", L"Microsoft.VCRedist.2010.x86",
	L"Microsoft.VCRedist.2012.x86", L"Microsoft.VCRedist.2013.x86", L"Microsoft.VCRedist.2015+.x86",
	L"ElectronicArts.EADesktop", L"EpicGames.EpicGamesLauncher", L"Valve.Steam",
	L"Blizzard.BattleNet", L"Microsoft.VCRedist.2005.x64", L"Microsoft.VCRedist.2008.x64",
	L"Microsoft.VCRedist.2010.x64", L"Microsoft.VCRedist.2012.x64", L"Microsoft.VCRedist.2013.x64",
	L"Microsoft.VCRedist.2015+.x64", L"9N0DX20HK701"
};

std::vector<std::wstring> commands_helper = {
	L"w32tm /resync",
	L"powercfg -restoredefaultschemes",
	L"powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61",
	L"powercfg /h off",
	L"wsreset -i",
	L"Add-WindowsCapability -Online -Name NetFx3~~~~",
	L"Update-Help -Force -ErrorAction SilentlyContinue",
	L"Get-AppxPackage -AllUsers | ForEach-Object { Add-AppxPackage -DisableDevelopmentMode -Register \"$($_.InstallLocation)\\AppxManifest.xml\" }"
};


// Helper function to generate file names dynamically
std::vector<std::wstring> generateFiles(const std::vector<std::wstring>& prefixes, const std::wstring& suffix) {
	std::vector<std::wstring> files;
	for (const auto& prefix : prefixes) {
		files.push_back(prefix + suffix);
	}
	return files;
}

// Common prefixes for the files
const std::vector<std::wstring> dates = {
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

const std::vector<std::wstring> dxsetup = {
L"DSETUP.dll", L"dsetup32.dll", L"dxdllreg_x86.cab", L"DXSETUP.exe", L"dxupdate.cab", L"Apr2006_MDX1_x86_Archive.cab", L"Apr2006_MDX1_x86.cab"
};

const std::vector<std::wstring> mcprocesses = { L"Minecraft.exe", L"MinecraftLauncher.exe", L"javaw.exe", L"MinecraftServer.exe", L"java.exe", L"Minecraft.Windows.exe" };

// LimitSingleInstance Class
class LimitSingleInstance {
	HANDLE Mutex;

public:
	explicit LimitSingleInstance(const std::wstring& mutexName)
		: Mutex(CreateMutex(nullptr, FALSE, mutexName.c_str())) {
	}

	~LimitSingleInstance() {
		if (Mutex) {
			ReleaseMutex(Mutex);
			CloseHandle(Mutex);
		}
	}

	LimitSingleInstance(const LimitSingleInstance&) = delete;
	LimitSingleInstance& operator=(const LimitSingleInstance&) = delete;

	static bool AnotherInstanceRunning() {
		return GetLastError() == ERROR_ALREADY_EXISTS;
	}
};

// Function to join a base path at a given index with an additional path component
std::wstring JoinPath(const int index, const std::wstring& add) {
	return (fs::path(fileBuffer[index]) / add).wstring();
}

// Function to append an additional path component to the base path at a given index
void AppendPath(const int index, const std::wstring& add) {
	fileBuffer[index] = JoinPath(index, add);
}

// Function to combine a source path at a specific index with an additional path component
// and store the resulting path in the destination index
void CombinePath(const int destIndex, const int srcIndex, const std::wstring& add) {
	fileBuffer[destIndex] = JoinPath(srcIndex, add);
}

// Simplified Folder Browser
HRESULT BrowseFolder(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath) {
	fileBuffer[0].clear();

	IFileDialog* pfd = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) return E_FAIL;

	pfd->SetOptions(FOS_PICKFOLDERS);
	if (SUCCEEDED(pfd->Show(hwndOwner))) {
		IShellItem* psi = nullptr;
		if (SUCCEEDED(pfd->GetResult(&psi))) {
			PWSTR pszPath = nullptr;
			if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
				wcsncpy_s(pszFolderPath, cchFolderPath, pszPath, _TRUNCATE);
				CoTaskMemFree(pszPath);
				fileBuffer[0] = pszFolderPath;
			}
			psi->Release();
		}
	}
	pfd->Release();
	return S_OK;
}

// Download Helper
void URLDownload(const std::wstring& url, int idx, bool fromServer) {
	std::wstring targetUrl = fromServer ? L"https://lolsuite.org/repo/" + url : url;
	DeleteUrlCacheEntry(targetUrl.c_str());
	URLDownloadToFile(nullptr, targetUrl.c_str(), fileBuffer[idx].c_str(), 0, nullptr);
	auto zoneFile = fileBuffer[idx] + L":Zone.Identifier";
	if (fs::exists(zoneFile)) fs::remove(zoneFile);
}

void dx9Async(const std::vector<std::wstring>& files, size_t baseIndex) {
	std::thread([=]() {
		for (size_t i = 0; i < files.size(); ++i) {
			fileBuffer[baseIndex + i].clear();
			CombinePath(baseIndex + i, 158, files[i]);
			URLDownload(L"dx9/" + files[i], baseIndex + i, true);
		}
		}).detach(); // Detach thread to run asynchronously
}

size_t countFilesInDirectory(const std::wstring& directoryPath) {
	size_t count = 0;
	for (const auto& entry : fs::directory_iterator(directoryPath)) {
		if (entry.is_regular_file()) {
			++count;
		}
	}
	return count;
}

void waitForFileCount(const std::wstring& directoryPath, size_t expectedCount) {
	while (true) {
		size_t currentCount = countFilesInDirectory(directoryPath);
		if (currentCount >= expectedCount) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid busy-waiting
	}
}

// Combined function for executing a series of PowerShell commands sequentially
void CommandExecute(const std::vector<std::wstring>& commands) {
	for (const auto& command : commands) {
		// Prepare the full PowerShell command
		std::wstring fullCommand = L"powershell.exe -Command \"" + command + L"\"";

		// Set up the SHELLEXECUTEINFO structure
		SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
		sei.lpVerb = L"open";
		sei.lpFile = L"powershell.exe";
		sei.lpParameters = fullCommand.c_str();
		sei.nShow = SW_HIDE;
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;

		// Execute the command and wait for completion
		if (ShellExecuteEx(&sei)) {
			WaitForSingleObject(sei.hProcess, INFINITE);
			CloseHandle(sei.hProcess);
		}
	}
}


// Start a Process
void Start(const std::wstring& file, const std::wstring& params, bool wait) {
	SHELLEXECUTEINFO info = { sizeof(SHELLEXECUTEINFO) };
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.nShow = SW_SHOWNORMAL;
	info.lpFile = file.c_str();
	info.lpParameters = params.c_str();
	if (ShellExecuteEx(&info) && wait && info.hProcess) {
		SetPriorityClass(info.hProcess, HIGH_PRIORITY_CLASS);
		WaitForSingleObject(info.hProcess, INFINITE);
		CloseHandle(info.hProcess);
	}
}

// Terminate a Process
void Terminate(const std::wstring& processName) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;

	PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };
	for (BOOL hasProcess = Process32First(snapshot, &entry); hasProcess; hasProcess = Process32Next(snapshot, &entry)) {
		if (!wcscmp(entry.szExeFile, processName.c_str())) {
			HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
			if (processHandle) {
				TerminateProcess(processHandle, 0);
				CloseHandle(processHandle);
			}
			break;
		}
	}
	CloseHandle(snapshot);
}

// Check for 64-bit Process
bool IsProcess64Bit() {
	BOOL isWow64 = FALSE;
	USHORT processMachine, nativeMachine;

	auto fnIsWow64Process2 = reinterpret_cast<decltype(&IsWow64Process2)>(
		GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process2"));

	if (fnIsWow64Process2 && fnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)) {
		return processMachine != IMAGE_FILE_MACHINE_UNKNOWN;
	}

	auto fnIsWow64Process = reinterpret_cast<BOOL(WINAPI*)(HANDLE, PBOOL)>(
		GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process"));

	return fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &isWow64) && isWow64;
}

void manageGame(const std::wstring& game, bool restore) {
	if (game == L"leagueoflegends") {
		MessageBoxEx(nullptr, L"Select: C:\\Riot Games", L"LoLSuite", MB_OK, 0);
		BrowseFolder(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));

		const std::vector<std::wstring> processes = {
			L"LeagueClient.exe", L"LeagueClientUx.exe", L"LeagueClientUxRender.exe",
			L"League of Legends.exe", L"Riot Client.exe", L"RiotClientServices.exe",
			L"RiotClientCrashHandler.exe", L"LeagueCrashHandler64.exe"
		};

		for (const auto& process : processes) {
			Terminate(process);
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
			URLDownload(restore ? L"r/lol/" + file : file, index, true);
		}

		CombinePath(51, 0, L"Game");

		CombinePath(53, 51, L"D3DCompiler_47.dll");
		CombinePath(55, 51, L"tbb.dll");
		CombinePath(54, 0, L"d3dcompiler_47.dll");

		if (restore) {
			fs::remove(fileBuffer[55]);
		}
		else {
			URLDownload(L"tbb.dll", 55, true);
		}

		const auto d3dcompilerPath = restore ? L"r/lol/D3DCompiler_47.dll" :
			(IsProcess64Bit() ? L"D3DCompiler_47.dll_x64" : L"D3DCompiler_47.dll");

		URLDownload(d3dcompilerPath, 53, true);
		URLDownload(d3dcompilerPath, 54, true);

		Start(JoinPath(56, L"Riot Client.exe"), L"", false);
	}

	else if (game == L"dota2") {
		// Prompt the user to select the Dota 2 directory
		MessageBoxEx(nullptr,
			L"Select: C:\\Program Files (x86)\\Steam\\steamapps\\common\\dota 2 beta\\game",
			L"LoLSuite", MB_OK, 0);
		BrowseFolder(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));

		// Terminate any running Dota 2 process
		Terminate(L"dota2.exe");

		// Prepare paths and handle required files
		AppendPath(0, L"bin\\win64");
		CombinePath(1, 0, L"embree3.dll");
		URLDownload(restore ? L"r/dota2/embree3.dll" : L"embree4.dll", 1, true);

		// Launch Dota 2 via Steam
		Start(L"steam://rungameid/570//-high -dx11 -fullscreen/", L"", false);
	}

	else if (game == L"smite2") {
		// Prompt the user to select the SMITE 2 directory
		MessageBoxEx(nullptr,
			L"Select: C:\\Program Files (x86)\\Steam\\steamapps\\common\\SMITE 2\\Windows",
			L"LoLSuite", MB_OK, 0);
		BrowseFolder(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));

		// Terminate running SMITE 2 processes
		std::vector<std::wstring> processes = {
			L"Hemingway.exe",
			L"Hemingway-Win64-Shipping.exe"
		};
		for (const auto& process : processes) {
			Terminate(process);
		}

		// Prepare paths
		CombinePath(8, 0, L"Engine\\Binaries\\Win64");
		CombinePath(7, 0, L"Hemingway\\Binaries\\Win64");

		// Download required files
		CombinePath(1, 8, L"tbb.dll");
		URLDownload(restore ? L"r/smite2/tbb.dll" : L"tbb.dll", 1, true);

		CombinePath(2, 8, L"tbbmalloc.dll");
		URLDownload(restore ? L"r/smite2/tbbmalloc.dll" : L"tbbmalloc.dll", 2, true);

		CombinePath(4, 7, L"tbb.dll");
		CombinePath(5, 7, L"tbb12.dll");
		CombinePath(6, 7, L"tbbmalloc.dll");

		URLDownload(restore ? L"r/smite2/tbb.dll" : L"tbb.dll", 4, true);
		URLDownload(restore ? L"r/smite2/tbb12.dll" : L"tbb.dll", 5, true);
		URLDownload(restore ? L"r/smite2/tbbmalloc.dll" : L"tbbmalloc.dll", 6, true);

		Start(L"steam://rungameid/2437170", L"", false);
	}
	else if (game == L"minecraft")
	{
		for (const auto& process : mcprocesses)
			Terminate(process);

		// Define the Java executable path
		std::wstring javaPath = L"C:\\\\Program Files\\\\Java\\\\jdk-24\\\\bin\\\\javaw.exe";

		// Use getenv_s for safer environment variable retrieval
		const size_t bufferSize = MAX_PATH + 1; // Define buffer size for APPDATA
		char appdataBuffer[bufferSize];
		size_t retrievedSize = 0; // Variable to store the size of the retrieved string
		errno_t err = getenv_s(&retrievedSize, appdataBuffer, bufferSize, "APPDATA");

		// Combine paths using filesystem
		fs::path configPath = appdataBuffer;
		configPath /= ".minecraft";
		fs::remove_all(configPath);
		configPath /= "launcher_profiles.json";

		CommandExecute(commands_minecraft);

		// Wait for update
		Start(L"C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe", L"", false);
		while (!std::filesystem::exists(configPath)) {
			std::this_thread::sleep_for(std::chrono::seconds(10));
		}

		for (const auto& process : mcprocesses)
			Terminate(process);

		std::wifstream configFile(configPath);
		configFile.imbue(std::locale("en_US.UTF-8")); // Use UTF-8 locale directly without <codecvt>

		if (configFile.is_open()) {
			// Read the entire configuration file
			std::wstring configData((std::istreambuf_iterator<wchar_t>(configFile)),
				std::istreambuf_iterator<wchar_t>());
			configFile.close();

			std::wstringstream configStream(configData);
			std::wstring line;
			std::wstring updatedConfigData;

			while (std::getline(configStream, line)) {
				// Check for "javaDir" and "skipJreVersionCheck"
				if (line.find(L"\"javaDir\"") == std::wstring::npos &&
					line.find(L"\"skipJreVersionCheck\"") == std::wstring::npos) {
					// Include valid lines
					updatedConfigData.append(line).append(L"\n");
				}
			}

			// Save updated configuration
			fs::path updatedConfigPath = configPath.parent_path() / L"updated_config.txt";
			std::wofstream updatedConfigFile(updatedConfigPath);
			updatedConfigFile.imbue(std::locale("en_US.UTF-8"));
			updatedConfigFile << updatedConfigData;
			updatedConfigFile.close();


			// Process for both "latest-release" and "latest-snapshot"
			std::vector<std::wstring> types = { L"\"type\" : \"latest-release\"", L"\"type\" : \"latest-snapshot\"" };

			for (const auto& type : types) {
				size_t typePos = updatedConfigData.find(type);

				if (typePos != std::wstring::npos) {
					// Find the line before the type entry for "skipJreVersionCheck"
					size_t lineStartPos = updatedConfigData.rfind(L'\n', typePos); // Position of the newline before the target
					if (lineStartPos != std::wstring::npos) {
						// Prepare the "skipJreVersionCheck" entry
						std::wstring skipJreCheckEntry = L"      \"skipJreVersionCheck\" : true,\n";
						// Insert the entry one line above the type entry
						updatedConfigData.insert(lineStartPos + 1, skipJreCheckEntry);
					}

					// Calculate position to add "javaDir" 5 lines above the type entry
					size_t javaDirPos = typePos;
					for (int i = 0; i < 4; ++i) { // Adjust to move back 5 lines
						javaDirPos = updatedConfigData.rfind(L'\n', javaDirPos - 1);
						if (javaDirPos == std::wstring::npos) {
							break; // Stop if fewer than 5 lines above
						}
					}
					if (javaDirPos != std::wstring::npos) {
						// Prepare the "javaDir" key-value pair
						std::wstring javaDirEntry = L"      \"javaDir\" : \"" + javaPath + L"\",\n";
						// Insert "javaDir" entry 5 lines above
						updatedConfigData.insert(javaDirPos + 1, javaDirEntry);
					}
				}
			}

			// Write back to the configuration file
			std::wofstream outFile(configPath);
			outFile.imbue(std::locale("en_US.UTF-8")); // Use UTF-8 locale directly without codecvt
			outFile << updatedConfigData; // Write the updated configuration data
			outFile.close();
		}

		Start(L"C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe", L"", false);

	}
}

// Start or stop a Windows service
void ManageService(const std::wstring& serviceName, bool start) {
	SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!schSCManager) return;

	SC_HANDLE schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_START | SERVICE_STOP);
	if (!schService) {
		CloseServiceHandle(schSCManager);
		return;
	}

	if (start) {
		StartService(schService, 0, nullptr);
	}
	else {
		SERVICE_STATUS status;
		ControlService(schService, SERVICE_CONTROL_STOP, &status);
		while (status.dwCurrentState != SERVICE_STOPPED) {
			Sleep(1000);
			if (!QueryServiceStatus(schService, &status)) break;
		}
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

void CacheClean(const std::wstring& basePath, const std::vector<std::wstring>& patterns) {
	for (const auto& pattern : patterns) {
		WIN32_FIND_DATA findFileData;
		HANDLE hFind = FindFirstFile((fs::path(basePath) / pattern).c_str(), &findFileData);

		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				fs::remove(fs::path(basePath) / findFileData.cFileName);
			} while (FindNextFile(hFind, &findFileData) != 0);
			FindClose(hFind);
		}
	}
}

void manageTasks(const std::wstring& task)
{
	if (task == L"support")
	{
		SHEmptyRecycleBin(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);

		HMODULE dnsapi = LoadLibraryW(L"dnsapi.dll");
		DnsFlushResolverCacheFuncPtr DnsFlushResolverCache = (DnsFlushResolverCacheFuncPtr)GetProcAddress(dnsapi, "DnsFlushResolverCache");
		DnsFlushResolverCache();
		FreeLibrary(dnsapi);

		for (const auto& process : processes) {
			Terminate(process);
		}

		fileBuffer[158].clear();
		AppendPath(158, workdir);
		AppendPath(158, L"tmp");
		fs::remove_all(fileBuffer[158]);
		fs::create_directory(fileBuffer[158]);

		// Generate x86 and x64 file lists
		std::vector<std::wstring> dxx86_cab = generateFiles(dates, L"_x86.cab");
		std::vector<std::wstring> dxx64_cab = generateFiles(dates, L"_x64.cab");

		// Asynchronously download files
		dx9Async(dxx86_cab, 0);
		dx9Async(dxx64_cab, dxx86_cab.size());
		dx9Async(dxsetup, dxx86_cab.size() + dxx64_cab.size());

		// Wait for 157 files in the \tmp directory
		waitForFileCount(fileBuffer[158], 157);

		// Execute DXSETUP
		Start(JoinPath(158, L"DXSETUP.exe"), L"/silent", true); // Wait for finish

		fs::remove_all(fileBuffer[158]);

		// Generate uninstall commands
		std::vector<std::wstring> commands;
		for (const auto& app : apps_remove) {
			commands.push_back(uninstallCommand + app + optionsPurge);
		}

		CommandExecute(commands);
		commands.clear();
		// Generate install commands
		for (const auto& app : apps_install) {
			std::wstring command = installCommand + app;
			if (app == L"Blizzard.BattleNet") {
				command += L" --location \"C:\\Battle.Net\""; // Special case for Blizzard
			}
			command += optionsAccept;
			commands.push_back(command);

		}

		CommandExecute(commands);

		ManageService(L"W32Time", true);
		CommandExecute(commands_helper);

		// Stop services
		const std::vector<std::wstring> services = { L"wuauserv", L"BITS", L"CryptSvc" };
		for (const auto& service : services) {
			ManageService(service, false);
		}

		// Clean Explorer cache
		WCHAR localAppDataPath[MAX_PATH + 1];
		if (SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppDataPath) == S_OK) {
			std::vector<std::wstring> explorerPatterns = { L"thumbcache_*.db", L"iconcache_*.db", L"ExplorerStartupLog*.etl" };
			CacheClean((fs::path(localAppDataPath) / L"Microsoft/Windows/Explorer").wstring(), explorerPatterns);
		}

		// Clear SoftwareDistribution folder
		WCHAR windowsPath[MAX_PATH + 1];
		if (GetWindowsDirectory(windowsPath, MAX_PATH + 1)) {
			fs::path updateCachePath = fs::path(windowsPath) / L"SoftwareDistribution";
			if (fs::exists(updateCachePath)) {
				fs::remove_all(updateCachePath);
			}
		}

		// Restart services
		for (const auto& service : services) {
			ManageService(service, true);
		}
	}
}

void handleCommand(int cb, bool flag) {
	static const std::unordered_map<int, std::function<void()>> commandMap = {
		{0, [flag]() { manageGame(L"leagueoflegends", flag); }},
		{1, [flag]() { manageGame(L"dota2", flag); }},
		{2, [flag]() { manageGame(L"smite2", flag); }},
		{3, [flag]() { manageGame(L"minecraft", flag); }},
		{4, []() { manageTasks(L"support"); }}
	};

	if (auto it = commandMap.find(cb); it != commandMap.end()) {
		it->second();
		exit(0);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_COMMAND:
		if (HIWORD(wParam) == CBN_SELCHANGE) {
			cb = SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0);
		}

		switch (LOWORD(wParam)) {
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
	return RegisterClassEx(&wcex);
}


HWND CreateMainWindow(HINSTANCE hInstance, LPCWSTR className, int width, int height) {
	return CreateWindowEx(
		0,
		className,
		L"LoLSuite",
		WS_EX_LAYERED & ~WS_MAXIMIZEBOX,
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
		CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr);
	}
}

HWND CreateComboBox(HWND hWnd, HINSTANCE hInstance, int x, int y, int width, int height) {
	// Create the ComboBox
	return CreateWindowEx(
		0, // Extended window style
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
	HWND hWnd = CreateMainWindow(hInstance, L"LoLSuite", 350, 100); // Adjusted width and height
	if (!hWnd) {
		return FALSE;
	}

	// Initialize controls
	InitializeControls(hWnd, hInstance);

	const wchar_t* box[5] = {
	L"League of Legends",
	L"Dota 2",
	L"SMITE 2",
	L"Minecraft Java (x64)",
	L"Tweaks"
	};

	// Create and populate the ComboBox
	HWND combobox = CreateComboBox(hWnd, hInstance, 150, 20, 150, 300);
	PopulateComboBox(combobox, box, std::size(box)); // Use std::size to calculate the number of items

	// Clear clipboard content
	if (OpenClipboard(nullptr)) {
		EmptyClipboard();
		CloseClipboard();
	}

	// Show and update the main window
	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}
