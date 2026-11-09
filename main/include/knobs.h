#pragma once
#include <stdbool.h>

void knobs_init(void);
void knobs_task(void *pvParameters);
void knobs_resync(void);
bool knobs_set_macro(const char *name);
const char *knobs_get_macro(void);
