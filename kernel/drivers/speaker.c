// PolandOS — Sterownik brzeczyka PC + Mazurek Dąbrowskiego
// PIT kanał 2 steruje głośniczkiem systemowym
#include "speaker.h"
#include "timer.h"
#include "../../include/io.h"
#include "../lib/printf.h"

// ─── PIT constants ────────────────────────────────────────────────────────────
#define PIT_BASE_FREQ   1193182u   // PIT input frequency in Hz
#define PIT_CMD         0x43       // PIT command register
#define PIT_CH2         0x42       // PIT channel 2 data port

// PIT command: channel 2, lobyte/hibyte, mode 3 (square wave), binary
#define PIT_CMD_CH2_MODE3  0xB6

// ─── PC speaker control (port 0x61) ──────────────────────────────────────────
#define SPEAKER_PORT    0x61
#define SPEAKER_GATE    (1u << 0)   // connect PIT ch2 output to speaker
#define SPEAKER_DATA    (1u << 1)   // speaker data bit

// ─── speaker_init ─────────────────────────────────────────────────────────────
void speaker_init(void) {
    speaker_off();
    kprintf("[DOBRZE] Speaker: inicjalizacja zakonczona\n");
}

// ─── speaker_tone ─────────────────────────────────────────────────────────────
void speaker_tone(u32 frequency) {
    if (frequency == 0) {
        speaker_off();
        return;
    }

    // Clamp to reasonable audio range
    if (frequency < 20)   frequency = 20;
    if (frequency > 20000) frequency = 20000;

    u32 divisor = PIT_BASE_FREQ / frequency;

    // Programme PIT channel 2 for mode 3 (square wave)
    outb(PIT_CMD, PIT_CMD_CH2_MODE3);
    outb(PIT_CH2, (u8)(divisor & 0xFF));         // low byte
    outb(PIT_CH2, (u8)((divisor >> 8) & 0xFF));  // high byte

    // Enable speaker: set bits 0 and 1 of port 0x61
    u8 ctrl = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, ctrl | SPEAKER_GATE | SPEAKER_DATA);
}

// ─── speaker_off ──────────────────────────────────────────────────────────────
void speaker_off(void) {
    u8 ctrl = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, ctrl & (u8)~(SPEAKER_GATE | SPEAKER_DATA));
}

// ─── speaker_beep ─────────────────────────────────────────────────────────────
void speaker_beep(u32 freq, u32 ms) {
    speaker_tone(freq);
    timer_sleep_ms((u64)ms);
    speaker_off();
}

// ─── Mazurek Dąbrowskiego ─────────────────────────────────────────────────────
// Polska – nie zginęła, póki my żyjemy!
// Note frequencies (Hz) for the opening bars:
//   C4=262, D4=294, E4=330, F4=349, G4=392, A4=440, B4=494
//   C5=523, D5=587, E5=659, F5=698, G5=784

void play_mazurek_dabrowskiego(void) {
    // Note duration in ms
    #define Q  250   // quarter note
    #define E  125   // eighth note
    #define H  500   // half note
    #define R    0   // rest marker

    // Frequency 0 means rest
    static const u32 notes[] = {
        // Bar 1-2: "Jeszcze Polska nie zginęła"
        392, 392, 440, 494,   // G4 G4 A4 B4
        523, 523, 494, 440,   // C5 C5 B4 A4
        // Bar 3-4: "kiedy my żyjemy"
        392, 440, 392, 349,   // G4 A4 G4 F4
        330, 294, 262,   0,   // E4 D4 C4 rest
        // Bar 5-6: "Co nam obca przemoc wzięła"
        294, 294, 349, 392,   // D4 D4 F4 G4
        392, 349, 294, 262,   // G4 F4 D4 C4 (adj)
        // Bar 7-8: "szablą odbierzemy"
        294, 392, 392, 330,   // D4 G4 G4 E4
        349, 294, 262,   0,   // F4 D4 C4 rest
        // Bar 9-10: "Marsz, marsz, Dąbrowski"
        392, 392, 523, 523,   // G4 G4 C5 C5
        523, 494, 440, 392,   // C5 B4 A4 G4
        // Bar 11-12: "z ziemi włoskiej do Polski"
        440, 440, 494, 523,   // A4 A4 B4 C5
        587, 523, 494, 440,   // D5 C5 B4 A4
        // Bar 13-14: "za twoim przewodem"
        523, 523, 659, 659,   // C5 C5 E5 E5
        659, 587, 523, 494,   // E5 D5 C5 B4
        // Bar 15-16: "złączym się z narodem"
        523, 392, 440, 494,   // C5 G4 A4 B4
        523,   0,   0,   0,   // C5 rest rest rest
    };

    static const u32 durations[] = {
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, H, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, H, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        Q, Q, Q, Q,
        H, Q, Q, Q,
    };

    kprintf("[Speaker] Mazurek Dabrowskiego — Grajmy!\n");

    u32 n = sizeof(notes) / sizeof(notes[0]);
    for (u32 i = 0; i < n; i++) {
        u32 freq = notes[i];
        u32 dur  = durations[i];

        if (freq == 0 || freq == R) {
            // Rest: silence for the duration
            speaker_off();
            timer_sleep_ms((u64)dur);
        } else {
            // Play note for (duration - 50ms gap) then silence for 50ms
            u32 play_ms = (dur > 50) ? (dur - 50) : dur;
            u32 gap_ms  = (dur > 50) ? 50 : 0;
            speaker_tone(freq);
            timer_sleep_ms((u64)play_ms);
            speaker_off();
            if (gap_ms)
                timer_sleep_ms((u64)gap_ms);
        }
    }

    kprintf("[Speaker] Mazurek Dabrowskiego — Koniec!\n");

    #undef Q
    #undef E
    #undef H
    #undef R
}
