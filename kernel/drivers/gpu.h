// PolandOS — Sterownik GPU (detekcja kart graficznych)
#pragma once
#include "../../include/types.h"

#define GPU_MAX_DEVICES 8

typedef struct {
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  bus, slot, func;
    u64 bar0;
    u64 fb_size;
    const char *type_name;
    const char *vendor_name;
} GPUDevice;

extern GPUDevice gpu_devices[GPU_MAX_DEVICES];
extern int       gpu_device_count;

void gpu_init(void);
void gpu_list_devices(void);
