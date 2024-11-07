#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <functional>
#include <shellapi.h>
#include <shobjidl.h>
#include <string>
#include <TlHelp32.h>
#include <vector>
#include "resource.h"
#include <filesystem>

namespace fs = std::filesystem;

auto currentPath = fs::current_path();
constexpr auto MAX_LOADSTRING = 100;
std::vector<std::wstring> v(58);
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
const wchar_t* box[8] = {
	L"League of Legends", L"DOTA2", L"Minecraft Java", L"Mesen", L"GoldenEye CE",
	L"MAME, HBMAME & FBNeo", L"VCRedist AIO", L"Internet Cafe Client Installer"
};
HRESULT BrowseForFolder(HWND hwndOwner, LPWSTR pszFolderPath, DWORD cchFolderPath)
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
	return hr;
}

std::wstring PathJoin(const int index, const std::wstring& add)
{
	return (fs::path(v[index]) / add).wstring();
}
void PathAppend(const int index, const std::wstring& add)
{
	v[index] = PathJoin(index, add);
}
void PathCombine(const int destIndex, const int srcIndex, const std::wstring& add)
{
	v[destIndex] = PathJoin(srcIndex, add);
}
void SHELLEXECUTE(const std::wstring& lpFile, const std::wstring& lpParameters, bool wait)
{
	SHELLEXECUTEINFO shellExecuteInfo = {};
	shellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	shellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shellExecuteInfo.nShow = SW_SHOWNORMAL;
	shellExecuteInfo.lpFile = lpFile.c_str();
	shellExecuteInfo.lpParameters = lpParameters.c_str();
	if (!ShellExecuteEx(&shellExecuteInfo))
	{
		return;
	}
	if (wait && shellExecuteInfo.hProcess != nullptr)
	{
		WaitForSingleObject(shellExecuteInfo.hProcess, INFINITE);
		CloseHandle(shellExecuteInfo.hProcess);
	}
}
void pkill(const std::wstring& process_name)
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
void DeleteZoneIdentifier(const std::wstring& file)
{
	std::wstring zoneIdentifierPath = L"\\?\"" + file + L":Zone.Identifier";
	!DeleteFile(zoneIdentifierPath.c_str());
}
void Download(const std::wstring& url, int j, bool serv)
{
	std::wstring fullUrl = serv ? L"http://lolsuite.org/lolsuite/" + url : url;
	HRESULT hr = URLDownloadToFile(nullptr, fullUrl.c_str(), v[j].c_str(), 0, nullptr);
	if (SUCCEEDED(hr))
	{
		DeleteZoneIdentifier(v[j]);
	}
}
bool IsProcess64Bit()
{
	USHORT processMachine = 0;
	USHORT nativeMachine = 0;
	auto fnIsWow64Process2 = reinterpret_cast<decltype(&IsWow64Process2)>(
		GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process2"));
	if (!fnIsWow64Process2) return false;
	if (!fnIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)) return false;
	return processMachine != IMAGE_FILE_MACHINE_UNKNOWN;
}

