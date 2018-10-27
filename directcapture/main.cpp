#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstddef>
#include <cstdint>

#include <cxxopts.hpp>

#include <mlib/unicode/unicodecvt.hpp>

namespace
{

template<class T>
std::ostream &serialize(std::ostream &o, const T &t)
{
	return o.write((const char*)&t, sizeof(t));
}

std::ostream &serialize(std::ostream &o, const std::string &s)
{
	return o.write(s.c_str(), s.size() + 1);
}

std::ostream &serialize(std::ostream &o, const std::wstring &s)
{
	return o.write((const char*)s.c_str(), (s.size() + 1) * sizeof(wchar_t));
}

enum class MoveRegister : uint8_t { rax = 0xB8, rcx = 0xB9, rdx = 0xBA };
void emitMoveRegister(std::ostream &code, MoveRegister r, uintptr_t val) //emitted bytes: 10
{
	serialize<uint8_t>(code, 0x48); //mov rcx, ...
	serialize<uint8_t>(code, static_cast<uint8_t>(r));
	serialize<uintptr_t>(code, val);
}

enum class MoveRegister2 : uint8_t { rcx = 0xC1, rdx = 0xC2 };
void emitMoveEax(std::ostream &code, MoveRegister2 dst) //emitted bytes: 3
{
	serialize<uint8_t>(code, 0x48); //mov ..., rax
	serialize<uint8_t>(code, 0x89);
	serialize<uint8_t>(code, static_cast<uint8_t>(dst));
}

void emitAlignRsp(std::ostream &code)
{
	serialize<uint8_t>(code, 0x48); //and rsp, 0xF0 (align rsp to 16)
	serialize<uint8_t>(code, 0x83);
	serialize<uint8_t>(code, 0xE4);
	serialize<uint8_t>(code, 0xF0);
}

void emitSubRsp(std::ostream &code, uint8_t val) //emitted bytes: 4
{
	serialize<uint8_t>(code, 0x48); //sub rsp, ...
	serialize<uint8_t>(code, 0x83);
	serialize<uint8_t>(code, 0xEC);
	serialize<uint8_t>(code, val);
}

void emitAddRsp(std::ostream &code, uint8_t val)
{
	serialize<uint8_t>(code, 0x48); //add rsp, ...
	serialize<uint8_t>(code, 0x83);
	serialize<uint8_t>(code, 0xC4);
	serialize<uint8_t>(code, val);
}

void emitCallRax(std::ostream &code) //emitted bytes: 10
{
	emitSubRsp(code, 32);
	serialize<uint8_t>(code, 0xFF); //call rax
	serialize<uint8_t>(code, 0xD0);
	emitAddRsp(code, 32);
}

void emitSetEnvironment(std::ostream &code, uintptr_t nameptr, uintptr_t contentptr, uintptr_t funcptr)
{
	emitMoveRegister(code, MoveRegister::rcx, nameptr);
	emitMoveRegister(code, MoveRegister::rdx, contentptr);
	emitMoveRegister(code, MoveRegister::rax, funcptr);
	emitCallRax(code);
}

void emitReturn(std::ostream &code)
{
	serialize(code, 0xC3); //ret
}

void emitTestRax(std::ostream &code)
{
	serialize<uint8_t>(code, 0x48);
	serialize<uint8_t>(code, 0x85);
	serialize<uint8_t>(code, 0xC0);
}

void emitJz(std::ostream &code, uint8_t off)
{
	serialize<uint8_t>(code, 0x74);
	serialize<uint8_t>(code, off);
}

std::pair<std::string, uintptr_t> emitSetupCode(uintptr_t base, const std::wstring &dllpath, const std::wstring &recpath, unsigned int snddelay)
{
	const auto hKernel32 = GetModuleHandleA("kernel32.dll");
	const auto setenvptr = (uintptr_t)GetProcAddress(hKernel32, "SetEnvironmentVariableW");
	const auto loadlibptr = (uintptr_t)GetProcAddress(hKernel32, "LoadLibraryW");
	const auto lerrorptr = (uintptr_t)GetProcAddress(hKernel32, "GetLastError");

	std::stringstream code;
	
	const auto dllpathoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, dllpath);

