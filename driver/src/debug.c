#define _GNU_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "../external/cJSON/cJSON.h"
#include "./check.c"
#include "./common.h"

short programNum = 0;
long partCount = -1;
char deviceID[MAXLEN];

static volatile int runningCondition = 0;
void intHandler(int sig) { runningCondition = 1; }
const long minimum_interval = (1.5) * 1e6;

int main() {
  char deviceIP[MAXPATH] = "127.0.0.1";
  int devicePort = 8193;
  struct timespec t0, t1;
  unsigned long tt;
  cJSON *updates, *meta;

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  updates = cJSON_CreateObject();
  meta = cJSON_CreateObject();

  strcpy(deviceIP, "127.0.0.1");
  devicePort = 8193;

  if (setupConnection(deviceIP, devicePort)) {
    fprintf(stderr, "failed to setup machine connection!\n");
    exit(EXIT_FAILURE);
    return 1;
  };

  if (checkMachineInfo(updates, meta)) {
    fprintf(stderr, "failed to read machine info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  do {
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    if (checkMachinePartCount(updates, meta) ||
        checkMachineCycleTime(updates, meta) ||
        checkMachineStatus(updates, meta) ||
        checkMachineToolInfo(updates, meta) ||
        checkMachineDynamic(updates, meta) ||
        checkMachineMessage(updates, meta) ||
        checkMachineProgramName(updates, meta) ||
        checkMachineProgram(updates, meta) ||
        checkMachineBlock(updates, meta)) {
      fprintf(stderr, "failed to check machine values\n");
      exit(EXIT_FAILURE);
      return 1;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
    printf("loop took %ld\n", tt);

    cJSON *total_meta_datum = cJSON_CreateNumber(tt);
    cJSON_AddItemToObject(meta, "total", total_meta_datum);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "values", updates);
    cJSON_AddItemToObject(obj, "meta", meta);

    char *serialized = cJSON_Print(obj);
    cJSON_Delete(obj);
    updates = cJSON_CreateObject();
    meta = cJSON_CreateObject();

    printf("%s\n", serialized);
    free(serialized);

    if (tt < minimum_interval) {
      printf("sleeping for %ld\n", minimum_interval - tt);
      usleep(minimum_interval - tt);
    }

  } while (!runningCondition);

  exit(EXIT_SUCCESS);
}