void manageGame(const std::wstring& game, bool restore)
{
	if (game == L"leagueoflegends") {
		const wchar_t* processes[] = {
			L"LeagueClient.exe", L"LeagueClientUx.exe", L"LeagueClientUxRender.exe",
			L"League of Legends.exe", L"Riot Client.exe", L"RiotClientServices.exe"
		};
		for (const auto& process : processes) {
			pkill(process);
		}
		PathCombine(56, 0, L"Riot Client\\RiotClientElectron");
		PathCombine(57, 56, L"d3dcompiler_47.dll");
		const wchar_t* baseFiles[] = {
			L"concrt140.dll", L"d3dcompiler_47.dll", L"msvcp140.dll", L"msvcp140_1.dll",
			L"msvcp140_2.dll", L"msvcp140_codecvt_ids.dll", L"ucrtbase.dll",
			L"vcruntime140.dll", L"vcruntime140_1.dll"
		};
		PathAppend(0, L"League of Legends");
		for (int i = 0; i < 9; ++i) {
			PathCombine(42 + i, 0, baseFiles[i]);
		}
		PathCombine(51, 0, L"Game");
		DeleteZoneIdentifier(PathJoin(51, L"League of Legends.exe"));
		const wchar_t* gameFiles[] = {
			L"D3DCompiler_47.dll", L"D3dx9_43.dll", L"xinput1_3.dll", L"tbb.dll"
		};
		for (int i = 0; i < 3; ++i) {
			PathCombine(52 + i, 51, gameFiles[i]);
		}
		PathCombine(55, 51, gameFiles[3]);
		auto downloadFiles = [&](const wchar_t* prefix, bool deletetbb = false) {
			for (int i = 0; i < 9; ++i) {
				Download(std::wstring(prefix) + baseFiles[i], 42 + i, true);
			}
			for (int i = 0; i < 3; ++i) {
				Download(std::wstring(prefix) + gameFiles[i], 52 + i, true);
			}
			if (deletetbb) {
				DeleteFile(v[55].c_str());
			}
			};
		if (restore) {
			downloadFiles(L"r/lol/", true);
		}
		else {
			downloadFiles(L"", false);
			if (IsProcess64Bit()) {
				Download(L"6/D3DCompiler_47.dll", 52, true);
				Download(L"6/tbb12.dll", 54, true);
				Download(L"6/D3DCompiler_47.dll", 55, true);
			}
			else {
				Download(L"D3DCompiler_47.dll", 52, true);
				Download(L"tbb12.dll", 54, true);
				Download(L"D3DCompiler_47.dll", 55, true);
			}
			Download(L"D3DCompiler_47.dll", 57, true);
		}
		SHELLEXECUTE(PathJoin(56, L"Riot Client.exe"), L"", false);
		exit(0);
	}
	else if (game == L"dota2") {
		pkill(L"dota2.exe");
		PathAppend(0, L"game\\bin\\win64");
		PathCombine(1, 0, L"embree3.dll");
		DeleteZoneIdentifier(PathJoin(0, L"dota2.exe"));
		Download(restore ? L"r/dota2/embree3.dll" : L"6/embree4.dll", 1, true);
		SHELLEXECUTE(L"steam://rungameid/570//-high/", L"", false);
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
	wcex.lpszMenuName = nullptr; // Assuming no menu is used
	wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON));
	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;
	HWND hWnd = CreateWindowExW(
		0, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 130,
		nullptr, nullptr, hInstance, nullptr
	);
	if (!hWnd) return FALSE;
	std::vector<std::tuple<DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HMENU>> controls = {
		{WS_EX_TOOLWINDOW, L"BUTTON", L"Patch", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 20, 100, 50, reinterpret_cast<HMENU>(1)},
		{0, L"BUTTON", L"Restore", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 20, 100, 50, reinterpret_cast<HMENU>(2)},
		{0, WC_COMBOBOX, L"", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, 260, 20, 200, 200, nullptr}
	};
	HWND hComboBox = nullptr;
	for (const auto& [dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hMenu] : controls) {
		HWND hControl = CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWnd, hMenu, hInstance, nullptr);
		if (!hControl) return FALSE;
		if (lpClassName == WC_COMBOBOX) {
			hComboBox = hControl;
		}
	}
	for (const auto& str : box) {
		SendMessage(hComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(str));
	}
	SendMessageW(hComboBox, CB_SETCURSEL, 0, 0);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

