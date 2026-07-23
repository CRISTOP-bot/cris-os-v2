#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

extern "C" {
    fn inb(port: u16) -> u8;
    fn outb(port: u16, value: u8);
}

#[no_mangle]
pub unsafe extern "C" fn memset(s: *mut u8, c: i32, n: usize) -> *mut u8 {
    if s.is_null() || n == 0 { return s; }
    let val = c as u8;
    for i in 0..n { *s.add(i) = val; }
    s
}

#[no_mangle]
pub unsafe extern "C" fn memcpy(dest: *mut u8, src: *const u8, n: usize) -> *mut u8 {
    if dest.is_null() || src.is_null() || n == 0 { return dest; }
    for i in 0..n { *dest.add(i) = *src.add(i); }
    dest
}

#[no_mangle]
pub unsafe extern "C" fn memcmp(a: *const u8, b: *const u8, n: usize) -> i32 {
    if n == 0 { return 0; }
    for i in 0..n {
        let ca = *a.add(i);
        let cb = *b.add(i);
        if ca != cb { return (ca as i32) - (cb as i32); }
    }
    0
}

#[no_mangle]
pub unsafe extern "C" fn strlen(s: *const u8) -> usize {
    if s.is_null() { return 0; }
    let mut i: usize = 0;
    while *s.add(i) != 0 { i += 1; }
    i
}

const SERIAL_PORT: u16 = 0x3F8;

#[no_mangle]
pub extern "C" fn rust_serial_init() {
    unsafe {
        outb(SERIAL_PORT + 1, 0x00);
        outb(SERIAL_PORT + 3, 0x80);
        outb(SERIAL_PORT + 0, 0x03);
        outb(SERIAL_PORT + 1, 0x00);
        outb(SERIAL_PORT + 3, 0x03);
        outb(SERIAL_PORT + 2, 0xC7);
        outb(SERIAL_PORT + 4, 0x0B);
    }
}

#[no_mangle]
pub extern "C" fn rust_serial_putchar(c: u8) {
    unsafe {
        while (inb(SERIAL_PORT + 5) & 0x20) == 0 {}
        outb(SERIAL_PORT, c);
    }
}

