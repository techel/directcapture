#include <cstdint>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <map>
#include <vector>
#include <functional>

#include <Windows.h>
#include <dsound.h>

#include "wavefile.hpp"

namespace
{

class AtomicGuard
{
public:
	AtomicGuard(std::atomic<int> &i) : Ato(i) { ++Ato; }
	~AtomicGuard() { --Ato; }

private:
	std::atomic<int> &Ato;
};

class Hook
{
public:
	template<class Hooked, class Replacement>
	Hook(Hooked h, Replacement r) { hook(h, r); }
	Hook() = default;
	~Hook() { disable(); }

	template<class Hooked, class Replacement>
	void hook(Hooked h, Replacement r)
	{
		HookedFunc = (void*)h;
		ReplFunc = (void*)r;
		ReadProcessMemory(GetCurrentProcess(), HookedFunc, &OrigInstr, sizeof(OrigInstr), nullptr);

		NewInstr[0] = 0x48;
		NewInstr[1] = 0xB8;
		*(uintptr_t*)&NewInstr[2] = (uintptr_t)r;
		NewInstr[10] = 0xFF;
		NewInstr[11] = 0xE0;
	}
	void enable()
	{
		WriteProcessMemory(GetCurrentProcess(), HookedFunc, &NewInstr, sizeof(NewInstr), nullptr);
	}
	void disable()
	{
		WriteProcessMemory(GetCurrentProcess(), HookedFunc, &OrigInstr, sizeof(OrigInstr), nullptr);
	}

private:
	void *HookedFunc, *ReplFunc;
	uint8_t OrigInstr[12], NewInstr[12];
};

template<class Method>
class COMHook
{
public:
	COMHook() = default;

	void method(Method *m)
	{
		NewMethod = m;
	}

	template<class COMInterface>
	void hook(COMInterface *i, size_t funcidx)
	{
		assert(NewMethod);

		Method **vtable = *(Method***)i;

		DWORD oldprot;
		VirtualProtect(vtable, 0x1000, PAGE_EXECUTE_READWRITE, &oldprot);

		Method **entry = (Method**)&vtable[funcidx];
		
		if(*entry != NewMethod)
		{
			InstalledHooks[i] = { *entry, entry };
			*entry = NewMethod;
		}
	}

	void unhook()
	{
		for(const auto &i : InstalledHooks)
		{
			DWORD oldprot;
			VirtualProtect(i.second.VTableEntry, 0x1000, PAGE_EXECUTE_READWRITE, &oldprot);
			if(*i.second.VTableEntry == NewMethod)
				*i.second.VTableEntry = i.second.Old;
		}
	}

	template<class... Args>
	auto callOld(IUnknown *obj, Args &&...args)
	{
		auto it = InstalledHooks.find(obj);
		assert(it != InstalledHooks.end());
		return it->second.Old(std::forward<Args>(args)...);
	}

	~COMHook() { unhook(); }

private:
	Method *NewMethod = nullptr;
	struct HookInfo
	{
		Method *Old, **VTableEntry;
	};
	std::map<IUnknown*, HookInfo> InstalledHooks;
};

//
// global information
//

struct DllData
{
	std::mutex Lock;

	Hook DirectSoundCreateHook;
	COMHook<HRESULT WINAPI(LPUNKNOWN, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN)> CreateSoundBufferHook;
	COMHook<HRESULT WINAPI(LPUNKNOWN, LPWAVEFORMATEX)> SetFormatHook;
	COMHook<HRESULT WINAPI(LPUNKNOWN, LPVOID, DWORD, LPVOID, DWORD)> UnlockHook;

	std::fstream OutputFile;
	WaveFile Output;
	unsigned int SoundDelay = 0;

	void closeOutput()
	{
		Output.close();
		OutputFile.close();
	}

