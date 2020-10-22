#define _GNU_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../external/cJSON/cJSON.h"
#include "../external/fwlib/fwlib32.h"
#include "../external/librdkafka/src/rdkafka.h"
#include "./check.c"
#include "./common.h"

short unsigned int fLibHandle;

char deviceIP[MAXPATH] = "127.0.0.1";
char deviceID[MAXLEN];
char machineName[MAXLEN];
short programNum = 0;
long partCount = -1;
int devicePort = 8193;
const double minimum_interval = 0.5;
char *brokers = "localhost:9092"; /* Argument: broker list */
char *topic = "input";            /* Argument: topic to produce to */

static volatile int runningCondition = 0;

int setupEnv() {
  char *pTmp;
  if ((pTmp = getenv("DEVICE_IP")) != NULL) {
    sprintf(deviceIP, "%s", pTmp);
  }

  if ((pTmp = getenv("DEVICE_PORT")) != NULL) {
    char dp[10];
    char *ptr;
    memset(dp, '\0', 10);
    strncpy(dp, pTmp, 10 - 1);
    devicePort = strtol(dp, &ptr, 10);
    if (devicePort <= 0 || devicePort > 65535) {
      fprintf(stderr, "invalid DEVICE_PORT: %s\n", pTmp);
      return 1;
    }
  }

  if ((pTmp = getenv("MACHINE_NAME")) != NULL) {
    sprintf(machineName, "%s", pTmp);
  }

  if ((pTmp = getenv("KAFKA_BROKERS")) != NULL) {
    brokers = (char *)malloc(strlen(pTmp) + 1);
    strcpy(brokers, pTmp);
  }

  if ((pTmp = getenv("KAFKA_TOPIC")) != NULL) {
    topic = (char *)malloc(strlen(pTmp) + 1);
    strcpy(topic, pTmp);
  }

  return 0;
}

void intHandler(int sig) { runningCondition = 1; }

int loop_setup(cJSON **updatesPtr, cJSON **metaPtr) {
  atexit(cleanup);

  *updatesPtr = cJSON_CreateObject();
  *metaPtr = cJSON_CreateObject();

  if (checkMachineInfo(*updatesPtr, *metaPtr)) {
    fprintf(stderr, "failed to read machine info!\n");
    return 1;
  }

  return 0;
}

int loop_tick(cJSON *updates, cJSON *meta) {
  if (checkMachinePartCount(updates, meta) ||
      checkMachineCycleTime(updates, meta) ||
      checkMachineStatus(updates, meta) ||
      checkMachineToolInfo(updates, meta) ||
      checkMachineDynamic(updates, meta) ||
      checkMachineMessage(updates, meta) ||
      checkMachineProgramName(updates, meta) ||
      checkMachineProgram(updates, meta) || checkMachineBlock(updates, meta)) {
    fprintf(stderr, "failed to check machine values\n");
    return 1;
  }
  return 0;
}

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage,
                      void *opaque) {
  if (rkmessage->err) {
    fprintf(stderr, "%% Message delivery failed: %s\n",
            rd_kafka_err2str(rkmessage->err));
  }
  // fprintf(stderr,
  //        "%% Message delivered (%zd bytes, "
  //        "partition %" PRId32 ")\n",
  //        rkmessage->len, rkmessage->partition);
}

int main(int argc, char **argv) {
  rd_kafka_t *rk;        /* Producer instance handle */
  rd_kafka_conf_t *conf; /* Temporary configuration object */
  char errstr[512];      /* librdkafka API error reporting buffer */

  setbuf(stdout, NULL);

  if (setupEnv()) {
    fprintf(stderr, "failed to configure environment variables\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  printf("using kafka brokers: \"%s\" and topic \"%s\"\n", brokers, topic);

  conf = rd_kafka_conf_new();

  if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%s\n", errstr);
    exit(EXIT_FAILURE);
    return 1;
  }
  rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

  rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));

  if (!rk) {
    fprintf(stderr, "%% Failed to create new producer: %s\n", errstr);
    exit(EXIT_FAILURE);
    return 1;
  }

  if (setupConnection(deviceIP, devicePort)) {
    fprintf(stderr, "failed to setup machine connection!\n");
    exit(EXIT_FAILURE);
    return 1;
  };

  cJSON *updates = NULL;
  cJSON *meta = NULL;

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  if (loop_setup(&updates, &meta)) {
    fprintf(stderr, "failed to setup loop\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  struct timespec t0, t1;
  unsigned long tt;

  do {
    rd_kafka_resp_err_t err;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

    if (loop_tick(updates, meta)) {
      fprintf(stderr, "loop check failed\n");
      exit(EXIT_FAILURE);
      return 1;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;

    cJSON *total_meta_datum = cJSON_CreateNumber(tt);
    cJSON_AddItemToObject(meta, "total", total_meta_datum);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "values", updates);
    cJSON_AddItemToObject(obj, "meta", meta);
    char *serialized = cJSON_PrintUnformatted(obj);

    size_t vlen = strlen(serialized);
    size_t klen = strlen(deviceID);

    if (vlen == 0) {
      rd_kafka_poll(rk, 0 /*non-blocking */);
      continue;
    }

    err = rd_kafka_producev(
        rk, RD_KAFKA_V_TOPIC(topic), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_VALUE(serialized, vlen), RD_KAFKA_V_KEY(deviceID, klen),
        RD_KAFKA_V_OPAQUE(NULL), RD_KAFKA_V_END);

    if (err) {
      fprintf(stderr, "%% Failed to produce to topic %s: %s\n", topic,
              rd_kafka_err2str(err));

      if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
        fprintf(stderr, "error: queue full\n");
      }
    }

    rd_kafka_poll(rk, 0 /*non-blocking*/);

    if (tt < minimum_interval) {
      usleep((long)((minimum_interval - tt) * 1e6));
    }

    free(serialized);
    cJSON_Delete(obj);
    updates = cJSON_CreateObject();
    meta = cJSON_CreateObject();
  } while (!runningCondition);

  cJSON_Delete(updates);

  fprintf(stderr, "%% Flushing final messages..\n");
  rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);

  /* If the output queue is still not empty there is an issue
   * with producing messages to the clusters. */
  if (rd_kafka_outq_len(rk) > 0) {
    fprintf(stderr, "%% %d message(s) were not delivered\n",
            rd_kafka_outq_len(rk));
  }

  rd_kafka_destroy(rk);
  exit(EXIT_SUCCESS);
}
