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
static int cb = 0;
static int rarecb = 0;
WCHAR szFolderPath[MAX_PATH + 1];
auto currentPath = fs::current_path();
constexpr auto MAX_LOADSTRING = 100;

std::vector<std::wstring> v(58);

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const wchar_t* box[7] = {
	L"League of Legends", L"DOTA2", L"Minecraft", L"Mesen Multi-Emulator",
	L"Arcade Pack", L"AIO Pack", L"Game-Client Pack"
};
const wchar_t* rarebox[5] = {
	L"[XBLA] GoldenEye CE", L"[XBLA] Perfect Dark", L"[XBLA] Banjo Kazooie", L"[XBLA] Banjo Tooie", L"[XBLA] GoldenEye"
};

HRESULT FolderBrowser(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath)
{
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

auto clearAndAppend = [](int index, const std::wstring& path)
	{
		v[index].clear();
		AppendPath(index, fs::current_path());
		AppendPath(index, path);
	};
auto executeCommands = [](const std::vector<std::wstring>& commands)
	{
		for (const auto& cmd : commands)
		{
			_wsystem(cmd.c_str());
		}
	};

void dx9()
{
	clearAndAppend(0, L"dxwebsetup.exe");
	dl(L"https://download.microsoft.com/download/1/7/1/1718CCC4-6315-4D8E-9543-8E28A4E18C4C/dxwebsetup.exe", 0, false);
	Run(v[0], L"/Q", true);
	fs::remove(v[0]);
}

void manageGame(const std::wstring& game, bool restore)
{
	if (game == L"leagueoflegends") {
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
		const wchar_t* processes[] = {
			L"LeagueClient.exe", L"LeagueClientUx.exe", L"LeagueClientUxRender.exe",
			L"League of Legends.exe", L"Riot Client.exe", L"RiotClientServices.exe"
		};
		for (const auto& process : processes) {
			Term(process);
		}
		CombinePath(56, 0, L"Riot Client\\RiotClientElectron");
		CombinePath(57, 56, L"d3dcompiler_47.dll");
		const wchar_t* baseFiles[] = {
			L"concrt140.dll", L"d3dcompiler_47.dll", L"msvcp140.dll", L"msvcp140_1.dll",
			L"msvcp140_2.dll", L"msvcp140_codecvt_ids.dll", L"ucrtbase.dll",
			L"vcruntime140.dll", L"vcruntime140_1.dll"
		};
		AppendPath(0, L"League of Legends");
		for (int i = 0; i < 9; ++i) {
			CombinePath(42 + i, 0, baseFiles[i]);
		}
		CombinePath(51, 0, L"Game");
		unblock(JoinPath(51, L"League of Legends.exe"));
		const wchar_t* gameFiles[] = {
			L"D3DCompiler_47.dll", L"tbb.dll"
		};
		for (int i = 0; i < 2; ++i) {
			CombinePath(52 + i, 51, gameFiles[i]);
		}
		CombinePath(55, 51, gameFiles[1]);
		auto downloadFiles = [&](const wchar_t* prefix, bool deletetbb = false) {
			for (int i = 0; i < 9; ++i) {
				dl(std::wstring(prefix) + baseFiles[i], 42 + i, true);
			}
			for (int i = 0; i < 2; ++i) {
				dl(std::wstring(prefix) + gameFiles[i], 52 + i, true);
			}
			if (deletetbb) {
				fs::remove(v[55]);
			}
			};
		if (restore) {
			downloadFiles(L"r/lol/", true);
		}
		else {
			downloadFiles(L"", false);
			if (ProccessIs64Bit()) {
				dl(L"6/D3DCompiler_47.dll", 52, true);
				dl(L"6/D3DCompiler_47.dll", 53, true);
			}
			else {
				dl(L"D3DCompiler_47.dll", 52, true);
				dl(L"D3DCompiler_47.dll", 53, true);
			}
			dl(L"tbb.dll", 55, true);
			dl(L"D3DCompiler_47.dll", 57, true);
		}
		dx9();
		Run(JoinPath(56, L"Riot Client.exe"), L"", false);
		exit(0);
	}
	else if (game == L"dota2") {
		FolderBrowser(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
		Term(L"dota2.exe");
		AppendPath(0, L"game\\bin\\win64");
		CombinePath(1, 0, L"embree3.dll");
		unblock(JoinPath(0, L"dota2.exe"));
		dl(restore ? L"r/dota2/embree3.dll" : L"6/embree4.dll", 1, true);
		dx9();
		Run(L"steam://rungameid/570//-high/", L"", false);
		exit(0);
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
		{WS_EX_TOOLWINDOW, L"BUTTON", L"Patch", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 20, 60, 30, reinterpret_cast<HMENU>(1)},
		{0, L"BUTTON", L"Restore", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 75, 20, 60, 30, reinterpret_cast<HMENU>(2)},
		{0, L"BUTTON", L"XBLA", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 20, 60, 30, reinterpret_cast<HMENU>(3)}
	};

	for (const auto& [dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hMenu] : controls) {
		HWND hControl = CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr);
		if (!hControl) return FALSE;
	}

	HWND hwndCombo1, hwndCombo2;
	hwndCombo1 = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWN | WS_CHILD | WS_VISIBLE, 260, 20, 200, 200, hWnd, NULL, hInstance, NULL);
	hwndCombo2 = CreateWindow(L"COMBOBOX", L"", CBS_DROPDOWN | WS_CHILD | WS_VISIBLE, 260, 50, 200, 200, hWnd, NULL, hInstance, NULL);

	for (const auto& str : box) {
		SendMessage(hwndCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	}
	SendMessageW(hwndCombo1, CB_SETCURSEL, 0, 0);

	for (const auto& str : rarebox) {
		SendMessage(hwndCombo2, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	}
	SendMessageW(hwndCombo2, CB_SETCURSEL, 0, 0);


	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

void manageTasks(const std::wstring& task)
{

	if (task == L"JDK")
	{
		std::vector<std::wstring> PreCommands = {
			L"winget source update",
			L"winget uninstall Mojang.MinecraftLauncher",
			L"winget uninstall Oracle.JavaRuntimeEnvironment",
			L"winget uninstall Oracle.JDK.23",
			L"winget uninstall Oracle.JDK.22",
			L"winget uninstall Oracle.JDK.21",
			L"winget uninstall Oracle.JDK.20",
			L"winget uninstall Oracle.JDK.19",
			L"winget uninstall Oracle.JDK.18",
			L"winget uninstall Oracle.JDK.17"
		};
		executeCommands(PreCommands);

		std::vector<std::wstring> PostCommands = {
			L"winget install Mojang.MinecraftLauncher",
			L"winget install Oracle.JavaRuntimeEnvironment"
		};
		executeCommands(PostCommands);

		v[0].clear();
		AppendPath(0, fs::current_path());
		AppendPath(0, L"jdk-23_windows-x64_bin.exe");
		dl(L"https://download.oracle.com/java/23/latest/jdk-23_windows-x64_bin.exe", 0, false);
		Run(v[0], L"", true);
		fs::remove_all(v[0]);

		MessageBoxEx(
			nullptr,
			L"Minecraft Launcher > Minecraft : Java Edition > Installations > Latest Release > Edit > More Options > Java Executable > Browse > <drive>:\\Program Files\\Java\\jdk-23\\bin\\javaw.exe",
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
			{3, L"FBNeo.zip" },
			{4, L"FBNeo_support.7z"}
		};

		std::vector<std::tuple<std::wstring, int, bool>> downloads;

		for (const auto& [index, subPath] : paths)
		{
			AppendPath(index, currentPath);
			AppendPath(index, subPath);
		}

		if (ProccessIs64Bit())
		{
			_wsystem(L"winget uninstall Microsoft.VCRedist.2015+.x64 --purge -h");
			_wsystem(L"winget install Microsoft.VCRedist.2015+.x64 --accept-package-agreements");
			downloads = {
			{L"7z.exe", 0, true},
			{L"https://hbmame.1emulation.com/hbmameui21.7z", 1, false},
			{L"https://github.com/mamedev/mame/releases/download/mame0272/mame0272b_64bit.exe", 2, false},
			{L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x64.zip", 3 , false},
			{L"https://lolsuite.org/funz/support.7z", 4, false}
			};
		}
		else
		{
			_wsystem(L"winget uninstall Microsoft.VCRedist.2015+.x86 --purge -h");
			_wsystem(L"winget install Microsoft.VCRedist.2015+.x86 --accept-package-agreements");
			downloads = {
			{L"7z.exe", 0, true},
			{L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x32.zip", 3, false},
			{L"https://lolsuite.org/funz/support.7z", 4, false}
			};
		}

		for (const auto& [url, index, flag] : downloads)
		{
			dl(url, index, flag);
		}

		fs::remove_all(L"HBMAME");
		fs::remove_all(L"MAME");
		fs::remove_all(L"FBNeo");
		fs::create_directory(L"HBMAME");
		fs::create_directory(L"MAME");
		fs::create_directory(L"FBNeo");

		for (const auto& cmd : { L"x HBMAME.7z -oHBMAME -y", L"x MAME.exe -oMAME -y", L"x FBNeo.zip -oFBNeo -y", L"x FBNeo_support.7z -oFBNeo\\support -y" })
		{
			Run(v[0], cmd, true);
		}
		for (int i : {0, 1, 2, 3, 4})
		{
			fs::remove(v[i]);
		}
		dx9();
		fs::remove(v[0]);
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

		_wsystem(L"winget uninstall Microsoft.DotNet.DesktopRuntime.8 --purge -h");
		_wsystem(L"winget install Microsoft.DotNet.DesktopRuntime.8 --accept-package-agreements");
		dl(L"7z.exe", 0, true);
		dl(L"https://nightly.link/SourMesen/Mesen2/workflows/build/master/Mesen%20%28Windows%20-%20net8.0%29.zip", 1, false);
		fs::remove_all(L"Mesen");
		fs::create_directory(L"Mesen");
		Run(v[0], L"x Mesen.zip -oMesen -y", true);
		for (int i : {0, 1})fs::remove_all(v[i]);
		Run(v[2], L"", false);
		dx9();
		exit(0);
	}
	else if (task == L"support")
	{
		Term(L"MSPCManager.exe");
		Term(L"Powershell.exe");
		Term(L"cmd.exe");

		std::vector<std::wstring> PreCommands = {
			L"powercfg /hibernate off",
			L"winget source update",
			L"winget uninstall Microsoft.PCManager",
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
			L"winget uninstall 9PB0TRCNRHFX --purge -h",
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
			L"winget uninstall Microsoft.VCRedist.2015+.x86 --purge -h"
		};

		std::vector<std::wstring> installCommands = {
			L"winget install Microsoft.WindowsTerminal --accept-package-agreements",
			L"winget install Microsoft.PowerShell --accept-package-agreements",
			L"winget install Microsoft.EdgeWebView2Runtime --accept-package-agreement",
			L"winget install Microsoft.PCManager",
			L"winget install 9NQPSL29BFFF --accept-package-agreements",
			L"winget install 9PB0TRCNRHFX --accept-package-agreements",
			L"winget install 9N95Q1ZZPMH4 --accept-package-agreements",
			L"winget install 9NCTDW2W1BH8 --accept-package-agreements",
			L"winget install 9MVZQVXJBQ9V --accept-package-agreements",
			L"winget install 9PMMSR1CGPWG --accept-package-agreements",
			L"winget install 9N4D0MSMP0PT --accept-package-agreements",
			L"winget install 9PG2DK419DRG --accept-package-agreements",
			L"winget install 9PB0TRCNRHFX --accept-package-agreements",
			L"winget install 9N5TDP8VCMHS --accept-package-agreements",
			L"winget install 9PCSD6N03BKV --accept-package-agreements" ,
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
			L"dir 'C:\' -Recurse | Unblock-File"
		};
		executeCommands(PreCommands);
		executeCommands(installCommands);
		dx9();
		exit(0);
	}
	else if (task == L"XBLA")
	{
		Term(L"xenia.exe");
		Term(L"xenia_canary.exe");
		for (auto& path : v) path.clear();
		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"Bean.7z"},
			{1, L"7z.exe"},
			{2, L"XBLA.zip"},
			{3, L"XBLA\\LICENSE"},
			{4, L"XBLA\\xenia_canary.exe"},
			{5, L"Bean\\defaultCE.xex"},
			{6, L"XBLA.7z"},
			{7, L"PD.7z"},
			{8, L"BK.7z"},
			{9, L"BT.7z"},
			{10, L"PD\\8292DB976888C5DCD68C695F11B3DFED5F4512E858 --license_mask -1"},
			{11, L"BK\\DA78E477AA5E31A7D01AE8F84109FD4BF89E49E858 --license_mask -1"},
			{12, L"BT\\ABB9CAB336175357D09F2D922735D23C62F90DDD58 --license_mask -1"},
			{13, L"BeanOG.7z"},
			{14, L"BeanOG\\30BA92710985645EF623D4A6BA9E8EFFAEC62617 --license_mask -1"},
		};
		for (const auto& [index, subPath] : paths)
		{
			AppendPath(index, currentPath);
			AppendPath(index, subPath);
		}
		dl(L"7z.exe", 1, true);
		dl(L"https://github.com/xenia-canary/xenia-canary/releases/download/experimental/xenia_canary.zip", 2, false);
		dl(L"https://lolsuite.org/funz/XBLA.7z", 6, false);

		std::vector<std::wstring> commands = { L"x XBLA.zip -oXBLA -y", L"x XBLA.7z -oXBLA -y" };
		for (const auto& cmd : commands)
		{
			Run(v[1], cmd, true);
		}
		dx9();
		switch (rarecb)
		{
		case 0:
			dl(L"https://lolsuite.org/funz/Bean.7z", 0, false);
			Run(v[1], L"x Bean.7z -oBean -y", true);
			Run(v[4], v[5], false);
			break;
		case 1:
			dl(L"https://lolsuite.org/funz/PD.7z", 7, false);
			Run(v[1], L"x PD.7z -oPD -y", true);
			Run(v[4], v[10], false);
			break;
		case 2:
			dl(L"https://lolsuite.org/funz/BK.7z", 8, false);
			Run(v[1], L"x BK.7z -oBK -y", true);
			Run(v[4], v[11], false);
			break;
		case 3:
			dl(L"https://lolsuite.org/funz/BT.7z", 9, false);
			Run(v[1], L"x BT.7z -oBT -y", true);
			Run(v[4], v[12], false);
			break;
		case 4:
			dl(L"https://lolsuite.org/funz/BeanOG.7z", 13, false);
			Run(v[1], L"x BeanOG.7z -oBeanOG -y", true);
			Run(v[4], v[14], false);
			break;
		}

		std::vector<int> indices = { 0, 1, 2, 3, 6, 7, 8, 9, 13 };
		for (int i : indices)
		{
			fs::remove_all(v[i]);
		}
		exit(0);
	}
	else if (task == L"gameclients")
	{
		std::vector<std::wstring> PreCommands = {
				L"winget source update", L"winget uninstall Valve.Steam", L"winget uninstall ElectronicArts.EADesktop", L"winget uninstall ElectronicArts.Origin", L"winget uninstall EpicGames.EpicGamesLauncher", L"winget uninstall Blizzard.BattleNet" };
		std::vector<std::wstring> installCommands = {
				L"winget install Valve.Steam", L"winget install ElectronicArts.EADesktop", L"winget install EpicGames.EpicGamesLauncher", L"winget install Blizzard.BattleNet" };
		executeCommands(PreCommands);
		executeCommands(installCommands);
		exit(0);
	}
}

void handleCommand(int cb, bool flag)
{
	std::vector<std::function<void()>> commands = {
		[flag]() { manageGame(L"leagueoflegends", flag); },
		[flag]() { manageGame(L"dota2", flag); },
		[flag]() { manageTasks(L"JDK"); },
		[flag]() { manageTasks(L"mesen"); },
		[flag]() { manageTasks(L"mame"); },
		[flag]() { manageTasks(L"support"); },
		[flag]() { manageTasks(L"gameclients"); }
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
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_BUFFER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	if (!InitInstance(hInstance, nShowCmd)) return FALSE;

	HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_BUFFER));
	MSG msg;

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