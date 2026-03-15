// PolandOS — Powloka jadra (ring 0)
// Jadro Orzel — interaktywna powloka systemowa

#include "shell.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../lib/panic.h"
#include "../drivers/fb.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../drivers/pci.h"
#include "../drivers/nvme.h"
#include "../drivers/serial.h"
#include "../drivers/gpu.h"
#include "../arch/x86_64/mm/pmm.h"
#include "../acpi/acpi.h"
#include "../net/ethernet.h"
#include "../net/icmp.h"
#include "../net/dns.h"
#include "../services/service.h"
#include "../installer/installer.h"
#include "../fs/vfs.h"
#include "../gui/desktop.h"
#include "../../include/types.h"

// ─── Kolory ───────────────────────────────────────────────────────────────────
#define COLOR_YELLOW   0xFFFF00
#define COLOR_WHITE    0xFFFFFF
#define COLOR_GREEN    0x00CC00
#define COLOR_RED      0xFF3333
#define COLOR_CYAN     0x00CCCC
#define COLOR_GRAY     0xAAAAAA
#define COLOR_BLACK    0x000000

// ─── Historia polecen ─────────────────────────────────────────────────────────
#define HISTORY_SIZE   16
#define LINE_MAX       256
#define ARGS_MAX       16

static char history[HISTORY_SIZE][LINE_MAX];
static int  history_count = 0;
static int  history_head  = 0;   // index of next write slot (circular)