	void openOutput(const WAVEFORMATEX &w)
	{
		closeOutput();

		const auto snddelay = _wgetenv(L"dircap_delay");
		if(snddelay)
			SoundDelay = _wtoi(snddelay);

		const auto outpath = _wgetenv(L"dircap_recpath");
		if(!outpath)
			return;

		OutputFile.open(outpath, std::ios::out | std::ios::binary);
		if(!OutputFile)
			return;

		Output.open(OutputFile, w.nSamplesPerSec, w.wBitsPerSample, w.nChannels);
	}

	std::atomic<int> HookUsage;

	~DllData()
	{
		{
			std::lock_guard<std::mutex> g(Lock);
			DirectSoundCreateHook.disable();
			CreateSoundBufferHook.unhook();
			SetFormatHook.unhook();
			UnlockHook.unhook();
		}

		while(HookUsage.load() > 0)
			Sleep(50);
	}
};

DllData Data;

//
// IDirectSoundBuffer hook
//

HRESULT WINAPI setFormatHook(LPUNKNOWN obj, LPWAVEFORMATEX f)
{
	AtomicGuard a(Data.HookUsage);
	std::lock_guard<std::mutex> g(Data.Lock);

	const HRESULT res = Data.SetFormatHook.callOld(obj, obj, f);
	if(res == S_OK && f)
		Data.openOutput(*f);
	return res;
}

HRESULT WINAPI unlockHook(LPUNKNOWN obj, LPVOID ptr1, DWORD len1, LPVOID ptr2, DWORD len2)
{
	AtomicGuard a(Data.HookUsage);
	std::lock_guard<std::mutex> g(Data.Lock);

	const auto res = Data.UnlockHook.callOld(obj, obj, ptr1, len1, ptr2, len2);
	if(res == S_OK)
	{
		if(Data.OutputFile.is_open())
		{
			Data.Output.write((const char*)ptr1, static_cast<size_t>(len1));

			if(ptr2)
				Data.Output.write((const char*)ptr2, static_cast<size_t>(len2));

			if(Data.SoundDelay != 0)
				Sleep(Data.SoundDelay);
		}
	}
	return res;
}

//
// IDirectSound hook
//

HRESULT WINAPI createSoundBufferHookFunc(LPUNKNOWN obj, LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter)
{
	AtomicGuard a(Data.HookUsage);
	std::lock_guard<std::mutex> g(Data.Lock);

	const auto res = Data.CreateSoundBufferHook.callOld(obj, obj, pcDSBufferDesc, ppDSBuffer, pUnkOuter);
	if(res == S_OK)
	{
		Data.SetFormatHook.hook(*ppDSBuffer, 14);
		Data.UnlockHook.hook(*ppDSBuffer, 19);
	}
	return res;
}

//
// DirectSoundCreate hook
//

HRESULT WINAPI directSoundCreateHookFunc(LPCGUID guid, LPDIRECTSOUND *ds, LPUNKNOWN o)
{
	AtomicGuard a(Data.HookUsage);
	std::lock_guard<std::mutex> g(Data.Lock);

	Data.DirectSoundCreateHook.disable();
	const auto res = DirectSoundCreate(guid, ds, o);
	Data.DirectSoundCreateHook.enable();

	if(res == S_OK)
		Data.CreateSoundBufferHook.hook(*ds, 3);

	return res;
}

void setup()
{
	Data.CreateSoundBufferHook.method(&createSoundBufferHookFunc);
	Data.SetFormatHook.method(&setFormatHook);
	Data.UnlockHook.method(&unlockHook);

	HMODULE hDs = GetModuleHandleA("dsound.dll");
	Data.DirectSoundCreateHook.hook(GetProcAddress(hDs, "DirectSoundCreate"), &directSoundCreateHookFunc);
	
	std::lock_guard<std::mutex> g(Data.Lock);
	Data.DirectSoundCreateHook.enable();
}

std::atomic<HMODULE> hDllModule;

}

__declspec(dllexport) void unloadDircap()
{
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__ )

	FreeLibraryAndExitThread(hDllModule, 0);
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD r, LPVOID)
{
	if(r == DLL_PROCESS_ATTACH)
	{
		hDllModule = hMod;
		setup();
	}
	return TRUE;
}