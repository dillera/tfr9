// info.c -- what NitrOS-9 and the TFR/911H know about themselves.
//
//   info [-p]        -p: plain text, no bold headings
//
// System      the OS and its settings, from the Init module; the time
// Memory      the system block map (F$GBlkMp)
// Processes   every process descriptor (F$GPrDsc)
// Devices     the device table, read from system memory with F$CpyMem,
//             and each descriptor's file manager, driver and port
// Disks       LSN 0 and the free-space bitmap of each RBF disk
// Interrupts  the IRQ polling table
// Modules     the module directory (F$GModDr)
// Hardware    the TFR/911H firmware's own report, from its info port:
//             write $A5 to $FF43, wait for $FF43 to read 1, then read
//             "label TAB value LF" lines from $FF44 until a 0.
//
// Build:  cmoc --os9 --function-stack=0 --add-os9-stack-space=1024 -o info info.c

#include <cmoc.h>

typedef unsigned char byte;
typedef unsigned int word;

#define CR 13

// ── OS-9 system calls ──────────────────────────────────────────────

byte os9_link(const char *name, byte **hdr) {
    byte *h = 0;
    byte err = 0;
    asm {
        ldx     :name
        clra                    ; any type
        pshs    u,y             ; F$Link returns the entry point in Y
        os9     $00             ; F$Link
        tfr     u,x
        puls    u,y
        bcs     @err
        stx     :h
        bra     @done
@err
        stb     :err
@done
    }
    *hdr = h;
    return err;
}

void os9_unlink(byte *hdr) {
    asm {
        pshs    u
        ldu     :hdr
        os9     $02             ; F$UnLink
        puls    u
    }
}

byte os9_gprdsc(byte id, byte *buf) {
    byte err = 0;
    asm {
        lda     :id
        ldx     :buf
        os9     $18             ; F$GPrDsc
        bcc     @ok
        stb     :err
@ok
    }
    return err;
}

// Copy n bytes at addr in the address space a DAT image describes.
void os9_cpymem(byte *dat, word addr, word n, byte *dst) {
    asm {
        pshs    u,y
        ldd     :dat
        ldx     :addr
        ldy     :n
        ldu     :dst
        os9     $1B             ; F$CpyMem
        puls    u,y
    }
}

word os9_gblkmp(byte *buf, word *blocks) {
    word size = 0, n = 0;
    asm {
        ldx     :buf
        pshs    y
        os9     $19             ; F$GBlkMp
        bcs     @err
        std     :size
        sty     :n
@err
        puls    y
    }
    *blocks = n;
    return size;
}

// Copy the module directory; returns its end in buf, and its address in
// system memory.
byte *os9_gmoddr(byte *buf, word *sys) {
    byte *end = 0;
    word s = 0;
    asm {
        ldx     :buf
        pshs    u,y
        os9     $1A             ; F$GModDr
        tfr     y,x
        tfr     u,d
        puls    u,y
        bcs     @err
        stx     :end
        std     :s
@err
    }
    *sys = s;
    return end;
}

void os9_time(byte *t) {
    asm {
        ldx     :t
        os9     $15             ; F$Time
    }
}

byte os9_id(void) {
    byte id = 0;
    asm {
        pshs    y
        os9     $0C             ; F$ID
        puls    y
        sta     :id
    }
    return id;
}

byte os9_open(const char *name, byte mode) {
    char n[40];
    byte path = 255, i = 0;
    while (*name && i < 38) n[i++] = *name++;
    n[i] = CR;
    asm {
        lda     :mode
        leax    :n
        os9     $84             ; I$Open
        bcs     @err
        sta     :path
@err
    }
    return path;
}

word os9_read(byte path, byte *buf, word n) {
    word got = 0;
    asm {
        lda     :path
        ldx     :buf
        pshs    y
        ldy     :n
        os9     $89             ; I$Read
        bcc     @ok
        ldy     #0
@ok
        sty     :got
        puls    y
    }
    return got;
}