	const auto recenvnameoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, std::wstring(L"dircap_recpath"));

	const auto recenvcontoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, recpath);

	const auto delayenvnameoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, std::wstring(L"dircap_delay"));

	const auto delayenvcontentoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, std::to_wstring(snddelay));

	const auto codeoff = static_cast<uintptr_t>(code.tellp());
	emitAlignRsp(code);

	emitSetEnvironment(code, base + recenvnameoff, base + recenvcontoff, setenvptr);
	emitSetEnvironment(code, base + delayenvnameoff, base + delayenvcontentoff, setenvptr);

	emitMoveRegister(code, MoveRegister::rcx, base + dllpathoff);
	emitMoveRegister(code, MoveRegister::rax, loadlibptr); //LoadLibraryW
	emitCallRax(code);

	emitMoveRegister(code, MoveRegister::rax, lerrorptr); //GetLastError
	emitCallRax(code);

	emitAddRsp(code, 8);
	emitReturn(code);

	return { code.str(), codeoff };
}

std::pair<std::string, uintptr_t> emitUnloadCode(uintptr_t base, const std::wstring &dllpath)
{
	const auto hKernel32 = GetModuleHandleA("kernel32.dll");
	const auto gethmod = (uintptr_t)GetProcAddress(hKernel32, "GetModuleHandleW");
	const auto getproc = (uintptr_t)GetProcAddress(hKernel32, "GetProcAddress");
	const auto lerrorptr = (uintptr_t)GetProcAddress(hKernel32, "GetLastError");

	std::stringstream code;

	const auto dllpathoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, dllpath);

	const auto unloadnameoff = static_cast<uintptr_t>(code.tellp());
	serialize(code, std::string("unloadDircap"));

	const auto codeoff = static_cast<uintptr_t>(code.tellp());
	emitAlignRsp(code);

	emitMoveRegister(code, MoveRegister::rcx, base + dllpathoff);
	emitMoveRegister(code, MoveRegister::rax, gethmod); //GetModuleHandleW
	emitCallRax(code);

	emitTestRax(code);
	emitJz(code, 33);

	emitMoveEax(code, MoveRegister2::rcx); //return value -> rcx
	emitMoveRegister(code, MoveRegister::rdx, base + unloadnameoff);
	emitMoveRegister(code, MoveRegister::rax, getproc); //GetProcAddress
	emitCallRax(code);

	emitTestRax(code);
	emitJz(code, 10);

	emitCallRax(code); //call unload function

	emitMoveRegister(code, MoveRegister::rax, lerrorptr); //GetLastError
	emitCallRax(code);

	emitAddRsp(code, 8);
	emitReturn(code);

	return { code.str(), codeoff };
}

void inject(HANDLE hProcess, uintptr_t base, const std::pair<std::string, uintptr_t> &exec)
{
	WriteProcessMemory(hProcess, (void*)base, exec.first.data(), exec.first.size(), nullptr);
	std::clog << "Code placed at 0x" << (void*)base << ".\n";

	DWORD threadid;
	auto hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)(base + exec.second), nullptr, 0, &threadid);
	std::clog << "Thread with ID " << threadid << " created.\n";
	WaitForSingleObject(hThread, INFINITE);

	DWORD c;
	GetExitCodeThread(hThread, &c);
	std::clog << "Thread finished with code " << c << (c != 0 ? " [ERROR].\n" : ".\n");
	CloseHandle(hThread);

    if(c == 126)
        std::cout << "The DLL was not found.\n";
}

std::wstring replaceExepath(const std::string &path)
{
    wchar_t buf[MAX_PATH];
    buf[GetModuleFileNameW(GetModuleHandle(0), buf, MAX_PATH)] = L'\0';

    auto rend = std::make_reverse_iterator(buf);
    if(auto ptr = std::find(std::make_reverse_iterator(buf + MAX_PATH), rend, L'\\'); ptr != rend) //strip exe file name
        *ptr = L'\0';

    auto wpath = mlib::unicode::toNative(path);

    static const std::wstring search = L"<exepath>";
    if(auto pos = wpath.find(search); pos != wpath.npos)
        wpath = wpath.substr(0, pos) + buf + wpath.substr(pos + search.size());

    return wpath;
}