void manageTasks(const std::wstring& task, bool restore = false)
{
	auto clearAndAppend = [](int index, const std::wstring& path)
		{
			v[index].clear();
			PathAppend(index, fs::current_path());
			PathAppend(index, path);
		};
	auto executeCommands = [](const std::vector<std::wstring>& commands)
		{
			for (const auto& cmd : commands)
			{
				_wsystem(cmd.c_str());
			}
		};
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
		PathAppend(0, fs::current_path());
		PathAppend(0, L"jdk-23_windows-x64_bin.exe");
		Download(L"https://download.oracle.com/java/23/latest/jdk-23_windows-x64_bin.exe", 0, false);
		SHELLEXECUTE(v[0], L"", true);
		fs::remove_all(v[0]);
		MessageBoxEx(
			nullptr,
			L"Minecraft Launcher > Minecraft: Java Edition > Installations > Latest Release > Edit > More Options > Java Executable > Browse > <drive>:\\Program Files\\Java\\jdk-23\\bin\\javaw.exe",
			L"LoLSuite",
			MB_OK,
			0 // Language identifier (0 for default)
		);
	}
	else if (task == L"mame")
	{
		for (auto& path : v) path.clear();

		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"7z.exe"},
			{1, L"HBMAME.7z"},
			{2, L"HBMAME"},
			{3, L"MAME.exe"},
			{4, L"MAME"},
			{5, L"FBNeo.zip" },
			{6, L"FBNeo"}
		};
		for (const auto& [index, subPath] : paths)
		{
			PathAppend(index, currentPath);
			PathAppend(index, subPath);
		}
		std::vector<std::tuple<std::wstring, int, bool>> downloads = {
			{L"7z.exe", 0, true},
			{L"https://hbmame.1emulation.com/hbmameui20.7z", 1, false},
			{L"https://github.com/mamedev/mame/releases/download/mame0271/mame0271b_64bit.exe", 3, false},
		{IsProcess64Bit()
			? L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x64.zip"
			: L"https://github.com/finalburnneo/FBNeo/releases/download/latest/Windows.x32.zip", 5, false}
		};
		for (const auto& [url, index, flag] : downloads)
		{
			Download(url, index, flag);
		}
		for (int i : {2, 4, 6})
		{
			CreateDirectory(v[i].c_str(), nullptr);
		}
		for (const auto& cmd : { L"x HBMAME.7z -oHBMAME -y", L"x MAME.exe -oMAME -y", L"x FBNeo.zip -oFBNeo -y" })
		{
			SHELLEXECUTE(v[0], cmd, true);
		}
		for (int i : {0, 1, 3, 5})
		{
			fs::remove_all(v[i]);
		}
	}
	else if (task == L"mesen")
	{
		for (int i : {0, 1, 2}) v[i].clear();
		std::vector<std::pair<int, std::wstring>> paths = {
			{0, L"7z.exe"},
			{1, L"Mesen.zip"},
			{2, L"mesen.exe"},
		};
		for (const auto& [index, subPath] : paths)
		{
			PathAppend(index, currentPath);
			PathAppend(index, subPath);
		}
		_wsystem(L"winget install Microsoft.DotNet.DesktopRuntime.8 --accept-package-agreements");
		Download(L"7z.exe", 0, true);
		Download(
			L"https://nightly.link/SourMesen/Mesen2/workflows/build/master/Mesen%20%28Windows%20-%20net8.0%29.zip",
			1, false);
		SHELLEXECUTE(v[0], L"x Mesen.zip -y", true);
		for (int i : {0, 1})fs::remove_all(v[i]);
		SHELLEXECUTE(v[2], L"--nes.disableGameDatabase=true", false);
		exit(0);
	}
	else if (task == L"support")
	{
		std::vector<std::wstring> installCommands = {
			L"winget install Microsoft.Terminal --accept-package-agreements",
			L"winget install Microsoft.PowerShell --accept-package-agreements",
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
			L"winget install Microsoft.EdgeWebView2Runtime --accept-package-agreement",
			L"winget install Microsoft.VCRedist.2005.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x86 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x86 --accept-package-agreements",
			L"winget install Microsoft.VSTOR --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2005.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2008.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2010.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2012.x64 --accept-package-agreements",
			L"winget install Microsoft.VCRedist.2013.x64 --accept-package-agreements",
			L"winget install 9PCSD6N03BKV --accept-package-agreements" };

		std::vector<std::wstring> PreCommands = {
				L"winget uninstall Microsoft.Terminal",
				L"winget uninstall Microsoft.PowerShell",
				L"winget source update",
				L"powercfg /hibernate off",
				L"winget uninstall 9PB0TRCNRHFX",
				L"winget uninstall 9NQPSL29BFFF",
				L"winget uninstall Microsoft.EdgeWebView2Runtime",
				L"winget uninstall 9N95Q1ZZPMH4",
				L"winget uninstall 9NCTDW2W1BH8",
				L"winget uninstall 9MVZQVXJBQ9V",
				L"winget uninstall 9PMMSR1CGPWG",
				L"winget uninstall 9N4D0MSMP0PT",
				L"winget uninstall 9PG2DK419DRG",
				L"winget uninstall 9PB0TRCNRHFX",
				L"winget uninstall 9N5TDP8VCMHS",
				L"winget uninstall 9PCSD6N03BKV",
				L"winget uninstall Microsoft.VCRedist.2005.x86",
				L"winget uninstall Microsoft.VCRedist.2008.x86",
				L"winget uninstall Microsoft.VCRedist.2010.x86",
				L"winget uninstall Microsoft.VCRedist.2012.x86",
				L"winget uninstall Microsoft.VCRedist.2013.x86",
				L"winget uninstall Microsoft.VSTOR --accept-package-agreements",
				L"winget uninstall Microsoft.VCRedist.2005.x64",
				L"winget uninstall Microsoft.VCRedist.2008.x64",
				L"winget uninstall Microsoft.VCRedist.2010.x64",
				L"winget uninstall Microsoft.VCRedist.2012.x64",
				L"winget uninstall Microsoft.VCRedist.2013.x64"
		};
		executeCommands(PreCommands);
		executeCommands(installCommands);
		clearAndAppend(0, L"dxwebsetup.exe");
		Download(L"https://download.microsoft.com/download/1/7/1/1718CCC4-6315-4D8E-9543-8E28A4E18C4C/dxwebsetup.exe", 0, false);
		SHELLEXECUTE(v[0], L"/Q", true);
		fs::remove_all(v[0]);
	}
	else if (task == L"xenia")
	{
		for (auto& path : v) path.clear();
		std::vector<std::pair<int, std::wstring>> paths = {
			{5, L"g64.7z"},
			{0, L"7z.exe"},
			{1, L"xenia_master.zip"},
			{3, L"LICENSE"},
			{2, L"xenia.pdb"},
			{4, L"xenia.exe"},
			{6, L"defaultCE.xex"}
		};
		for (const auto& [index, subPath] : paths)
		{
			PathAppend(index, currentPath);
			PathAppend(index, subPath);
		}
		Download(L"http://92.35.115.29/server/g64.7z", 5, false);
		Download(L"7z.exe", 0, true);
		Download(L"https://github.com/xenia-project/release-builds-windows/releases/latest/download/xenia_master.zip", 1, false);
		std::vector<std::wstring> commands = { L"x g64.7z -y", L"x xenia_master.zip -y" };
		std::vector<int> indices = { 0, 1, 2, 3, 5 };
		for (const auto& cmd : commands)
		{
			SHELLEXECUTE(v[0], cmd, true);
		}
		for (int i : indices)
		{
			fs::remove_all(v[i]);
		}
		SHELLEXECUTE(v[4], v[6], false);
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
	}
}

void handleCommand(int cb, bool flag)
{
	std::vector<std::function<void()>> commands = {
		[flag]() { manageGame(L"leagueoflegends", flag); },
		[flag]() { manageGame(L"dota2", flag); },
		[flag]() { manageTasks(L"JDK"); },
		[flag]() { manageTasks(L"mesen"); },
		[flag]() { manageTasks(L"xenia"); },
		[flag]() { manageTasks(L"mame"); },
		[flag]() { manageTasks(L"support", flag); },
		[flag]() { manageTasks(L"gameclients"); }
	};
	if (cb >= 0 && cb < commands.size())
	{
		commands[cb]();
	}
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Default is League of Legends
	static int cb = 0;
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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_BUFFER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	if (!InitInstance(hInstance, nCmdShow)) return FALSE;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"Failed to initialize COM library", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	WCHAR szFolderPath[MAX_PATH] = {};
	hr = BrowseForFolder(nullptr, szFolderPath, ARRAYSIZE(szFolderPath));
	if (SUCCEEDED(hr))
	{
		v[0] = szFolderPath;
	}
	CoUninitialize();

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
