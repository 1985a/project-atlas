#ifndef PPM_DEFAULT_H
#define PPM_DEFAULT_H

int ppm_set_default_sink(const char *name);
int ppm_set_default_source(const char *name);

/*
 * Opens the PipeWire registry, resolves the sink by name,
 * sets it as default and closes the registry.
 */
int ppm_apply_default_sink(const char *name);
int ppm_apply_default_source(const char *name);

#endif