static void history_push(const char *line)
{
    if (!line || !line[0]) return;
    // Avoid duplicate consecutive entries
    int last = (history_head + HISTORY_SIZE - 1) % HISTORY_SIZE;
    if (history_count > 0 && strcmp(history[last], line) == 0) return;

    strncpy(history[history_head], line, LINE_MAX - 1);
    history[history_head][LINE_MAX - 1] = '\0';
    history_head = (history_head + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

// Returns entry relative to end: index 0 = most recent, 1 = one before, etc.
static const char *history_get(int rel)
{
    if (rel < 0 || rel >= history_count) return NULL;
    int idx = (history_head - 1 - rel + HISTORY_SIZE * 2) % HISTORY_SIZE;
    return history[idx];
}

// ─── MOTD ─────────────────────────────────────────────────────────────────────
static const char *motd_jokes[] = {
    "Dlaczego programista nosil okulary? Bo nie mogl C#!",
    "Co mowi kernel do procesu? Lepiej cie nie widze!",
    "Jaki jest ulubiony jezyk programisty w Polsce? C++owiak!",
    "Dlaczego Linux jest lepszy od Windows? Bo zna sie na swojej robocie!",
    "Co to jest null pointer? Problem nastepnego programisty.",
    "Rekurencja: patrz 'Rekurencja'.",
    "99 bledow w kodzie... wez jeden popraw... 127 bledow w kodzie.",
    "Kompilacja zakonczyna sukcesem. Dzialanie -- to juz nie moja sprawa.",
};
#define MOTD_COUNT ((int)(sizeof(motd_jokes) / sizeof(motd_jokes[0])))

// ─── Pomocnicze ───────────────────────────────────────────────────────────────

// Print a string in the given color, then reset to white
static void print_colored(const char *s, u32 color)
{
    fb_set_color(color, COLOR_BLACK);
    fb_puts(s);
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
}

// Erase N characters on the current terminal line
static void erase_chars(int n)
{
    for (int i = 0; i < n; i++) {
        fb_putchar('\b');
        fb_putchar(' ');
        fb_putchar('\b');
    }
}

// ─── Czytanie linii z historią i backspace ────────────────────────────────────
static void shell_readline(char *buf, int maxlen)
{
    int  pos       = 0;      // current cursor position in buf
    int  len       = 0;      // current string length
    int  hist_idx  = -1;     // -1 = not browsing history
    char saved[LINE_MAX];    // saved in-progress input when browsing history
    saved[0] = '\0';

    while (1) {
        char c = kb_getchar();

        // ── Escape sequence (arrow keys) ──────────────────────────────────────
        if (c == '\x1b') {
            char c2 = kb_getchar();
            if (c2 == '[') {
                char c3 = kb_getchar();
                if (c3 == 'A') {
                    // UP arrow — older history entry
                    if (hist_idx == -1) {
                        // Save current input
                        strncpy(saved, buf, maxlen);
                        saved[len] = '\0';
                    }
                    int next = hist_idx + 1;
                    const char *entry = history_get(next);
                    if (entry) {
                        hist_idx = next;
                        erase_chars(len);
                        strncpy(buf, entry, maxlen - 1);
                        buf[maxlen - 1] = '\0';
                        len = (int)strlen(buf);
                        pos = len;
                        fb_puts(buf);
                    }
                } else if (c3 == 'B') {
                    // DOWN arrow — newer history entry
                    if (hist_idx <= 0) {
                        // Already at the live input or nothing to go forward to
                        if (hist_idx == 0) {
                            hist_idx = -1;
                            erase_chars(len);
                            strncpy(buf, saved, maxlen - 1);
                            buf[maxlen - 1] = '\0';
                            len = (int)strlen(buf);
                            pos = len;
                            fb_puts(buf);
                        }
                    } else {
                        int next = hist_idx - 1;
                        erase_chars(len);
                        if (next < 0) {
                            hist_idx = -1;
                            strncpy(buf, saved, maxlen - 1);
                            buf[maxlen - 1] = '\0';
                        } else {
                            const char *entry = history_get(next);
                            if (entry) {
                                strncpy(buf, entry, maxlen - 1);
                                buf[maxlen - 1] = '\0';
                                hist_idx = next;
                            }
                        }
                        len = (int)strlen(buf);
                        pos = len;
                        fb_puts(buf);
                    }
                }
                // Other sequences (e.g. left/right) ignored for simplicity
            }
            continue;
        }

        // ── Enter ─────────────────────────────────────────────────────────────
        if (c == '\n' || c == '\r') {
            fb_putchar('\n');
            buf[len] = '\0';
            return;
        }

        // ── Backspace ─────────────────────────────────────────────────────────
        if (c == '\b' || c == 0x7f) {
            if (pos > 0) {
                pos--;
                len--;
                // Shift chars left from cursor
                for (int i = pos; i < len; i++) buf[i] = buf[i + 1];
                buf[len] = '\0';
                // Redraw: move back, reprint rest, blank last char
                fb_putchar('\b');
                for (int i = pos; i < len; i++) fb_putchar(buf[i]);
                fb_putchar(' ');
                // Return cursor to position
                for (int i = pos; i < len + 1; i++) fb_putchar('\b');
            }
            continue;
        }

        // ── Ctrl+U — clear line ───────────────────────────────────────────────
        if (c == 0x15) {
            erase_chars(len);
            len = 0;
            pos = 0;
            buf[0] = '\0';
            continue;
        }

        // ── Regular printable character ───────────────────────────────────────
        if (c >= 0x20 && c < 0x7f && len < maxlen - 1) {
            // Insert at pos
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos] = c;
            len++;
            pos++;
            buf[len] = '\0';
            fb_putchar(c);
            // If inserting in the middle, reprint tail and move back
            if (pos < len) {
                for (int i = pos; i < len; i++) fb_putchar(buf[i]);
                for (int i = pos; i < len; i++) fb_putchar('\b');
            }
            hist_idx = -1;
        }
    }
}

// ─── Parser argumentów ────────────────────────────────────────────────────────
static int parse_args(char *line, char *args[], int max_args)
{
    int count = 0;
    char *p = line;
    while (*p && count < max_args) {
        while (*p == ' ') p++;
        if (!*p) break;
        args[count++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; p++; }
    }
    return count;
}

// ─── Komendy ──────────────────────────────────────────────────────────────────

static void cmd_pomoc(void)
{
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("Dostepne polecenia:\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts("  pomoc           - wyswietl te pomoc\n");
    fb_puts("  info            - informacje o systemie\n");
    fb_puts("  pamiec          - statystyki pamieci\n");
    fb_puts("  siec            - konfiguracja sieci\n");
    fb_puts("  ping <ip>       - wyslij ping na adres IP\n");
    fb_puts("  dns <host>      - rozwiaz nazwe DNS\n");
    fb_puts("  pci             - lista urzadzen PCI\n");
    fb_puts("  gpu             - informacje o kartach graficznych\n");
    fb_puts("  czas            - aktualny czas i data\n");
    fb_puts("  dysk            - informacje o dysku NVMe\n");
    fb_puts("  wyczysc         - wyczysc ekran\n");
    fb_puts("  wylacz          - wylacz komputer\n");
    fb_puts("  restart         - zrestartuj komputer\n");
    fb_puts("  panika          - wymus panic jadra\n");
    fb_puts("  echo <tekst>    - wyswietl tekst\n");
    fb_puts("  hex <addr> <n>  - wyswietl zawartosc pamieci\n");
    fb_puts("  uslugi          - lista uslug systemowych\n");
    fb_puts("  instaluj        - zainstaluj system na dysku\n");
    fb_puts("  pulpit          - uruchom pulpit graficzny\n");
    fb_puts("  ls [sciezka]    - wyswietl zawartosc katalogu VFS\n");
}

static void cmd_info(void)
{
    u64 uptime = kernel_ticks / 1000;
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("PolandOS v0.0.1 -- Jadro Orzel\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts("Architektura: x86_64\n");
    kprintf("Uptime: %llu sekund\n", uptime);
}

static void cmd_pamiec(void)
{
    u64 total = pmm_total_bytes() / (1024 * 1024);
    u64 used  = pmm_used_bytes()  / (1024 * 1024);
    u64 free  = pmm_free_bytes()  / (1024 * 1024);

    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("Statystyki pamieci:\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("  Lacznie:  %llu MB\n", total);
    kprintf("  Uzyte:    %llu MB\n", used);
    kprintf("  Wolne:    %llu MB\n", free);
}

static void cmd_siec(void)
{
    char mac_str[20];
    char ip_str[16];
    char gw_str[16];
    char sn_str[16];
    char dns_str[16];

    mac_to_str(net_mac, mac_str);
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("Konfiguracja sieci:\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("  MAC:     %s\n", mac_str);

    if (net_configured) {
        ip_to_str(net_ip,      ip_str);
        ip_to_str(net_gateway, gw_str);
        ip_to_str(net_subnet,  sn_str);
        ip_to_str(net_dns,     dns_str);
        kprintf("  IP:      %s\n", ip_str);
        kprintf("  Brama:   %s\n", gw_str);
        kprintf("  Maska:   %s\n", sn_str);
        kprintf("  DNS:     %s\n", dns_str);
        print_colored("  Siec skonfigurowana\n", COLOR_GREEN);
    } else {
        print_colored("  Brak konfiguracji IP\n", COLOR_YELLOW);
    }
}

static void cmd_ping(char *args[], int argc)
{
    if (argc < 2) {
        print_colored("Uzycie: ping <adres_ip>\n", COLOR_YELLOW);
        return;
    }
    u32 ip = str_to_ip(args[1]);
    if (!ip) {
        print_colored("Nieprawidlowy adres IP\n", COLOR_RED);
        return;
    }

    char ip_str[16];
    ip_to_str(ip, ip_str);
    kprintf("PING %s:\n", ip_str);

    for (int seq = 1; seq <= 4; seq++) {
        int rtt = icmp_ping(ip, (u16)seq);
        if (rtt >= 0) {
            kprintf("  %d: czas=%d ms\n", seq, rtt);
        } else {
            fb_set_color(COLOR_YELLOW, COLOR_BLACK);
            kprintf("  %d: Brak odpowiedzi\n", seq);
            fb_set_color(COLOR_WHITE, COLOR_BLACK);
        }
    }
}

static void cmd_dns(char *args[], int argc)
{
    if (argc < 2) {
        print_colored("Uzycie: dns <hostname>\n", COLOR_YELLOW);
        return;
    }
    u32 ip = 0;
    if (dns_resolve(args[1], &ip) == 0) {
        char ip_str[16];
        ip_to_str(ip, ip_str);
        kprintf("%s -> %s\n", args[1], ip_str);
    } else {
        print_colored("Blad DNS\n", COLOR_RED);
    }
}

static void cmd_pci(void)
{
    pci_list_devices();
}

static void cmd_gpu(void)
{
    gpu_list_devices();
}

static void cmd_czas(void)
{
    RTCTime t = rtc_read();
    kprintf("%02u.%02u.%04u %02u:%02u:%02u\n",
            t.day, t.month, t.year,
            t.hours, t.minutes, t.seconds);
}

static void cmd_dysk(void)
{
    u64 blocks    = nvme_get_block_count(1);
    u32 blk_size  = nvme_get_block_size(1);

    if (!blocks || !blk_size) {
        print_colored("Brak dysku NVMe lub dysk niedostepny\n", COLOR_YELLOW);
        return;
    }

    u64 bytes = blocks * blk_size;
    u64 mb    = bytes / (1024ULL * 1024ULL);
    u64 gb    = mb    / 1024ULL;

    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("Dysk NVMe:\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("  Bloki:      %llu\n",    blocks);
    kprintf("  Rozmiar:    %u B\n",    blk_size);
    if (gb > 0)
        kprintf("  Pojemnosc:  %llu GB (%llu MB)\n", gb, mb);
    else
        kprintf("  Pojemnosc:  %llu MB\n", mb);
}

static void cmd_echo(char *args[], int argc)
{
    for (int i = 1; i < argc; i++) {
        fb_puts(args[i]);
        if (i + 1 < argc) fb_putchar(' ');
    }
    fb_putchar('\n');
}

// Parse a hex number (with optional 0x prefix)
static u64 parse_hex(const char *s)
{
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    u64 val = 0;
    while (*s) {
        char c = *s++;
        u8 digit;
        if (c >= '0' && c <= '9')      digit = (u8)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (u8)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = (u8)(c - 'A' + 10);
        else break;
        val = (val << 4) | digit;
    }
    return val;
}

static void cmd_hex(char *args[], int argc)
{
    if (argc < 3) {
        print_colored("Uzycie: hex <adres_hex> <n_bajtow>\n", COLOR_YELLOW);
        return;
    }
    u64 addr  = parse_hex(args[1]);
    int count = atoi(args[2]);
    if (count <= 0 || count > 4096) count = 64;

    const u8 *ptr = (const u8 *)(uintptr_t)addr;

    for (int row = 0; row < count; row += 16) {
        kprintf("%016llx: ", addr + (u64)row);

        // Hex part
        for (int col = 0; col < 16; col++) {
            if (row + col < count)
                kprintf("%02x ", ptr[row + col]);
            else
                fb_puts("   ");
        }

        fb_puts("| ");

        // ASCII part
        for (int col = 0; col < 16 && row + col < count; col++) {
            u8 b = ptr[row + col];
            fb_putchar((b >= 0x20 && b < 0x7f) ? (char)b : '.');
        }
        fb_putchar('\n');
    }
}

// ─── Shell init / run ─────────────────────────────────────────────────────────

void shell_init(void)
{
    history_count = 0;
    history_head  = 0;
}

void shell_run(void)
{
    shell_init();

    // MOTD
    fb_set_color(COLOR_GREEN, COLOR_BLACK);
    fb_puts("\n*** PolandOS v0.0.1 — Jadro Orzel ***\n");
    fb_set_color(COLOR_YELLOW, COLOR_BLACK);
    kprintf("Dowcip: %s\n", motd_jokes[kernel_ticks % (u64)MOTD_COUNT]);
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts("Wpisz 'pomoc' aby wyswietlic liste polecen.\n\n");

    char line[LINE_MAX];
    char *args[ARGS_MAX];

    while (1) {
        // Prompt
        fb_set_color(COLOR_YELLOW, COLOR_BLACK);
        fb_puts("PolandOS> ");
        fb_set_color(COLOR_WHITE, COLOR_BLACK);

        // Read input
        line[0] = '\0';
        shell_readline(line, LINE_MAX);

        // Skip empty lines
        if (!line[0]) continue;

        // Add to history
        history_push(line);

        // Parse arguments
        int argc = parse_args(line, args, ARGS_MAX);
        if (argc == 0) continue;

        const char *cmd = args[0];

        // ── Dispatch ──────────────────────────────────────────────────────────
        if (strcmp(cmd, "pomoc") == 0) {
            cmd_pomoc();
        } else if (strcmp(cmd, "info") == 0) {
            cmd_info();
        } else if (strcmp(cmd, "pamiec") == 0) {
            cmd_pamiec();
        } else if (strcmp(cmd, "siec") == 0) {
            cmd_siec();
        } else if (strcmp(cmd, "ping") == 0) {
            cmd_ping(args, argc);
        } else if (strcmp(cmd, "dns") == 0) {
            cmd_dns(args, argc);
        } else if (strcmp(cmd, "pci") == 0) {
            cmd_pci();
        } else if (strcmp(cmd, "gpu") == 0) {
            cmd_gpu();
        } else if (strcmp(cmd, "czas") == 0) {
            cmd_czas();
        } else if (strcmp(cmd, "dysk") == 0) {
            cmd_dysk();
        } else if (strcmp(cmd, "wyczysc") == 0) {
            fb_clear(COLOR_BLACK);
        } else if (strcmp(cmd, "wylacz") == 0) {
            fb_set_color(COLOR_YELLOW, COLOR_BLACK);
            fb_puts("Zamykanie systemu...\n");
            acpi_power_off();
        } else if (strcmp(cmd, "restart") == 0) {
            fb_set_color(COLOR_YELLOW, COLOR_BLACK);
            fb_puts("Restartowanie systemu...\n");
            acpi_reset();
        } else if (strcmp(cmd, "panika") == 0) {
            kpanic("Test paniki jadra");
        } else if (strcmp(cmd, "echo") == 0) {
            cmd_echo(args, argc);
        } else if (strcmp(cmd, "hex") == 0) {
            cmd_hex(args, argc);
        } else if (strcmp(cmd, "uslugi") == 0) {
            svc_print_status();
        } else if (strcmp(cmd, "instaluj") == 0) {
            installer_run();
        } else if (strcmp(cmd, "pulpit") == 0) {
            fb_set_color(COLOR_YELLOW, COLOR_BLACK);
            fb_puts("Uruchamianie pulpitu graficznego...\n");
            desktop_run();
        } else if (strcmp(cmd, "ls") == 0) {
            const char *path = (argc >= 2) ? args[1] : "/";
            VFSNode *node = vfs_lookup(path);
            if (node) {
                vfs_list_dir(node);
            } else {
                print_colored("Nie znaleziono sciezki\n", COLOR_RED);
            }
        } else {
            fb_set_color(COLOR_RED, COLOR_BLACK);
            kprintf("Nieznane polecenie: %s.", cmd);
            fb_set_color(COLOR_WHITE, COLOR_BLACK);
            fb_puts(" Wpisz 'pomoc' aby uzyskac pomoc.\n");
        }
    }
}
