// PolandOS — Menedzer uslug (OpenRC-style)
// Jadro Orzel — system startowy
#pragma once
#include "../../include/types.h"

// ─── Status uslugi ───────────────────────────────────────────────────────────
typedef enum {
    SVC_STOPPED = 0,
    SVC_STARTING,
    SVC_RUNNING,
    SVC_FAILED,
} ServiceStatus;

// ─── Typ uslugi ──────────────────────────────────────────────────────────────
typedef int (*svc_init_fn)(void);   // returns 0 = ok, <0 = fail

typedef struct {
    const char    *name;        // display name (Polish)
    const char    *desc;        // short description
    svc_init_fn    init;        // init function (NULL = no-op)
    ServiceStatus  status;      // current status
    bool           critical;    // if true, panic on failure
} Service;

// ─── API ─────────────────────────────────────────────────────────────────────

#define SVC_MAX 32

// Register a service (returns index, or -1 on error)
int  svc_register(const char *name, const char *desc, svc_init_fn init, bool critical);

// Start all registered services in order (OpenRC-style animation)
void svc_start_all(void);

// Get number of registered services
int  svc_count(void);

// Get service by index
const Service *svc_get(int idx);

// Print service status table (for 'uslugi' shell command)
void svc_print_status(void);
