// PolandOS — Resolver DNS
#pragma once
#include "../../include/types.h"

int dns_resolve(const char *hostname, u32 *ip_out);  // 0=success, -1=fail
