#include <libconfig.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char deviceIP[];
extern int devicePort;

int read_config(const char *cfg_file, config_t *cfg) {
  int ret;
  int iTmp;
  char *pTmp;
  bool config_avail = true;
  config_init(cfg);

  if (!config_read_file(cfg, cfg_file)) {
    config_avail = false;
    fprintf(stderr, "unable to read config file \"%s\"\n", cfg_file);
  }

  if ((pTmp = getenv("DEVICE_IP")) != NULL) {
    sprintf(deviceIP, "%s", pTmp);
  } else if (config_avail && config_lookup_string(cfg, "ip", &deviceIP)) {
    // set by libconfig
  } else {
    // use default
  }

  if (((pTmp = getenv("DEVICE_PORT")) != NULL) && (iTmp = atoi(pTmp)) > 0) {
    devicePort = iTmp;
  } else if (config_avail && config_lookup_int(cfg, "port", &devicePort)) {
    // set by libconfig
  } else {
    // use default
  }

  if (!config_lookup_int(cfg, "machine_host", &deviceHost)) {
    fprintf(stderr,
            "'machine_port' definition required in configuration file.\n");
    ret = 1;
    goto cleanup;
  }

  if (!config_lookup_int(cfg, "machine_port", &devicePort)) {
    fprintf(stderr,
            "'machine_port' definition required in configuration file.\n");
    ret = 1;
    goto cleanup;
  }

  if (!config_lookup_float(cfg, "minimum_polling_interval",
                           &minimum_interval)) {
    fprintf(stderr,
            "'minimum_polling_interval' definition required in configuration "
            "file.\n");
    ret = 1;
    goto cleanup;
  }

cleanup:
  config_destroy(cfg);

  return ret;
}
