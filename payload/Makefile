# Axel '0vercl0k' - 3 May 2019
TARGET = payload.dll

SOURCES = \
    src\ReflectiveLoader.c \
    src\ReflectiveDll.cc \

ASSEMBLIES = src\trampoline.asm

CFLAGS = /O1 /nologo /W3 /D_AMD64_ /DWIN_X64 /DREFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN
LDFLAGS = /DLL /nologo /debug:full /PDBALTPATH:%%_PDB%%

all: $(TARGET) jsify clean

$(TARGET): $(SOURCES) trampoline.obj
    if not exist .\bin mkdir bin
    type injected-script.js > src\injected-script.h
    cl $(CFLAGS) /Febin\$@ $** /link $(LDFLAGS)

trampoline.obj: $(ASSEMBLIES)
    ml64 /c $**

jsify: $(TARGET)
    python jsify_payload.py bin\$**
    move payload.js ..\

clean:
    del *.obj
    del src\injected-script.h
    if exist .\bin del bin\*.exp bin\*.ilk bin\*.lib
