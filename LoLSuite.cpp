#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <shellapi.h>
#include <TlHelp32.h>
#include <vector>
#include <filesystem>
#include <ShObjIdl_core.h>
#include "resource.h"

namespace fs = std::filesystem;
int cb = 0;
int rarecb = 0;
WCHAR szFolderPath[MAX_PATH + 1];
auto currentPath = fs::current_path();
constexpr int MAX_LOADSTRING = 100;
std::vector<std::wstring> v(158);
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
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

const wchar_t* box[7] = {
	L"League of Legends", L"DOTA2", L"Minecraft Java", L"Mesen2",
	L"Arcades", L"PC Booster + DirectX", L"Game Clients"
};
const wchar_t* rarebox[5] = {
	L"[XBLA] GoldenEye CE", L"[XBLA] Perfect Dark", L"[XBLA] Banjo Kazooie", L"[XBLA] Banjo Tooie", L"[XBLA] GoldenEye"
};


HRESULT FolderBrowser(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath)
{
	for (auto& path : v) path.clear();
	IFileDialog* pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (FAILED(hr)) return hr;
	DWORD dwOptions;
	hr = pfd->GetOptions(&dwOptions);
	if (FAILED(hr))
	{
		pfd->Release();
		return hr;
	}
	hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
	if (FAILED(hr))
	{
		pfd->Release();
		return hr;
	}
	hr = pfd->Show(hwndOwner);
	if (FAILED(hr))
	{
		pfd->Release();
		return hr;
	}
	IShellItem* psi = nullptr;
	hr = pfd->GetResult(&psi);
	if (SUCCEEDED(hr))
	{
		PWSTR pszPath = nullptr;
		hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
		if (SUCCEEDED(hr))
		{
			wcsncpy_s(pszFolderPath, cchFolderPath, pszPath, _TRUNCATE);
			CoTaskMemFree(pszPath);
		}
		psi->Release();
	}
	pfd->Release();
	v[0] = szFolderPath;
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


void Run(const std::wstring& lpFile, const std::wstring& lpParameters, bool wait)
{
	SHELLEXECUTEINFO sEInfo = {};
	sEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	sEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	sEInfo.nShow = SW_SHOWNORMAL;
	sEInfo.lpFile = lpFile.c_str();
	sEInfo.lpParameters = lpParameters.c_str();
	if (!ShellExecuteEx(&sEInfo))
	{
		return;
	}
	if (wait && sEInfo.hProcess != nullptr)
	{
		WaitForSingleObject(sEInfo.hProcess, INFINITE);
		CloseHandle(sEInfo.hProcess);
	}
}
void Term(const std::wstring& process_name)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;
	PROCESSENTRY32 processEntry = { sizeof(PROCESSENTRY32) };
	for (BOOL hasProcess = Process32First(snapshot, &processEntry); hasProcess; hasProcess = Process32Next(
		snapshot, &processEntry))
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
void unblock(const std::wstring& file)
{
	fs::remove(file + L":Zone.Identifier");
}
void dl(const std::wstring& url, int j, bool serv)
{
	std::wstring Url = serv ? L"https://lolsuite.org/files/" + url : url;
	URLDownloadToFile(nullptr, Url.c_str(), v[j].c_str(), 0, nullptr);
	unblock(v[j]);
}
bool ProccessIs64Bit()
{
	USHORT processMachine = 0;
	USHORT nativeMachine = 0;
	auto fnIsWow64Process2 = reinterpret_cast<decltype(&IsWow64Process2)>(
		GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process2"));
	if (!fnIsWow64Process2) return false;
	if (!fnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)) return false;
	return processMachine != IMAGE_FILE_MACHINE_UNKNOWN;
}

auto executeCommands = [](const std::vector<std::wstring>& commands)
	{
		for (const auto& cmd : commands)
		{
			_wsystem(cmd.c_str());
		}
	};

void manageGame(const std::wstring& game, bool restore)
{
	if (game == L"leagueoflegends") {
		MessageBoxEx(nullptr, L"Select Folder: C:\\Riot Games", L"LoLSuite", MB_OK, 0);
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
			dl(L"tbb.dll", 55, true); // Multi-Threaded
		}

		const std::wstring d3dcompilerPath = restore ? L"r/lol/D3DCompiler_47.dll" : (ProccessIs64Bit() ? L"6/D3DCompiler_47.dll" : L"D3DCompiler_47.dll");
		dl(d3dcompilerPath.c_str(), 53, true);
		dl(d3dcompilerPath.c_str(), 54, true);

		Run(JoinPath(56, L"Riot Client.exe"), L"", false);
	}
	if (game == L"dota2") {
		MessageBoxEx(nullptr, L"Select Folder: C:\\Program Files (x86)\\Steam\\steamapps", L"LoLSuite", MB_OK, 0);
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
		Term(L"dota2.exe");

		AppendPath(0, L"common\\dota 2 beta\\game\\bin\\win64");
		CombinePath(1, 0, L"embree3.dll");

		unblock(JoinPath(0, L"dota2.exe"));
		dl(restore ? L"r/dota2/embree3.dll" : L"6/embree4.dll", 1, true);

		Run(L"steam://rungameid/570//-high/", L"", false);
	}
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
	wcex.lpszClassName = szWindowClass;
	wcex.lpszMenuName = nullptr;
	wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON));
	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;
	HWND hWnd = CreateWindowExW(
		0, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 150,
		nullptr, nullptr, hInstance, nullptr
	);
	if (!hWnd) return FALSE;

	std::vector<std::tuple<DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HMENU>> controls = {
		{WS_EX_TOOLWINDOW, L"BUTTON", L"Patch", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 20, 60, 30, reinterpret_cast<HMENU>(1)},
		{0, L"BUTTON", L"Restore", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 75, 20, 60, 30, reinterpret_cast<HMENU>(2)},
		{0, L"BUTTON", L"XBLA", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 20, 60, 30, reinterpret_cast<HMENU>(3)}
	};

	for (const auto& [dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hMenu] : controls) {
		if (!CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr))
			return FALSE;
	}

	HWND hwndCombo1 = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWN | WS_CHILD | WS_VISIBLE, 260, 20, 200, 200, hWnd, NULL, hInstance, NULL);
	HWND hwndCombo2 = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWN | WS_CHILD | WS_VISIBLE, 260, 50, 200, 200, hWnd, NULL, hInstance, NULL);

	for (const auto& str : box)
		SendMessage(hwndCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	SendMessageW(hwndCombo1, CB_SETCURSEL, 0, 0);

	for (const auto& str : rarebox)
		SendMessage(hwndCombo2, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	SendMessageW(hwndCombo2, CB_SETCURSEL, 0, 0);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

void manageTasks(const std::wstring& task)
{

	if (task == L"JDK")
	{
		executeCommands({
	L"winget source update",
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
	else if (task == L"mame")
	{

		for (auto& path : v) path.clear();

		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"7z.exe"},
			{1, L"HBMAME.7z"},
			{2, L"MAME.exe"},
			{3, L"FBNeo.zip"},
			{4, L"FBNeo_support.7z"}
		};

		for (const auto& [index, subPath] : paths) {
			AppendPath(index, currentPath);
			AppendPath(index, subPath);
		}

		std::vector<std::tuple<std::wstring, int, bool>> downloads;
		std::vector<std::wstring> Commands;

		if (ProccessIs64Bit()) {
			Commands = {
				L"winget source update",
				L"winget uninstall Microsoft.VCRedist.2015+.x64 --purge -h",
				L"winget install Microsoft.VCRedist.2015+.x64 --accept-package-agreements"
			};

			downloads = {
				{L"7z.exe", 0, true},
				{L"https://hbmame.1emulation.com/hbmameui21.7z", 1, false},
				{L"https://github.com/mamedev/mame/releases/download/mame0273/mame0273b_64bit.exe", 2, false},
				{L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x64.zip", 3, false}
			};
		}
		else {
			Commands = {
				L"winget source update",
				L"winget uninstall Microsoft.VCRedist.2015+.x86 --purge -h",
				L"winget install Microsoft.VCRedist.2015+.x86 --accept-package-agreements"
			};

			downloads = {
				{L"7z.exe", 0, true},
				{L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x32.zip", 3, false}
			};
		}

		for (const auto& [url, index, flag] : downloads) {
			dl(url, index, flag);
		}
		executeCommands(Commands);

		fs::remove_all(L"HBMAME");
		fs::remove_all(L"MAME");
		fs::remove_all(L"FBNeo");
		dl(L"support.7z", 4, true);

		for (const auto& cmd : {
			L"x HBMAME.7z -oHBMAME -y",
			L"x MAME.exe -oMAME -y",
			L"x FBNeo.zip -oFBNeo -y",
			L"x FBNeo_support.7z -oFBNeo\\support -y"
			}) {
			Run(v[0], cmd, true);
		}

		for (int i : {0, 1, 2, 3, 4}) {
			fs::remove(v[i]);
		}
	}
	else if (task == L"mesen")
	{

		for (int i : {0, 1, 2}) v[i].clear();
		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"7z.exe"},
			{1, L"Mesen.zip"},
			{2, L"Mesen\\Mesen.exe"}
		};
		for (const auto& [index, subPath] : paths)
		{
			AppendPath(index, currentPath);
			AppendPath(index, subPath);
		}
		executeCommands({
			L"winget source update",
			L"winget uninstall Microsoft.DotNet.DesktopRuntime.8 --purge -h",
			L"winget install Microsoft.DotNet.DesktopRuntime.8 --accept-package-agreements"
			});
		dl(L"7z.exe", 0, true);
		dl(L"https://nightly.link/SourMesen/Mesen2/workflows/build/master/Mesen%20%28Windows%20-%20net8.0%29.zip", 1, false);
		fs::remove_all(L"Mesen");
		Run(v[0], L"x Mesen.zip -oMesen -y", true);
		for (int i : {0, 1})fs::remove(v[i]);
		Run(v[2], L"", false);
	}
	else if (task == L"support")
	{

		const std::vector<std::wstring> processes = {L"MSPCManager.exe", L"Powershell.exe", L"OpenConsole.exe", L"cmd.exe", L"DXSETUP.exe"};

		for (const auto& process : processes) {
			Term(process);
		}

		const std::vector<std::wstring> dxx86_cab = {
			L"Apr2005_d3dx9_25_x86.cab", L"Apr2006_d3dx9_30_x86.cab", L"Apr2006_MDX1_x86.cab",
			L"Apr2006_MDX1_x86_Archive.cab", L"Apr2006_XACT_x86.cab", L"Apr2006_xinput_x86.cab",
			L"APR2007_d3dx9_33_x86.cab", L"APR2007_d3dx10_33_x86.cab", L"APR2007_XACT_x86.cab",
			L"APR2007_xinput_x86.cab", L"Aug2005_d3dx9_27_x86.cab", L"AUG2006_XACT_x86.cab",
			L"AUG2006_xinput_x86.cab", L"AUG2007_d3dx9_35_x86.cab", L"AUG2007_d3dx10_35_x86.cab",
			L"AUG2007_XACT_x86.cab", L"Aug2008_d3dx9_39_x86.cab", L"Aug2008_d3dx10_39_x86.cab",
			L"Aug2008_XACT_x86.cab", L"Aug2008_XAudio_x86.cab", L"Aug2009_D3DCompiler_42_x86.cab",
			L"Aug2009_d3dcsx_42_x86.cab", L"Aug2009_d3dx9_42_x86.cab", L"Aug2009_d3dx10_42_x86.cab",
			L"Aug2009_d3dx11_42_x86.cab", L"Aug2009_XACT_x86.cab", L"Aug2009_XAudio_x86.cab",
			L"Dec2005_d3dx9_28_x86.cab", L"DEC2006_d3dx9_32_x86.cab", L"DEC2006_d3dx10_00_x86.cab",
			L"DEC2006_XACT_x86.cab", L"Feb2005_d3dx9_24_x86.cab", L"Feb2006_d3dx9_29_x86.cab",
			L"Feb2006_XACT_x86.cab", L"FEB2007_XACT_x86.cab", L"Feb2010_X3DAudio_x86.cab",
			L"Feb2010_XACT_x86.cab", L"Feb2010_XAudio_x86.cab", L"Jun2005_d3dx9_26_x86.cab",
			L"JUN2006_XACT_x86.cab", L"JUN2007_d3dx9_34_x86.cab", L"JUN2007_d3dx10_34_x86.cab",
			L"JUN2007_XACT_x86.cab", L"JUN2008_d3dx9_38_x86.cab", L"JUN2008_d3dx10_38_x86.cab",
			L"JUN2008_X3DAudio_x86.cab", L"JUN2008_XACT_x86.cab", L"JUN2008_XAudio_x86.cab",
			L"Jun2010_D3DCompiler_43_x86.cab", L"Jun2010_d3dcsx_43_x86.cab", L"Jun2010_d3dx9_43_x86.cab",
			L"Jun2010_d3dx10_43_x86.cab", L"Jun2010_d3dx11_43_x86.cab", L"Jun2010_XACT_x86.cab",
			L"Jun2010_XAudio_x86.cab", L"Mar2008_d3dx9_37_x86.cab", L"Mar2008_d3dx10_37_x86.cab",
			L"Mar2008_X3DAudio_x86.cab", L"Mar2008_XACT_x86.cab", L"Mar2008_XAudio_x86.cab",
			L"Mar2009_d3dx9_41_x86.cab", L"Mar2009_d3dx10_41_x86.cab", L"Mar2009_X3DAudio_x86.cab",
			L"Mar2009_XACT_x86.cab", L"Mar2009_XAudio_x86.cab", L"Nov2007_d3dx9_36_x86.cab",
			L"Nov2007_d3dx10_36_x86.cab", L"NOV2007_X3DAudio_x86.cab", L"NOV2007_XACT_x86.cab",
			L"Nov2008_d3dx9_40_x86.cab", L"Nov2008_d3dx10_40_x86.cab", L"Nov2008_X3DAudio_x86.cab",
			L"Nov2008_XACT_x86.cab", L"Nov2008_XAudio_x86.cab", L"Oct2005_xinput_x86.cab",
			L"OCT2006_d3dx9_31_x86.cab", L"OCT2006_XACT_x86.cab"
		};

		const std::vector<std::wstring> dxx64_cab = {
			L"Apr2005_d3dx9_25_x64.cab", L"Apr2006_d3dx9_30_x64.cab", L"Apr2006_XACT_x64.cab",
			L"Apr2006_xinput_x64.cab", L"APR2007_d3dx9_33_x64.cab", L"APR2007_d3dx10_33_x64.cab",
			L"APR2007_XACT_x64.cab", L"APR2007_xinput_x64.cab", L"Aug2005_d3dx9_27_x64.cab",
			L"AUG2006_XACT_x64.cab", L"AUG2006_xinput_x64.cab", L"AUG2007_d3dx9_35_x64.cab",
			L"AUG2007_d3dx10_35_x64.cab", L"AUG2007_XACT_x64.cab", L"Aug2008_d3dx9_39_x64.cab",
			L"Aug2008_d3dx10_39_x64.cab", L"Aug2008_XACT_x64.cab", L"Aug2008_XAudio_x64.cab",
			L"Aug2009_D3DCompiler_42_x64.cab", L"Aug2009_d3dcsx_42_x64.cab", L"Aug2009_d3dx9_42_x64.cab",
			L"Aug2009_d3dx10_42_x64.cab", L"Aug2009_d3dx11_42_x64.cab", L"Aug2009_XACT_x64.cab",
			L"Aug2009_XAudio_x64.cab", L"Dec2005_d3dx9_28_x64.cab", L"DEC2006_d3dx9_32_x64.cab",
			L"DEC2006_d3dx10_00_x64.cab", L"DEC2006_XACT_x64.cab", L"Feb2005_d3dx9_24_x64.cab",
			L"Feb2006_d3dx9_29_x64.cab", L"Feb2006_XACT_x64.cab", L"FEB2007_XACT_x64.cab",
			L"Feb2010_X3DAudio_x64.cab", L"Feb2010_XACT_x64.cab", L"Feb2010_XAudio_x64.cab",
			L"Jun2005_d3dx9_26_x64.cab", L"JUN2006_XACT_x64.cab", L"JUN2007_d3dx9_34_x64.cab",
			L"JUN2007_d3dx10_34_x64.cab", L"JUN2007_XACT_x64.cab", L"JUN2008_d3dx9_38_x64.cab",
			L"JUN2008_d3dx10_38_x64.cab", L"JUN2008_X3DAudio_x64.cab", L"JUN2008_XACT_x64.cab",
			L"JUN2008_XAudio_x64.cab", L"Jun2010_D3DCompiler_43_x64.cab", L"Jun2010_d3dcsx_43_x64.cab",
			L"Jun2010_d3dx9_43_x64.cab", L"Jun2010_d3dx10_43_x64.cab", L"Jun2010_d3dx11_43_x64.cab",
			L"Jun2010_XACT_x64.cab", L"Jun2010_XAudio_x64.cab", L"Mar2008_d3dx9_37_x64.cab",
			L"Mar2008_d3dx10_37_x64.cab", L"Mar2008_X3DAudio_x64.cab", L"Mar2008_XACT_x64.cab",
			L"Mar2008_XAudio_x64.cab", L"Mar2009_d3dx9_41_x64.cab", L"Mar2009_d3dx10_41_x64.cab",
			L"Mar2009_X3DAudio_x64.cab", L"Mar2009_XACT_x64.cab", L"Mar2009_XAudio_x64.cab",
			L"Nov2007_d3dx9_36_x64.cab", L"Nov2007_d3dx10_36_x64.cab", L"NOV2007_X3DAudio_x64.cab",
			L"NOV2007_XACT_x64.cab", L"Nov2008_d3dx9_40_x64.cab", L"Nov2008_d3dx10_40_x64.cab",
			L"Nov2008_X3DAudio_x64.cab", L"Nov2008_XACT_x64.cab", L"Nov2008_XAudio_x64.cab",
			L"Oct2005_xinput_x64.cab", L"OCT2006_d3dx9_31_x64.cab", L"OCT2006_XACT_x64.cab"
		};

		const std::vector<std::wstring> dxsetup_files = {
			L"DSETUP.dll", L"dsetup32.dll", L"dxdllreg_x86.cab", L"DXSETUP.exe", L"dxupdate.cab"
		};

		v[82].clear();
		AppendPath(82, currentPath);
		AppendPath(82, L"temp_dx");
		fs::create_directory(v[82]);

		auto download_files = [&](const std::vector<std::wstring>& files) {
			for (size_t i = 0; i < files.size(); ++i) {
				v[i].clear();
				CombinePath(i, 82, files[i]);
				dl(L"dx9/" + files[i], i, true);
			}
			};

		download_files(dxx86_cab);

		if (ProccessIs64Bit()) {
			download_files(dxx64_cab);
		}

		download_files(dxsetup_files);

		Run(v[3], L"/silent", true);

		fs::remove_all(v[82]);

		executeCommands({
			L"powercfg /hibernate off",
			L"winget source update",
			L"winget uninstall Microsoft.PCManager --purge -h",
			L"winget uninstall Microsoft.WindowsTerminal --purge -h",
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
			L"winget uninstall Microsoft.VCRedist.2005.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2008.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2010.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2012.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2013.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2015+.x64 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2005.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2008.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2010.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2012.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2013.x86 --purge -h",
			L"winget uninstall Microsoft.VCRedist.2015+.x86 --purge -h",
			L"winget install Microsoft.WindowsTerminal --accept-package-agreements",
			L"winget install Microsoft.PowerShell --accept-package-agreements",
			L"winget install Microsoft.EdgeWebView2Runtime --accept-package-agreement",
			L"winget install Microsoft.PCManager --accept-package-agreement",
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
			L"winget install Microsoft.VCRedist.2005.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2015+.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2005.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2015+.x86 --accept-package-agreements",
			L"dir 'C:\\' -Recurse | Unblock-File"
			});
	}
	else if (task == L"XBLA")
	{
		Term(L"xenia.exe");
		Term(L"xenia_canary.exe");
		fs::remove_all(L"XBLA");

		for (auto& path : v) path.clear();

		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"GECE.7z"}, {1, L"7z.exe"}, {2, L"XBLA.zip"}, {3, L"XBLA\\LICENSE"},
			{4, L"XBLA\\xenia_canary.exe"}, {5, L"XBLA\\GECE\\defaultCE.xex"}, {6, L"XBLA.7z"},
			{7, L"PD.7z"}, {8, L"BK.7z"}, {9, L"BT.7z"}, {10, L"XBLA\\8292DB976888C5DCD68C695F11B3DFED5F4512E858 --license_mask -1"},
			{11, L"XBLA\\DA78E477AA5E31A7D01AE8F84109FD4BF89E49E858 --license_mask -1"},
			{12, L"XBLA\\ABB9CAB336175357D09F2D922735D23C62F90DDD58 --license_mask -1"}, {13, L"Bean.7z"},
			{14, L"XBLA\\30BA92710985645EF623D4A6BA9E8EFFAEC62617"}
		};

		for (const auto& [index, subPath] : paths) {
			AppendPath(index, currentPath);
			AppendPath(index, subPath);
		}

		dl(L"7z.exe", 1, true);
		dl(L"https://github.com/xenia-canary/xenia-canary/releases/download/experimental/xenia_canary.zip", 2, false);
		dl(L"XBLA.7z", 6, true);

		// Extract downloaded archives
		const std::vector<std::wstring> commands = { L"x XBLA.zip -oXBLA -y", L"x XBLA.7z -oXBLA -y" };
		for (const auto& cmd : commands) {
			Run(v[1], cmd, true);
		}

		// Helper function to download and extract files
		auto download_and_extract = [&](const std::wstring& file, int index, const std::wstring& extract_path, int run_index, const std::wstring& run_command) {
			dl(file, index, true);
			Run(v[1], L"x " + file + L" -o" + extract_path + L" -y", true);
			Run(v[4], run_command, false);
			};

		// Download, extract, and run based on rarecb value
		switch (rarecb) {
		case 0: download_and_extract(L"GECE.7z", 0, L"XBLA\\GECE", 5, v[5]); break;
		case 1: download_and_extract(L"PD.7z", 7, L"XBLA", 10, v[10]); break;
		case 2: download_and_extract(L"BK.7z", 8, L"XBLA", 11, v[11]); break;
		case 3: download_and_extract(L"BT.7z", 9, L"XBLA", 12, v[12]); break;
		case 4: download_and_extract(L"Bean.7z", 13, L"XBLA", 14, v[14]); break;
		}

		// Remove specified files
		const std::vector<int> indices_to_remove = { 0, 1, 2, 3, 6, 7, 8, 9, 13 };
		for (int i : indices_to_remove) {
			fs::remove(v[i]);
		}
	}
	else if (task == L"gameclients")
	{
		executeCommands({
	L"winget source update",
	L"winget uninstall Valve.Steam --purge -h",
	L"winget uninstall ElectronicArts.EADesktop --purge -h",
	L"winget uninstall ElectronicArts.Origin --purge -h",
	L"winget uninstall EpicGames.EpicGamesLauncher --purge -h",
	L"winget uninstall Blizzard.BattleNet --purge -h",
	L"winget install Valve.Steam --accept-package-agreements",
	L"winget install ElectronicArts.EADesktop --accept-package-agreements",
	L"winget install EpicGames.EpicGamesLauncher --accept-package-agreements",
	L"winget install Blizzard.BattleNet --accept-package-agreements"
			});
	}
}

void handleCommand(int cb, bool flag)
{
	std::vector<std::function<void()>> commands = {
		[flag]() { manageGame(L"leagueoflegends", flag); },
		[flag]() { manageGame(L"dota2", flag); },
		[]() { manageTasks(L"JDK"); },
		[]() { manageTasks(L"mesen"); },
		[]() { manageTasks(L"mame"); },
		[]() { manageTasks(L"support"); },
		[]() { manageTasks(L"gameclients"); }
	};
	if (cb >= 0 && cb < commands.size())
	{
		commands[cb]();
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
			rarecb = SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0);
		}
		switch (LOWORD(wParam))
		{
		case 1:
			handleCommand(cb, false);
			break;
		case 2:
			handleCommand(cb, true);
			break;
		case 3:
			manageTasks(L"XBLA");
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
	// Ensure only one instance of the application is running
	LimitSingleInstance GUID(L"Global\\{L0LSU1T3-BYL0LSU1T3@G17HUB-V3RYR4ND0M4NDR4R3MUCHW0W}");
	if (LimitSingleInstance::AnotherInstanceRunning())
		return 0;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Load application title and window class name
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_BUFFER, szWindowClass, MAX_LOADSTRING);

	// Register the window class
	MyRegisterClass(hInstance);

	// Initialize the application instance
	if (!InitInstance(hInstance, nShowCmd))
		return FALSE;

	// Load the accelerator table
	HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_BUFFER));
	MSG msg;

	// Main message loop
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return static_cast<int>(msg.wParam);
}