int main2(int argc, char *argv[])
{
	cxxopts::Options opts("dircapinject", "Records DirectSound output from another application (x64).");

	opts.add_options()("p,pid", "Process ID of target application.", cxxopts::value<DWORD>(), "number");
	opts.add_options()("w,window", "Window title of target application. Overwrites -pid.", cxxopts::value<std::string>(), "text");
	opts.add_options()("o,output", "Output wave file.", cxxopts::value<std::string>()->default_value(".\\rec.wav"), "path");
	opts.add_options()("d,delay", "Optional time in milliseconds to wait after each write to sound buffer.",
				        cxxopts::value<unsigned int>()->default_value("0"), "number");
	opts.add_options()("q,load-only", "Inject DLL into application and quit immediately.");
	opts.add_options()("u,unload-only", "Unload DLL from application and quit immediately.");
	opts.add_options()("l,lib", "Specifies path to the DLL.", cxxopts::value<std::string>()->default_value("<exepath>\\dircapdll.dll"), "path");
    opts.add_options()("m,mem", "Sets working set size of specified process in MiB", cxxopts::value<std::string>(), "min,max");
    opts.add_options()("y,memonly", "Quit after settig working set size");

	if(argc <= 1)
	{
		std::cout << opts.help();
		return 0;
	}

	auto optres = opts.parse(argc, argv);

	DWORD pid;
	unsigned int soundDelay = 0;
	std::wstring outpath;
	bool InjectOnly = optres.count("load-only") > 0;
	bool UnloadOnly = optres.count("unload-only") > 0;

	if(optres.count("window") > 0)
	{
		const auto hWnd = FindWindowA(nullptr, optres["window"].as<std::string>().c_str());
		if(!hWnd)
		{
			std::cerr << "Window not found\n";
			return 42;
		}
		GetWindowThreadProcessId(hWnd, &pid);
	}
	else
	{
		if(optres.count("pid") <= 0)
			throw std::invalid_argument("Process ID required.");

		pid = optres["pid"].as<DWORD>();
	}

	if(!UnloadOnly)
	{
		outpath = mlib::unicode::toNative(optres["output"].as<std::string>());
		soundDelay = optres["delay"].as<unsigned int>();
	}

	wchar_t fulloutpath[MAX_PATH];
	if(!_wfullpath(fulloutpath, outpath.c_str(), MAX_PATH))
	{
		std::cerr << "Path to output file is malformed.\n";
		return 2;
	}

	const auto fulldllpath = replaceExepath(optres["lib"].as<std::string>());

	auto hProcess = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid);
	if(!hProcess)
	{
		std::cerr << "Opening process failed with error " << GetLastError() << '\n';
		return 3;
	}

	BOOL isWow64;
	IsWow64Process(hProcess, &isWow64);
	if(isWow64)
	{
		std::cerr << "The process is not a 64-bit process.\n";
		return 4;
	}

    if(optres.count("mem") > 0)
    {
        auto memspec = optres["mem"].as<std::string>();
        auto split = memspec.find_first_of(",");
        if(split == memspec.npos)
            throw std::invalid_argument("mem option expects parameter in format \"min,max\"");

        auto minstr = std::stoul(memspec.substr(0, split));
        auto maxstr = std::stoul(memspec.substr(split + 1));
        auto min = static_cast<SIZE_T>(minstr);
        auto max = static_cast<SIZE_T>(maxstr);

        if(SetProcessWorkingSetSize(hProcess, min * 1024 * 1024, max * 1024 * 1024) == 0)
            std::clog << "Error changing working set size: " << GetLastError() << "\n";
        else
            std::clog << "Working set size changed to " << min << "MiB/" << max << "MiB\n";

        if(optres.count("memonly") > 0)
            return 0;
    }

	auto *remotebuf = VirtualAllocEx(hProcess, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
	auto remoteptr = (uintptr_t)remotebuf;

	if(!UnloadOnly)
	{
		const auto execdata = emitSetupCode(remoteptr, fulldllpath, fulloutpath, soundDelay);
		inject(hProcess, remoteptr, execdata);
	}

	if(!InjectOnly && !UnloadOnly)
	{
		std::clog << "Waiting for keypress to release capture...\n";
		std::cin.get();
	}

	if(!InjectOnly)
	{
		const auto execdata = emitUnloadCode(remoteptr, fulldllpath);
		inject(hProcess, remoteptr, execdata);
	}

	VirtualFreeEx(hProcess, remotebuf, 0x1000, MEM_RELEASE);
	CloseHandle(hProcess);

	return 0;
}

}

int main(int argc, char *argv[])
{
	try
	{
		return main2(argc, argv);
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what();
		return 1;
	}
}