#[no_mangle]
pub extern "C" fn rust_serial_print(s: *const u8) {
    if s.is_null() { return; }
    let mut i = 0usize;
    unsafe {
        loop {
            let c = *s.add(i);
            if c == 0 { break; }
            rust_serial_putchar(c);
            i += 1;
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_serial_getchar() -> u8 {
    unsafe {
        while (inb(SERIAL_PORT + 5) & 0x01) == 0 {}
        inb(SERIAL_PORT)
    }
}

#[no_mangle]
pub extern "C" fn rust_serial_available() -> i32 {
    unsafe { if (inb(SERIAL_PORT + 5) & 0x01) != 0 { 1 } else { 0 } }
}

#[no_mangle]
pub extern "C" fn rust_strlen(s: *const u8) -> usize {
    unsafe { strlen(s) }
}

#[no_mangle]
pub extern "C" fn rust_strcmp(a: *const u8, b: *const u8) -> i32 {
    if a.is_null() || b.is_null() {
        if a == b { return 0; }
        return if a.is_null() { -1 } else { 1 };
    }
    unsafe {
        let mut i: usize = 0;
        loop {
            let ca = *a.add(i);
            let cb = *b.add(i);
            if ca != cb { return (ca as i32) - (cb as i32); }
            if ca == 0 { return 0; }
            i += 1;
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_strncmp(a: *const u8, b: *const u8, n: usize) -> i32 {
    if n == 0 { return 0; }
    unsafe {
        for i in 0..n {
            let ca = *a.add(i);
            let cb = *b.add(i);
            if ca != cb { return (ca as i32) - (cb as i32); }
            if ca == 0 { return 0; }
        }
    }
    0
}

#[no_mangle]
pub extern "C" fn rust_strcpy(dest: *mut u8, src: *const u8, maxlen: usize) -> *mut u8 {
    if maxlen == 0 || dest.is_null() || src.is_null() { return dest; }
    unsafe {
        let mut i: usize = 0;
        while i + 1 < maxlen {
            let c = *src.add(i);
            *dest.add(i) = c;
            if c == 0 { break; }
            i += 1;
        }
        *dest.add(i) = 0;
    }
    dest
}

#[no_mangle]
pub extern "C" fn rust_strcat(dest: *mut u8, src: *const u8, maxlen: usize) -> *mut u8 {
    if maxlen == 0 || dest.is_null() || src.is_null() { return dest; }
    unsafe {
        let mut i: usize = 0;
        while i < maxlen && *dest.add(i) != 0 { i += 1; }
        let mut j: usize = 0;
        while i + j < maxlen - 1 {
            let c = *src.add(j);
            *dest.add(i + j) = c;
            if c == 0 { break; }
            j += 1;
        }
        *dest.add(i + j) = 0;
    }
    dest
}

#[no_mangle]
pub extern "C" fn rust_strstr(haystack: *const u8, needle: *const u8) -> *const u8 {
    if haystack.is_null() || needle.is_null() { return core::ptr::null(); }
    unsafe {
        let mut h: usize = 0;
        loop {
            let ch = *haystack.add(h);
            let mut n: usize = 0;
            let mut match_pos = h;
            loop {
                let cn = *needle.add(n);
                let ch2 = *haystack.add(match_pos);
                if cn == 0 { return haystack.add(h); }
                if ch2 == 0 || ch2 != cn { break; }
                n += 1; match_pos += 1;
            }
            if ch == 0 { return core::ptr::null(); }
            h += 1;
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_strchr(s: *const u8, c: u8) -> *const u8 {
    if s.is_null() { return core::ptr::null(); }
    unsafe {
        let mut i: usize = 0;
        loop {
            let sc = *s.add(i);
            if sc == c { return s.add(i); }
            if sc == 0 { return core::ptr::null(); }
            i += 1;
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_memset(s: *mut u8, c: i32, n: usize) -> *mut u8 {
    unsafe { memset(s, c, n) }
}

#[no_mangle]
pub extern "C" fn rust_memcpy(dest: *mut u8, src: *const u8, n: usize) -> *mut u8 {
    unsafe { memcpy(dest, src, n) }
}

#[no_mangle]
pub extern "C" fn rust_memcmp(a: *const u8, b: *const u8, n: usize) -> i32 {
    unsafe { memcmp(a, b, n) }
}

fn div_mod_u64(val: u64, d: u64) -> (u64, u64) {
    if d == 0 { return (0, 0); }
    let mut q: u64 = 0;
    let mut r: u64 = 0;
    for i in (0..64).rev() {
        r = r.wrapping_add(r);
        r |= (val >> i) & 1;
        if r >= d { r = r.wrapping_sub(d); q |= 1 << i; }
    }
    (q, r)
}

#[no_mangle]
pub extern "C" fn rust_itoa(value: i64, buf: *mut u8, maxlen: usize) {
    if maxlen == 0 || buf.is_null() { return; }
    unsafe {
        let mut pos: usize = 0;
        if value == 0 {
            if pos + 1 < maxlen { *buf.add(pos) = 48; pos += 1; }
        } else {
            let neg = value < 0;
            let uv = if neg { 0u64.wrapping_sub(value as u64) } else { value as u64 };
            let mut rev = [0u8; 32];
            let mut rp: usize = 0;
            let mut tmp = uv;
            while tmp > 0 && rp < 32 {
                let (_, rem) = div_mod_u64(tmp, 10);
                rev[rp] = 48 + rem as u8;
                tmp = div_mod_u64(tmp, 10).0;
                rp += 1;
            }
            if neg && pos + 1 < maxlen { *buf.add(pos) = 45; pos += 1; }
            while rp > 0 && pos + 1 < maxlen {
                rp -= 1; *buf.add(pos) = rev[rp]; pos += 1;
            }
        }
        *buf.add(pos) = 0;
    }
}

#[no_mangle]
pub extern "C" fn rust_utoa(value: u64, buf: *mut u8, maxlen: usize) {
    if maxlen == 0 || buf.is_null() { return; }
    unsafe {
        let mut pos: usize = 0;
        if value == 0 {
            if pos + 1 < maxlen { *buf.add(pos) = 48; pos += 1; }
        } else {
            let mut rev = [0u8; 32];
            let mut rp: usize = 0;
            let mut tmp = value;
            while tmp > 0 && rp < 32 {
                let (_, rem) = div_mod_u64(tmp, 10);
                rev[rp] = 48 + rem as u8;
                tmp = div_mod_u64(tmp, 10).0;
                rp += 1;
            }
            while rp > 0 && pos + 1 < maxlen {
                rp -= 1; *buf.add(pos) = rev[rp]; pos += 1;
            }
        }
        *buf.add(pos) = 0;
    }
}

#[no_mangle]
pub extern "C" fn rust_xtoa(value: u64, buf: *mut u8, maxlen: usize) {
    if maxlen == 0 || buf.is_null() { return; }
    unsafe {
        let mut pos: usize = 0;
        if value == 0 {
            if pos + 1 < maxlen { *buf.add(pos) = 48; pos += 1; }
        } else {
            let mut rev = [0u8; 32];
            let mut rp: usize = 0;
            let mut tmp = value;
            while tmp > 0 && rp < 32 {
                rev[rp] = *b"0123456789ABCDEF\0".as_ptr().add((tmp & 0xF) as usize);
                tmp >>= 4;
                rp += 1;
            }
            while rp > 0 && pos + 1 < maxlen {
                rp -= 1; *buf.add(pos) = rev[rp]; pos += 1;
            }
        }
        *buf.add(pos) = 0;
    }
}

#[no_mangle]
pub extern "C" fn rust_atoi(s: *const u8) -> i32 {
    if s.is_null() { return 0; }
    unsafe {
        let mut value: i32 = 0;
        let mut sign: i32 = 1;
        let mut i: usize = 0;
        if *s.add(0) == 45 { sign = -1; i = 1; }
        loop {
            let c = *s.add(i);
            if c < 48 || c > 57 { break; }
            if value > 214748364 {
                return if sign > 0 { 2147483647 } else { -2147483647 - 1 };
            }
            value = value.wrapping_mul(10).wrapping_add((c - 48) as i32);
            i += 1;
        }
        value.wrapping_mul(sign)
    }
}

fn i32_div(a: i32, b: i32) -> i32 {
    if b == 0 { return 0; }
    if a == -2147483648 && b == -1 { return -2147483648; }
    let neg = (a < 0) != (b < 0);
    let ua = if a < 0 { 0u32.wrapping_sub(a as u32) } else { a as u32 };
    let ub = if b < 0 { 0u32.wrapping_sub(b as u32) } else { b as u32 };
    let q = div_mod_u64(ua as u64, ub as u64).0 as u32;
    if neg { 0u32.wrapping_sub(q) as i32 } else { q as i32 }
}

#[no_mangle]
pub extern "C" fn rust_add(a: i32, b: i32) -> i32 { a.wrapping_add(b) }
#[no_mangle]
pub extern "C" fn rust_sub(a: i32, b: i32) -> i32 { a.wrapping_sub(b) }
#[no_mangle]
pub extern "C" fn rust_mul(a: i32, b: i32) -> i32 { a.wrapping_mul(b) }
#[no_mangle]
pub extern "C" fn rust_div(a: i32, b: i32) -> i32 { i32_div(a, b) }

#[no_mangle]
pub extern "C" fn rust_fibonacci(n: u32) -> u64 {
    if n == 0 { return 0; }
    if n == 1 { return 1; }
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 2..=n { let next = a.wrapping_add(b); a = b; b = next; }
    b
}

#[no_mangle]
pub extern "C" fn rust_factorial(n: u32) -> u64 {
    let mut r = 1u64;
    for i in 2..=n { r = r.wrapping_mul(i as u64); }
    r
}

fn u32_mod(a: u32, b: u32) -> u32 {
    if b == 0 { return 0; }
    div_mod_u64(a as u64, b as u64).1 as u32
}

#[no_mangle]
pub extern "C" fn rust_is_prime(n: u32) -> i32 {
    if n <= 1 { return 0; }
    if n == 2 { return 1; }
    if u32_mod(n, 2) == 0 { return 0; }
    let mut i: u32 = 3;
    loop {
        let sq = i.wrapping_mul(i);
        if sq > n || sq < i { break; }
        if u32_mod(n, i) == 0 { return 0; }
        i = i.wrapping_add(2);
    }
    1
}

#[no_mangle]
pub extern "C" fn rust_gcd(mut a: u32, mut b: u32) -> u32 {
    while b != 0 { let t = b; b = u32_mod(a, b); a = t; }
    a
}

#[no_mangle]
pub extern "C" fn rust_rotate_left(val: u32, shift: u32) -> u32 {
    let s = shift & 31;
    if s == 0 { val } else { (val << s) | (val >> (32 - s)) }
}

#[no_mangle]
pub extern "C" fn rust_rotate_right(val: u32, shift: u32) -> u32 {
    let s = shift & 31;
    if s == 0 { val } else { (val >> s) | (val << (32 - s)) }
}

#[no_mangle]
pub extern "C" fn rust_popcount(val: u64) -> u32 {
    let mut count = 0u32;
    let mut v = val;
    while v != 0 { count += 1; v &= v.wrapping_sub(1); }
    count
}

#[no_mangle]
pub extern "C" fn rust_leading_zeros(val: u64) -> u32 {
    if val == 0 { return 64; }
    let mut n = 0u32;
    let mut v = val;
    if v & 0xFFFFFFFF00000000 == 0 { n += 32; v <<= 32; }
    if v & 0xFFFF000000000000 == 0 { n += 16; v <<= 16; }
    if v & 0xFF00000000000000 == 0 { n += 8; v <<= 8; }
    if v & 0xF000000000000000 == 0 { n += 4; v <<= 4; }
    if v & 0xC000000000000000 == 0 { n += 2; v <<= 2; }
    if v & 0x8000000000000000 == 0 { n += 1; }
    n
}

#[no_mangle]
pub extern "C" fn rust_trailing_zeros(val: u64) -> u32 {
    if val == 0 { return 64; }
    let mut n = 0u32;
    let mut v = val;
    if v & 0xFFFFFFFF == 0 { n += 32; v >>= 32; }
    if v & 0xFFFF == 0 { n += 16; v >>= 16; }
    if v & 0xFF == 0 { n += 8; v >>= 8; }
    if v & 0xF == 0 { n += 4; v >>= 4; }
    if v & 0x3 == 0 { n += 2; v >>= 2; }
    if v & 0x1 == 0 { n += 1; }
    n
}

#[no_mangle]
pub extern "C" fn rust_swap_bytes16(val: u16) -> u16 {
    ((val as u16) >> 8) | ((val as u16) << 8)
}

#[no_mangle]
pub extern "C" fn rust_swap_bytes32(val: u32) -> u32 {
    let v = val;
    ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8)
    | ((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24)
}

#[no_mangle]
pub extern "C" fn rust_swap_bytes64(val: u64) -> u64 {
    let v = val;
    ((v & 0x00000000000000FF) << 56) | ((v & 0x000000000000FF00) << 40)
    | ((v & 0x0000000000FF0000) << 24) | ((v & 0x00000000FF000000) << 8)
    | ((v & 0x000000FF00000000) >> 8) | ((v & 0x0000FF0000000000) >> 24)
    | ((v & 0x00FF000000000000) >> 40) | ((v & 0xFF00000000000000) >> 56)
}

#[repr(C)]
pub struct RustPoint { pub x: i64, pub y: i64 }

#[no_mangle]
pub extern "C" fn rust_distance_sq(p: RustPoint) -> u64 {
    let x = if p.x < 0 { 0u64.wrapping_sub(p.x as u64) } else { p.x as u64 };
    let y = if p.y < 0 { 0u64.wrapping_sub(p.y as u64) } else { p.y as u64 };
    x.wrapping_mul(x).wrapping_add(y.wrapping_mul(y))
}

#[no_mangle]
pub extern "C" fn rust_to_uppercase(s: *mut u8, len: usize) {
    if s.is_null() { return; }
    unsafe {
        for i in 0..len {
            let c = *s.add(i);
            if c >= 97 && c <= 122 { *s.add(i) = c - 32; }
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_to_lowercase(s: *mut u8, len: usize) {
    if s.is_null() { return; }
    unsafe {
        for i in 0..len {
            let c = *s.add(i);
            if c >= 65 && c <= 90 { *s.add(i) = c + 32; }
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_info() {
    let msg: *const u8 = b"\r\n[Rust] NucleOS Rust kernel module loaded\r\n[Rust] Version 1.0.0-x86_64\r\n[Rust] Built with rustc 1.97.1\r\n\0" as *const u8;
    rust_serial_print(msg);
}
