# CVE-2019-9810 Exploit for Firefox on Windows

CVE-2019-9810 is a vulnerability that has been found and exploited at [Pwn2Own 2019](https://www.thezdi.com/blog/2019/4/18/the-story-of-two-winning-pwn2own-jit-vulnerabilities-in-mozilla-firefox) by [Richard Zhu and Amat Cama](https://twitter.com/Fluoroacetate). It affects Mozilla's JavaScript engine, Spidermonkey and was used to achieve renderer compromise.

The issue has been fixed in [mfsa2019-09](https://www.mozilla.org/en-US/security/advisories/mfsa2019-09/) about two months ago.

<p align='center'>
    <img src='pics/party.gif' height='600px' />
</p>

## Overview of the issue

In a nutshell the bug allows for a check bounds to be found redundant and optimized away allowing code to access out of bounds memory. The issue itself lies in the Alias Analysis pass of Ion's pipeline. The below picture highlights the consequence of the bug in the GVN optimization (bounds check being optimized away) pass and serves as a good summary if this is what you are looking for:

![summary](https://doar-e.github.io/images/root_causing_cve-2019-9810/summary.png)

If you want to know more about it though, I would recommend to have a look at [A journey into IonMonkey: root-causing CVE-2019-9810](https://doar-e.github.io/blog/2019/06/17/a-journey-into-ionmonkey-root-causing-cve-2019-9810/).

## Organization

The repository contains the [exploit](https://github.com/0vercl0k/CVE-2019-9810/blob/master/party.js) code as well as a bunch of [tools](https://github.com/0vercl0k/blazefox/blob/master/exploits/moarutils.js) that I had previously developed for my [blazefox](https://github.com/0vercl0k/blazefox) exploits. I have just brushed them up and made them work with [BigInt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt). As a result, the exploit assumes that the support for [BigInt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt) is turned on in Firefox which you can do by toggling `javascript.options.bigint` in `about:config`.

![bigint](pics/bigint.png)

The exploit has been tested against Windows RS5 64-bit and it targets a custom build of Firefox so don't be surprised if a bit of work is required to make it work elsewhere :). However, if you just feel like running the exploit without compiling anything, I prepared a packaged browser that I uploaded in [release/firefox-68.0a1.en-US.win64.7z](https://github.com/0vercl0k/CVE-2019-9810/releases/download/1/firefox-68.0a1.en-US.win64.7z). It also includes the `js.exe` shell as well as private symbol information for `js.exe`, `firefox.exe` and `xul.dll`.

The exploitation process works very similarly than in my previous [kaizen.js](https://github.com/0vercl0k/blazefox/blob/master/exploits/kaizen.js) exploit as mentioned above. It dispatches execution on the [ReflectiveLoader](https://github.com/stephenfewer/ReflectiveDLLInjection/blob/master/dll/src/ReflectiveLoader.c#L51) of a [reflective dll](https://github.com/stephenfewer/ReflectiveDLLInjection) that implements the [payload](https://github.com/0vercl0k/CVE-2019-9810/blob/master/payload/src/ReflectiveDll.cc#L17):

1. If the payload detects that it is invoked by `js.exe`, it simply spams `stdout` with `PWN`, spawns a calculator and exits.
2. If it is run from the browser, it starts by injecting itself into other `Firefox.exe` processes. To achieve that, the Javascript exploit [passes a pointer to the reflective dll copy](https://github.com/0vercl0k/CVE-2019-9810/blob/master/toolbox.js#L496), and the reflective dll maps it in the other processes. Once this is accomplished, it creates a remote thread on the reflective loader and takes a nap. The reason for that is that the exploit is pretty dirty in its current state and doesn't implement process continuation. Altough, at this point I don't think it would be a lot of work. Maybe I'll get around of do it :-).
3. When the payload gets executed from other `Firefox.exe`'s, it inline-hooks the [xul!nsJSUtils::ExecutionContext::Compile](http://ff-woboq.s3-website-us-west-2.amazonaws.com/Firefox/dom/base/nsJSUtils.cpp.html#_ZN9nsJSUtils16ExecutionContext7CompileERN2JS14CompileOptionsERNS1_10SourceTextIDsEE) function. This function gets executed when scripts need to be evaluated by the JavaScript engine; so this sounded like a good enough candidate for what I wanted to do. The hooked version simply prepends an [arbitrary JavaScript payload](https://github.com/0vercl0k/CVE-2019-9810/blob/master/payload/injected-script.js) of our choice.
4. When the hook is placed, it simply returns. At this point, the other origins have had arbitrary JavaScript injected in them. The payload I use is simply to change the background image of those origins by the [Diary of a reverse-engineer](https://doar-e.github.io/) theme picture, as well as redirecting every links to the blog :).

In reality, there are a bunch of more subtle details that are not described by the above and so if you are interested you are invited to go find the truth and read the sources :).

## Building the payload

To build the payload, you just have to run `nmake` from a VS 2017 x64 prompt.

```text
CVE-2019-9810\payload>nmake

Microsoft (R) Program Maintenance Utility Version 14.16.27027.1
Copyright (C) Microsoft Corporation.  All rights reserved.

        ml64 /c src\trampoline.asm
Microsoft (R) Macro Assembler (x64) Version 14.16.27027.1
Copyright (C) Microsoft Corporation.  All rights reserved.

 Assembling: src\trampoline.asm
        if not exist .\bin mkdir bin
        type injected-script.js > src\injected-script.h
        cl /O1 /nologo /W3 /D_AMD64_ /DWIN_X64 /DREFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN /Febin\payload.dll src\ReflectiveLoader.c src\ReflectiveDll.cc trampoline.obj /link /DLL /nologo /debug:full /PDBALTPATH:%_PDB%
ReflectiveLoader.c
Generating Code...
Compiling...
ReflectiveDll.cc
Generating Code...
   Creating library bin\payload.lib and object bin\payload.exp
        python jsify_payload.py bin\payload.dll
        move payload.js ..
        1 file(s) moved.
        del *.obj
        del src\injected-script.h
        if exist .\bin del bin\*.exp bin\*.ilk bin\*.lib
```

This creates a `payload.dll` / `payload.pdb` file inside the `payload\bin` directory. As well as a JavaScript file called `payload.js` which embeds the dll inside an [Uint8Array](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array) with the offset to the loader.

## Building Firefox

I wrote this exploit against a local Windows build synchronized to the following revision id: [2abb636ad481768b7c88619080cf224b2c266b2d](https://hg.mozilla.org/mozilla-central/rev/2abb636ad481768b7c88619080cf224b2c266b2d) (if you don't feel like building it yourself, I've uploaded my build here: [release/firefox-68.0a1.en-US.win64.7z](https://github.com/0vercl0k/CVE-2019-9810/releases/download/1/firefox-68.0a1.en-US.win64.7z)):

```text
$ hg --debug id -i
2abb636ad481768b7c88619080cf224b2c266b2d
```

And I have used the following `mozconfig` file:

```text
. "$topsrcdir/browser/config/mozconfigs/win64/common-win64"

ac_add_options --disable-crashreporter
ac_add_options --enable-debug-symbols

. "$topsrcdir/build/mozconfig.clang-cl"
. "$topsrcdir/build/mozconfig.lld-link"

# Use the clang version in .mozbuild
CLANG_LIB_DIR="$(cd ~/.mozbuild/clang/lib/clang/*/lib/windows && pwd)"
export LIB=$LIB:$CLANG_LIB_DIR

ac_add_options --enable-js-shell
ac_add_options --enable-jitspew
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-ff64
```

## Discussion

Although it was fine for my purpose, I am unsure `xul!nsJSUtils::ExecutionContext::Compile` is the perfect function for inserting arbitrary scripts. I am sure spending more time understanding a bit how works the xul front-end one could come up with a better hooking point.

Another couple of avenues that I discovered after writing the exploit are discussed in this bugzilla entry: [982974](https://bugzilla.mozilla.org/show_bug.cgi?id=982974) (System principal for the JavaScript interpreter and `security.turn_off_all_security_so_that_viruses_can_take_over_this_computer`). It would be interesting to see how much of it is still relevant to Firefox today.

Maybe somebody already researched this subject and I completely missed it. In any case, feel free to ping me with any feedback!

Another interesting thing would be to explore if there any way to have a persistence mechanism with in the browser. I haven't researched this area at all but that would be pretty cool :).