void os9_close(byte path) {
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

void os9_writeln(const char *p, word n) {
    asm {
        lda     #1
        ldx     :p
        pshs    y
        ldy     :n
        os9     $8C             ; I$WritLn
        puls    y
    }
}

void os9_sleep(word ticks) {
    asm {
        ldx     :ticks
        os9     $0A             ; F$Sleep
    }
}

// ── Output ─────────────────────────────────────────────────────────

char line[200];
byte ll, plain;

void put(const char *s) {
    while (*s && ll < 190) line[ll++] = *s++;
}

void putch(char c) {
    if (ll < 190) line[ll++] = c;
}

void putul(unsigned long n) {
    char d[12];
    byte i = 0;
    do {
        d[i++] = (char)('0' + (byte)(n % 10));
        n /= 10;
    } while (n);
    while (i) putch(d[--i]);
}

void putu(word n) { putul(n); }

void puthex(word n, byte digits) {
    const char *hex = "0123456789ABCDEF";
    putch('$');
    while (digits--) putch(hex[(n >> (digits * 4)) & 15]);
}

void put2(byte n) {         // two digits
    putch((char)('0' + n / 10));
    putch((char)('0' + n % 10));
}

// A size: 512 bytes, 37K, 25.6M
void putsize(unsigned long b) {
    if (b < 1024) {
        putul(b);
        put(" bytes");
    } else if (b < 10UL * 1024 * 1024) {
        putul((b + 512) / 1024);
        putch('K');
    } else {
        putul(b / (1024UL * 1024));
        putch('.');
        putch((char)('0' + (byte)(b % (1024UL * 1024) * 10 / (1024UL * 1024))));
        putch('M');
    }
}

void tab(byte col) {
    while (ll < col) line[ll++] = ' ';
}

void endline(void) {
    line[ll++] = CR;
    os9_writeln(line, ll);
    ll = 0;
}

void heading(const char *t) {
    endline();
    if (!plain) put("\033[1m");
    put(t);
    if (!plain) put("\033[0m");
    endline();
}

void field(const char *label) {
    put("  ");
    put(label);
    tab(14);
}

// Names in modules end with the high bit set.
void putfcs(const byte *s, byte max) {
    while (max--) {
        putch((char)(*s & 0x7F));
        if (*s++ & 0x80) break;
    }
}

word get16(const byte *p) { return ((word)p[0] << 8) | p[1]; }

// ── System memory ──────────────────────────────────────────────────

byte sysproc[512];          // the system process descriptor
byte *sysdat;               // the system's DAT image, in it

void sys_read(word addr, word n, byte *dst) {
    os9_cpymem(sysdat, addr, n, dst);
}

word sys_word(word addr) {
    byte b[2];
    sys_read(addr, 2, b);
    return get16(b);
}

// The name of the module at addr in system memory, high bit on its end.
void sys_modname(word mod, byte *name) {
    byte hdr[6];
    sys_read(mod, 6, hdr);
    sys_read(mod + get16(hdr + 4), 32, name);
}

// ── Sections ───────────────────────────────────────────────────────

byte *init;                 // the Init module, linked

void put_init_str(word off) {   // a name in Init
    if (off) putfcs(init + off, 32);
}

void put_init_cstr(word off) {  // a NUL-terminated string in Init
    const byte *s;
    if (!off) return;
    for (s = init + off; *s; s++) putch((char)(*s & 0x7F));
}

void system_section(void) {
    byte t[6];
    heading("System");
    field("OS");
    put_init_cstr(get16(init + 29));
    endline();
    field("Machine");
    put_init_cstr(get16(init + 31));
    put(", as the TFR/911H pretends to be");
    endline();
    field("Kernel");
    put("Level ");
    putu(init[23]);
    put(", version ");
    putu(init[24]);
    putch('.');
    putu(init[25]);
    putch('.');
    putu(init[26]);
    put(init[27] & 2 ? ", built for the 6309" : ", built for the 6809");
    put(init[27] & 1 ? ", module CRCs checked" : "");
    endline();
    field("Startup");
    put("runs ");
    put_init_str(get16(init + 14));
    put(", disk ");
    put_init_str(get16(init + 16));
    put(", console ");
    put_init_str(get16(init + 18));
    endline();
    field("Tables");
    putu(init[13]);
    put(" device slots, ");
    putu(init[12]);
    put(" IRQ polling slots");
    endline();
    os9_time(t);
    field("Date");
    putu(1900 + t[0]);
    putch('/');
    put2(t[1]);
    putch('/');
    put2(t[2]);
    putch(' ');
    put2(t[3]);
    putch(':');
    put2(t[4]);
    putch(':');
    put2(t[5]);
    endline();
}

void memory_section(void) {
    static byte map[1024];
    word bsize, n, i, freeb = 0, modb = 0, used = 0, notram = 0, run = 0, best = 0;
    heading("Memory");
    bsize = os9_gblkmp(map, &n);
    for (i = 0; i < n && i < sizeof map; i++) {
        byte b = map[i];
        if (b & 0x80) notram++;
        else if (!b) {
            freeb++;
            if (++run > best) best = run;
            continue;
        } else if (b & 2) modb++;
        else used++;
        run = 0;
    }
    field("RAM");
    putsize((unsigned long)(n - notram) * bsize);
    put(" in ");
    putu(n - notram);
    put(" blocks of ");
    putsize(bsize);
    endline();
    field("Free");
    putsize((unsigned long)freeb * bsize);
    put(" (largest piece ");
    putsize((unsigned long)best * bsize);
    putch(')');
    endline();
    field("In use");
    putsize((unsigned long)modb * bsize);
    put(" modules, ");
    putsize((unsigned long)used * bsize);
    put(" system and processes");
    endline();
}

const char *state_name(byte s, byte me) {
    if (me) return "running";
    if (s & 0x01) return "dead";
    if (s & 0x08) return "suspended";
    if (s & 0x40) return "sleeping";
    return "waiting";
}

void putmem(unsigned long b) {  // a short size, for tables
    if (b < 1024) putul(b);
    else putsize(b);
}

void processes_section(void) {
    static byte pd[512];
    byte hdr[6], name[32];
    word id, count = 0;
    byte me = os9_id();
    heading("Processes");
    put("   ID  Parent  User  Prio  Memory  State      Program");
    endline();
    for (id = 1; id < 256; id++) {
        if (os9_gprdsc((byte)id, pd)) continue;
        count++;
        tab(2);
        putch(id == me ? '*' : ' ');
        putu(pd[0]);
        tab(9);
        putu(pd[1]);
        tab(17);
        putu(get16(pd + 8));
        tab(23);
        putu(pd[10]);
        tab(29);
        putmem((unsigned long)pd[7] * 256);
        tab(37);
        put(id == 1 ? "" : state_name(pd[12], id == me));
        tab(48);
        if (get16(pd + 0x11) && !(pd[12] & 1)) {
            os9_cpymem(pd + 0x40, get16(pd + 0x11), 6, hdr);
            os9_cpymem(pd + 0x40, get16(pd + 0x11) + get16(hdr + 4), 32, name);
            putfcs(name, 32);
        } else {
            put(id == 1 ? "(the system)" : "-");
        }
        endline();
    }
    put("  ");
    putu(count);
    put(" processes; * is this one");
    endline();
}

// A descriptor's first bytes, if it is a real module (stale table entries
// can point anywhere).
byte read_desc(word desc, byte *hdr) {
    if (!desc) return 0;
    sys_read(desc, 0x14, hdr);
    return hdr[0] == 0x87 && hdr[1] == 0xCD;
}

// The device table, copied from system memory.
#define MAXDEV 32
byte devtbl[MAXDEV * 13];
byte ndev;

void device_name(byte i, byte *name) {   // a device's name, from its descriptor
    sys_modname(get16(devtbl + i * 13 + 4), name);
}

// The module directory, copied with F$GModDr.  Each entry's module is
// read through its own DAT image, which the copy includes.
byte moddir[2048];
byte *modend;
word modsys;

void mod_read(byte *e, word off, word n, byte *dst) {
    os9_cpymem(moddir + (get16(e) - modsys), get16(e + 4) + off, n, dst);
}

// The device table entry for a descriptor called name, or 255.
byte attached(const byte *name) {
    byte i, k, hdr[0x14], n2[32];
    for (i = 0; i < ndev; i++) {
        if (!read_desc(get16(devtbl + i * 13 + 4), hdr)) continue;
        device_name(i, n2);
        for (k = 0; k < 32; k++) {
            if ((n2[k] | 0x20) != (name[k] | 0x20)) break;
            if (name[k] & 0x80) return i;
        }
    }
    return 255;
}

void devices_section(void) {
    byte *e, hdr[0x14], name[32], s[32], t;
    const char *types[] = {"SCF", "RBF", "Pipe", "SBF", "NFM", "CDFM", "RFM"};
    heading("Devices");
    put("  Device    Type  Manager  Driver    Port   Drive  Users");
    endline();
    for (e = moddir; e < modend; e += 8) {
        if (!get16(e)) continue;
        mod_read(e, 0, sizeof hdr, hdr);
        if ((hdr[6] & 0xF0) != 0xF0) continue;       // device descriptors only
        mod_read(e, get16(hdr + 4), 32, name);
        put("  /");
        putfcs(name, 32);
        tab(14);
        put(hdr[0x12] < 7 ? types[hdr[0x12]] : "?");
        tab(20);
        mod_read(e, get16(hdr + 9), 32, s);          // M$FMgr
        putfcs(s, 32);
        tab(29);
        mod_read(e, get16(hdr + 11), 32, s);         // M$PDev
        putfcs(s, 32);
        tab(39);
        puthex(get16(hdr + 15), 4);                  // M$Port (low 16 bits)
        tab(46);
        if (hdr[0x12] == 1) putu(hdr[0x13]);         // IT.DRV
        tab(53);
        t = attached(name);
        if (t == 255) put("-");
        else putu(devtbl[t * 13 + 8]);               // V$USRS
        endline();
    }
    put("  Users: paths open on an attached device (- is not attached)");
    endline();
}

// The disks: each RBF drive once (several descriptors can name one).
void disks_section(void) {
    static byte lsn0[256], buf[256];
    byte *e, *e2, hdr[0x14], h2[0x14], name[32], n2[32], b, path, dup;
    word mapbytes, k, w, bit, spc;
    unsigned long total, clusters, freec, seen;
    char dev[40];
    heading("Disks");
    for (e = moddir; e < modend; e += 8) {
        if (!get16(e)) continue;
        mod_read(e, 0, sizeof hdr, hdr);
        if ((hdr[6] & 0xF0) != 0xF0 || hdr[0x12] != 1) continue;
        dup = 0;                                     // a drive already shown?
        for (e2 = moddir; e2 < e && !dup; e2 += 8) {
            if (!get16(e2)) continue;
            mod_read(e2, 0, sizeof h2, h2);
            if ((h2[6] & 0xF0) == 0xF0 && h2[0x12] == 1 &&
                get16(h2 + 15) == get16(hdr + 15) && h2[0x13] == hdr[0x13]) dup = 1;
        }
        if (dup) continue;
        mod_read(e, get16(hdr + 4), 32, name);
        dev[0] = '/';
        for (k = 0; k < 30; k++) {
            dev[k + 1] = (char)(name[k] & 0x7F);
            if (name[k] & 0x80) break;
        }
        dev[k + 2] = 0;
        put("  ");
        put(dev);
        for (e2 = e + 8; e2 < modend; e2 += 8) {    // the drive's other names
            if (!get16(e2)) continue;
            mod_read(e2, 0, sizeof h2, h2);
            if ((h2[6] & 0xF0) == 0xF0 && h2[0x12] == 1 &&
                get16(h2 + 15) == get16(hdr + 15) && h2[0x13] == hdr[0x13]) {
                mod_read(e2, get16(h2 + 4), 32, n2);
                put(" = /");
                putfcs(n2, 32);
            }
        }
        endline();
        dev[k + 2] = '@';                            // the whole disk, raw
        dev[k + 3] = 0;
        field("");
        path = os9_open(dev, 1);
        if (path == 255 || os9_read(path, lsn0, 256) != 256) {
            put("can't read it (no disk?)");
            endline();
            if (path != 255) os9_close(path);
            continue;
        }
        total = ((unsigned long)lsn0[0] << 16) | ((unsigned long)lsn0[1] << 8) | lsn0[2];
        spc = get16(lsn0 + 6);                       // sectors per cluster
        if (!spc) spc = 1;
        mapbytes = get16(lsn0 + 4);
        clusters = total / spc;
        freec = seen = 0;
        while (mapbytes) {                           // count the free clusters
            k = mapbytes < sizeof buf ? mapbytes : sizeof buf;
            if (os9_read(path, buf, k) != k) break;
            for (w = 0; w < k; w++) {
                b = buf[w];
                for (bit = 0; bit < 8; bit++) {
                    if (seen++ >= clusters) break;
                    if (!(b & 0x80)) freec++;
                    b <<= 1;
                }
            }
            mapbytes -= k;
        }
        os9_close(path);
        put("\"");
        putfcs(lsn0 + 0x1F, 32);                     // DD.NAM
        put("\", made ");
        putu(1900 + lsn0[0x1A]);
        putch('/');
        put2(lsn0[0x1B]);
        putch('/');
        put2(lsn0[0x1C]);
        endline();
        field("");
        putsize(total * 256);
        put(" (");
        putul(total);
        put(" sectors), ");
        putsize(freec * spc * 256);
        put(" free (");
        putu(clusters ? (word)(freec * 100 / clusters) : 0);
        put("%)");
        endline();
    }
}

void interrupts_section(void) {
    byte i, j, n, q[11], q2[0x14], name[32], used = 0;
    word tbl, stat;
    heading("Interrupts");
    put("  Status reg  Mask  Prio  Handler");
    endline();
    n = init[12];
    tbl = sys_word(0x82);                            // D.PolTbl
    for (i = 0; i < n; i++) {
        sys_read(tbl + i * 11, 11, q);
        if (!get16(q)) continue;                     // Q$POLL: empty slot
        used++;
        tab(2);
        puthex(get16(q), 4);
        tab(14);
        puthex(q[3], 2);                             // Q$MASK
        tab(20);
        putu(q[8]);                                  // Q$PRTY
        tab(26);
        stat = get16(q + 6);                         // Q$STAT: whose static storage
        for (j = 0; j < ndev; j++) {
            if (read_desc(get16(devtbl + j * 13 + 4), q2) && get16(devtbl + j * 13 + 2) == stat) {
                sys_modname(get16(devtbl + j * 13), name);   // V$DRIV
                putfcs(name, 32);
                put(" (/");
                device_name(j, name);
                putfcs(name, 32);
                putch(')');
                break;
            }
        }
        if (j == ndev) {
            put("service routine at ");
            puthex(get16(q + 4), 4);
        }
        endline();
    }
    put("  ");
    putu(used);
    put(" of ");
    putu(n);
    put(" polling slots used; the clock runs from the GIME's VBORD IRQ");
    endline();
}

void modules_section(void) {
    byte *e, hdr[6], name[32];
    word count = 0;
    unsigned long bytes = 0;
    heading("Modules");
    put("  ");
    for (e = moddir; e < modend; e += 8) {
        if (!get16(e)) continue;                     // MD$MPDAT: an empty slot
        mod_read(e, 0, 6, hdr);
        mod_read(e, get16(hdr + 4), 32, name);
        if (ll > 68) {
            endline();
            put("  ");
        }
        putfcs(name, 12);
        putch(' ');
        count++;
        bytes += get16(hdr + 2);
    }
    endline();
    put("  ");
    putu(count);
    put(" modules in memory, ");
    putsize(bytes);
    endline();
}

void hardware_section(void) {
    byte *port = (byte *)0xFF43;
    byte *data = (byte *)0xFF44;
    byte c, tries;
    heading("Hardware (from the TFR/911H firmware)");
    *port = 0xA5;
    for (tries = 0; tries < 30 && *port != 1; tries++) os9_sleep(2);
    if (*port != 1) {
        put("  No report: this firmware has no info port.");
        endline();
        return;
    }
    put("  ");
    while ((c = *data) != 0) {
        if (c == '\t') tab(14);
        else if (c == '\n') {
            endline();
            put("  ");
        } else putch((char)c);
    }
    ll = 0;
}

int main(int argc, char **argv) {
    byte *hdr;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'p') plain = 1;
    else if (argc > 1) {
        put("Usage: info [-p]");
        endline();
        return 1;
    }
    if (os9_link("Init", &init)) {
        put("info: can't link to Init");
        endline();
        return 1;
    }
    os9_gprdsc(1, sysproc);
    sysdat = sysproc + 0x40;                         // P$DATImg
    ndev = init[13] < MAXDEV ? init[13] : MAXDEV;
    sys_read(sys_word(0x80), ndev * 13, devtbl);     // D.DevTbl
    modend = os9_gmoddr(moddir, &modsys);
    if (!plain) put("\033[1m");
    put_init_cstr(get16(init + 29));
    put(" on the TFR/911H");
    if (!plain) put("\033[0m");
    endline();
    system_section();
    memory_section();
    processes_section();
    devices_section();
    disks_section();
    interrupts_section();
    modules_section();
    hardware_section();
    hdr = init;
    os9_unlink(hdr);
    return 0;
}
