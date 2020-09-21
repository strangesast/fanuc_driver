#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "./data.h"
#include "./external/cJSON/cJSON.h"
#include "./external/fwlib/fwlib32.h"
#include "./external/librdkafka/src/rdkafka.h"

#define MAXPATH 1024
#define BILLION 1000000000.0

short unsigned int fLibHandle;

char deviceIP[MAXPATH] = "127.0.0.1";
char deviceID[128];
char machineName[128];
short programNum = 0;
long partCount = -1;
int devicePort = 8193;

static volatile int runningCondition = 0;

bool jsonEqual(cJSON *a, cJSON *b) {
  char *aa = cJSON_PrintUnformatted(a);
  char *bb = cJSON_PrintUnformatted(b);
  bool eql = strcmp(aa, bb) == 0;
  free(aa);
  free(bb);
  return eql;
}

void jsonDebug(char *ref, cJSON *o) {
  char *string = cJSON_Print(o);
  printf("%s%s\n", ref, string);
  free(string);
}

int serializeMachineInfo(MachineInfo *v, cJSON *datum) {
  cJSON *info_datum = cJSON_CreateObject();

  cJSON *id_datum = cJSON_CreateString(v->id);
  if (id_datum == NULL) {
    fprintf(stderr, "failed to allocate id\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "id", id_datum);

  cJSON *max_axis_datum = cJSON_CreateNumber(v->max_axis);
  if (max_axis_datum == NULL) {
    fprintf(stderr, "failed to allocate max_axis\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "max_axis", max_axis_datum);

  cJSON *addinfo_datum = cJSON_CreateNumber(v->addinfo);
  if (addinfo_datum == NULL) {
    fprintf(stderr, "failed to allocate addinfo\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "addinfo", addinfo_datum);

  cJSON *cnc_type_datum = cJSON_CreateString(v->cnc_type);
  if (cnc_type_datum == NULL) {
    fprintf(stderr, "failed to allocate cnc_type\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "cnc_type", cnc_type_datum);

  cJSON *mt_type_datum = cJSON_CreateString(v->mt_type);
  if (mt_type_datum == NULL) {
    fprintf(stderr, "failed to allocate mt_type\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "mt_type", mt_type_datum);

  cJSON *series_datum = cJSON_CreateString(v->series);
  if (series_datum == NULL) {
    fprintf(stderr, "failed to allocate series\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "series", series_datum);

  cJSON *version_datum = cJSON_CreateString(v->version);
  if (version_datum == NULL) {
    fprintf(stderr, "failed to allocate version\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "version", version_datum);

  cJSON *axes_count_datum = cJSON_CreateNumber(v->axes_count);
  if (axes_count_datum == NULL) {
    fprintf(stderr, "failed to allocate axes_count\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "axes_count", axes_count_datum);

  cJSON *axes_count_chk_datum = cJSON_CreateString(v->axes_count_chk);
  if (axes_count_chk_datum == NULL) {
    fprintf(stderr, "failed to allocate axes_count_chk\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "axes_count_chk", axes_count_chk_datum);

  cJSON *ether_type_datum = cJSON_CreateNumber(v->etherType);
  if (ether_type_datum == NULL) {
    fprintf(stderr, "failed to allocate ether_type\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "ether_type", ether_type_datum);

  cJSON *ether_device_datum = cJSON_CreateNumber(v->etherDevice);
  if (ether_device_datum == NULL) {
    fprintf(stderr, "failed to allocate ether_device\n");
    return 1;
  }
  cJSON_AddItemToObject(info_datum, "ether_device", ether_device_datum);

  cJSON *axes_datum = cJSON_CreateArray();

  for (int i = 0; i < v->axes_count; i++) {
    cJSON *axis_datum = cJSON_CreateObject();

    cJSON *axis_id_datum = cJSON_CreateString(v->axes[i].id);
    if (axis_id_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_id %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "id", axis_id_datum);

    cJSON *axis_index_datum = cJSON_CreateNumber(i);
    if (axis_index_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_index %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "index", axis_index_datum);

    cJSON *axis_suffix_datum = cJSON_CreateString(v->axes[i].suffix);
    if (axis_suffix_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_suffix %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "suffix", axis_suffix_datum);

    cJSON *axis_divisor_datum = cJSON_CreateNumber(v->axes[i].divisor);
    if (axis_divisor_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_divisor %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "divisor", axis_divisor_datum);

    cJSON *axis_name_datum = cJSON_CreateString(v->axes[i].name);
    if (axis_name_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_name %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "name", axis_name_datum);

    cJSON *axis_flag_datum = cJSON_CreateNumber(v->axes[i].flag);
    if (axis_flag_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_flag %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "flag", axis_flag_datum);

    cJSON *axis_unit_datum = cJSON_CreateNumber(v->axes[i].unit);
    if (axis_unit_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_unit %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "unit", axis_unit_datum);

    cJSON *axis_unith_datum = cJSON_CreateString(v->axes[i].unith);
    if (axis_unith_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_unith %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "unith", axis_unith_datum);

    cJSON *axis_decimal_datum = cJSON_CreateNumber(v->axes[i].decimal);
    if (axis_decimal_datum == NULL) {
      fprintf(stderr, "failed to allocate axis_decimal %d\n", i);
      return 1;
    }
    cJSON_AddItemToObject(axis_datum, "decimal", axis_decimal_datum);

    cJSON_AddItemToArray(axes_datum, axis_datum);
  }

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(info_datum, "axes", axes_datum);
  cJSON_AddItemToObject(meta_datum, "meta", meta_datum);
  cJSON_AddItemToObject(datum, "machine", info_datum);

  return 0;
}

int checkMachineInfo(cJSON *updates) {
  static MachineInfo *lv = NULL;
  static cJSON *last = NULL;

  MachineInfo *v;
  v = malloc(sizeof(MachineInfo));

  // check new value
  if (getMachineInfo(v)) {
    free(v);
    fprintf(stderr, "failed to get machine info\n");
    return 1;
  }
  strncpy(deviceID, v->id, 128);

  // if changed or first call
  if (lv == NULL ||
      (lv->id != v->id || lv->max_axis != v->max_axis ||
       lv->addinfo != v->addinfo || lv->cnc_type != v->cnc_type ||
       lv->mt_type != v->mt_type || lv->series != v->series ||
       lv->version != v->version || lv->axes_count_chk != v->axes_count_chk ||
       lv->axes_count != v->axes_count || lv->etherType != v->etherType ||
       lv->etherDevice != v->etherDevice)) {
    if (lv != NULL) {
      free(lv);            // free old version
      cJSON_Delete(last);  // free old json
    }
    // update to new version
    lv = v;
    last = cJSON_CreateObject();
    // serialize into json
    if (serializeMachineInfo(lv, last)) {
      fprintf(stderr, "failed to serialize machine info\n");
      return 1;
    }
    cJSON_AddItemReferenceToArray(updates, last);
  } else {
    // it's unchanged, so free new version
    free(v);
  }
  return 0;
}

int serializeMachinePartCount(MachinePartCount *v, cJSON *datum) {
  if (getMachinePartCount(v)) {
    fprintf(stderr, "failed to get machine part count\n");
    return 1;
  }

  cJSON *part_count_datum = cJSON_CreateObject();
  cJSON *count = cJSON_CreateNumber(v->count);
  cJSON_AddItemToObject(part_count_datum, "count", count);

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(datum, "part_count", part_count_datum);
  cJSON_AddItemToObject(datum, "meta", meta_datum);

  return 0;
}

int checkMachinePartCount(cJSON *updates) {
  static MachinePartCount *lv = NULL;
  static cJSON *last = NULL;

  MachinePartCount *v;
  v = malloc(sizeof(MachinePartCount));

  if (getMachinePartCount(v)) {
    fprintf(stderr, "failed to read machine part count\n");
    return 1;
  }

  if (lv == NULL || (lv->count != v->count)) {
    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachinePartCount(lv, last)) {
      fprintf(stderr, "failed to serialize part count\n");
      return 1;
    }
    cJSON_AddItemReferenceToArray(updates, last);

  } else {
    free(v);
  }

  return 0;
}

int serializeMachineMessage(MachineMessage *v, cJSON *datum) {
  cJSON *message_datum;
  if (v->number != -1) {
    message_datum = cJSON_CreateObject();

    cJSON *num_datum = cJSON_CreateNumber(v->number);
    cJSON_AddItemToObject(message_datum, "num", num_datum);

    cJSON *text_datum = cJSON_CreateString(v->text);
    cJSON_AddItemToObject(message_datum, "text", text_datum);
  } else {
    message_datum = cJSON_CreateNull();
  }

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(datum, "meta", meta_datum);
  cJSON_AddItemToObject(datum, "message", message_datum);

  return 0;
}

int checkMachineMessage(cJSON *updates) {
  static MachineMessage *lv = NULL;
  static cJSON *last = NULL;

  MachineMessage *v;
  v = malloc(sizeof(MachineMessage));

  if (getMachineMessage(v)) {
    free(v);
    fprintf(stderr, "failed to read machine message\n");
    return 1;
  }

  if (lv == NULL ||
      (lv->number != v->number || strcmp(lv->text, v->text) != 0)) {
    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachineMessage(lv, last)) {
      fprintf(stderr, "failed to serialize machine message\n");
      return 1;
    }
    cJSON_AddItemReferenceToArray(updates, last);
  } else {
    free(v);
  }
  return 0;
}

int serializeMachineStatus(cJSON *datum) {
  struct MachineStatus v;
  if (getMachineStatus(&v)) {
    fprintf(stderr, "failed to read machine status!\n");
    return 1;
  }
  cJSON *status_datum = cJSON_CreateObject();
  cJSON *execution_datum = cJSON_CreateString(v.execution);
  cJSON_AddItemToObject(status_datum, "execution", execution_datum);

  cJSON *mode_datum = cJSON_CreateString(v.mode);
  cJSON_AddItemToObject(status_datum, "mode", mode_datum);

  cJSON *estop_datum = cJSON_CreateString(v.estop);
  cJSON_AddItemToObject(status_datum, "estop", estop_datum);

  cJSON *raw_datum = cJSON_CreateObject();

  cJSON *aut_datum = cJSON_CreateNumber(v.raw.aut);
  cJSON_AddItemToObject(raw_datum, "aut", aut_datum);

  cJSON *run_datum = cJSON_CreateNumber(v.raw.run);
  cJSON_AddItemToObject(raw_datum, "run", run_datum);

  cJSON *edit_datum = cJSON_CreateNumber(v.raw.edit);
  cJSON_AddItemToObject(raw_datum, "edit", edit_datum);

  cJSON *motion_datum = cJSON_CreateNumber(v.raw.motion);
  cJSON_AddItemToObject(raw_datum, "motion", motion_datum);

  cJSON *mstb_datum = cJSON_CreateNumber(v.raw.mstb);
  cJSON_AddItemToObject(raw_datum, "mstb", mstb_datum);

  cJSON *emergency_datum = cJSON_CreateNumber(v.raw.emergency);
  cJSON_AddItemToObject(raw_datum, "emergency", emergency_datum);

  cJSON *alarm_datum = cJSON_CreateNumber(v.raw.alarm);
  cJSON_AddItemToObject(raw_datum, "alarm", alarm_datum);

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v.executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(status_datum, "raw", raw_datum);

  cJSON_AddItemToObject(datum, "meta", meta_datum);
  cJSON_AddItemToObject(datum, "status", status_datum);

  return 0;
}

int checkMachineStatus(cJSON *updates) {
  static MachineStatus *lv = NULL;
  static cJSON *last = NULL;

  MachineStatus *v;
  v = malloc(sizeof(MachineStatus));

  if (getMachineStatus(v)) {
    free(v);
    fprintf(stderr, "failed to read machine status\n");
    return 1;
  }

  if (lv == NULL ||
      (lv->raw.alarm != v->raw.alarm || lv->raw.aut != v->raw.aut ||
       lv->raw.edit != v->raw.edit || lv->raw.emergency != v->raw.emergency ||
       lv->raw.hdck != v->raw.hdck || lv->raw.motion != v->raw.motion ||
       lv->raw.mstb != v->raw.mstb || lv->raw.run != v->raw.run)) {
    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachineStatus(last)) {
      fprintf(stderr, "failed to serialize machine status\n");
      return 1;
    }
    cJSON_AddItemReferenceToArray(updates, last);
  } else {
    free(v);
  }

  return 0;
}

int serializeMachineDynamic(MachineDynamic *v, cJSON *datum) {
  cJSON *dynamic_datum = cJSON_CreateObject();
  cJSON *absolute_datum = cJSON_CreateArray();
  cJSON *relative_datum = cJSON_CreateArray();
  cJSON *actual_datum = cJSON_CreateArray();
  cJSON *load_datum = cJSON_CreateArray();

  int i = 0;
  for (i = 0; i < v->dim; i++) {
    cJSON *each_absolute_datum = cJSON_CreateNumber(v->absolute[i]);
    cJSON_AddItemToArray(absolute_datum, each_absolute_datum);
    cJSON *each_relative_datum = cJSON_CreateNumber(v->relative[i]);
    cJSON_AddItemToArray(relative_datum, each_relative_datum);
    cJSON *each_actual_datum = cJSON_CreateNumber(v->actual[i]);
    cJSON_AddItemToArray(actual_datum, each_actual_datum);
    cJSON *each_load_datum = cJSON_CreateNumber(v->load[i]);
    cJSON_AddItemToArray(load_datum, each_load_datum);
  }

  cJSON_AddItemToObject(dynamic_datum, "absolute", absolute_datum);
  cJSON_AddItemToObject(dynamic_datum, "relative", relative_datum);
  cJSON_AddItemToObject(dynamic_datum, "actual", actual_datum);
  cJSON_AddItemToObject(dynamic_datum, "load", load_datum);

  // current program
  cJSON *cprogram_datum = cJSON_CreateNumber(v->cprogram);
  cJSON_AddItemToObject(dynamic_datum, "cprogram", cprogram_datum);

  // main program
  cJSON *mprogram_datum = cJSON_CreateNumber(v->mprogram);
  cJSON_AddItemToObject(dynamic_datum, "mprogram", mprogram_datum);

  // line no
  cJSON *sequence_datum = cJSON_CreateNumber(v->sequence);
  cJSON_AddItemToObject(dynamic_datum, "sequence", sequence_datum);

  // actual feedrate
  cJSON *actf_datum = cJSON_CreateNumber(v->actf);
  cJSON_AddItemToObject(dynamic_datum, "actf", actf_datum);

  // actual spindle speed
  cJSON *acts_datum = cJSON_CreateNumber(v->acts);
  cJSON_AddItemToObject(dynamic_datum, "acts", acts_datum);

  // alarm status
  cJSON *alarm_datum = cJSON_CreateNumber(v->alarm);
  cJSON_AddItemToObject(dynamic_datum, "alarm", alarm_datum);

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(datum, "dynamic", dynamic_datum);
  cJSON_AddItemToObject(datum, "meta", meta_datum);

  return 0;
}

bool cmpMachineDynamic(MachineDynamic *a, MachineDynamic *b) {
  if (a->cprogram != b->cprogram || a->mprogram != b->mprogram ||
      a->sequence != b->sequence || a->actf != b->actf || a->acts != b->acts ||
      a->alarm != b->alarm || a->dim != b->dim) {
    return false;
  }
  for (int i = 0; i < sizeof(a->dim); i++) {
    if (a->absolute[i] != b->absolute[i] || a->relative[i] != b->relative[i] ||
        a->actual[i] != b->actual[i] || a->load[i] != b->load[i]) {
      return false;
    }
  }
  return true;
}

int checkMachineDynamic(cJSON *updates) {
  static MachineDynamic *lv = NULL;
  static cJSON *last = NULL;

  MachineDynamic *v;
  v = malloc(sizeof(MachineDynamic));

  if (getMachineDynamic(v)) {
    free(v);
    fprintf(stderr, "failed to read machine dynamic\n");
    return 1;
  }

  programNum = v->cprogram;

  if (lv == NULL || !cmpMachineDynamic(lv, v)) {
    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachineDynamic(lv, last)) {
      fprintf(stderr, "failed to serialize machine dynamic\n");
      return 1;
    }
    cJSON_AddItemReferenceToArray(updates, last);
  } else {
    free(v);
  }

  return 0;
}

int serializeMachineToolInfo(MachineToolInfo *v, cJSON *datum) {
  cJSON *tool_datum = cJSON_CreateObject();
  cJSON *id_datum = cJSON_CreateNumber(v->id);
  cJSON_AddItemToObject(tool_datum, "number", id_datum);

  cJSON *group_datum = cJSON_CreateNumber(v->group);
  cJSON_AddItemToObject(tool_datum, "group", group_datum);

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(datum, "meta", meta_datum);
  cJSON_AddItemToObject(datum, "tool", tool_datum);

  return 0;
}

int checkMachineToolInfo(cJSON *updates) {
  static MachineToolInfo *lv = NULL;
  static cJSON *last = NULL;

  MachineToolInfo *v;
  v = malloc(sizeof(MachineToolInfo));

  if (getMachineToolInfo(v)) {
    fprintf(stderr, "failed to read machine tool info\n");
    return 1;
  }

  if (lv == NULL || (lv->id != v->id || lv->group != v->group)) {
    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachineToolInfo(lv, last)) {
      fprintf(stderr, "failed to read machine tool info\n");
      return 1;
    }

    cJSON_AddItemReferenceToArray(updates, last);
  } else {
    free(v);
  }
  return 0;
}

int serializeMachineProgram(MachineProgram *v, cJSON *datum) {
  cJSON *program_datum = cJSON_CreateObject();
  cJSON *number_datum = cJSON_CreateNumber(v->number);
  cJSON_AddItemToObject(program_datum, "number", number_datum);

  cJSON *header_datum = cJSON_CreateString(v->header);
  cJSON_AddItemToObject(program_datum, "header", header_datum);

  cJSON *meta_datum = cJSON_CreateObject();
  cJSON *t_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta_datum, "t", t_datum);

  cJSON_AddItemToObject(datum, "meta", meta_datum);
  cJSON_AddItemToObject(datum, "program", program_datum);

  return 0;
}

int checkMachineProgram(cJSON *updates) {
  static MachineProgram *lv = NULL;
  static cJSON *last = NULL;
  static short lastProgramNum = 0;

  if (lastProgramNum != programNum) {
    MachineProgram *v;
    v = malloc(sizeof(MachineProgram));

    if (getMachineProgram(v, programNum)) {
      free(v);
      fprintf(stderr, "failed to check machine program: %d\n", programNum);
      return 1;
    }

    if (lv != NULL) {
      free(lv);
      cJSON_Delete(last);
    }

    lv = v;
    last = cJSON_CreateObject();

    if (serializeMachineProgram(v, last)) {
      fprintf(stderr, "failed to serialize machine program: %d\n", programNum);
      return 1;
    }

    cJSON_AddItemReferenceToArray(updates, last);
    lastProgramNum = programNum;
  }
  return 0;
}

int setupEnv() {
  char *pTmp;
  if ((pTmp = getenv("DEVICE_IP")) != NULL) {
    sprintf(deviceIP, "%s", pTmp);
  }

  if ((pTmp = getenv("DEVICE_PORT")) != NULL) {
    char dp[10];
    char *ptr;
    strncpy(dp, pTmp, 10);
    memset(dp, '\0', sizeof(dp));
    devicePort = strtol(dp, &ptr, 10);
    if (devicePort <= 0 || devicePort > 65535) {
      fprintf(stderr, "invalid DEVICE_PORT: %s\n", pTmp);
      return 1;
    }
  }

  if ((pTmp = getenv("MACHINE_NAME")) != NULL) {
    sprintf(machineName, "%s", pTmp);
  }

  return 0;
}

void intHandler(int sig) { runningCondition = 1; }

int loopSetup(cJSON **ptr) {
  atexit(cleanup);

  *ptr = cJSON_CreateArray();

  if (checkMachineInfo(*ptr)) {
    fprintf(stderr, "failed to read machine info!\n");
    return 1;
  }

  return 0;
}

int loopTick(cJSON *updates) {
  if (checkMachinePartCount(updates) || checkMachineStatus(updates) ||
      checkMachineToolInfo(updates) || checkMachineDynamic(updates) ||
      checkMachineMessage(updates) || checkMachineProgram(updates)) {
    fprintf(stderr, "failed to check machine values\n");
    return 1;
  }
  // jsonDebug("values: ", updates);
  return 0;
}

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage,
                      void *opaque) {
  if (rkmessage->err)
    fprintf(stderr, "%% Message delivery failed: %s\n",
            rd_kafka_err2str(rkmessage->err));
  else
    fprintf(stderr,
            "%% Message delivered (%zd bytes, "
            "partition %" PRId32 ")\n",
            rkmessage->len, rkmessage->partition);

  /* The rkmessage is destroyed automatically by librdkafka */
}

int main(int argc, char **argv) {
  rd_kafka_t *rk;        /* Producer instance handle */
  rd_kafka_conf_t *conf; /* Temporary configuration object */
  char errstr[512];      /* librdkafka API error reporting buffer */
  char buf[512];         /* Message value temporary buffer */
  const char *brokers;   /* Argument: broker list */
  const char *topic;     /* Argument: topic to produce to */

  /*
  if (argc != 3) {
    fprintf(stderr, "%% Usage: %s <broker> <topic>\n", argv[0]);
    return 1;
  }
  */

  brokers = "localhost:9092";
  topic = "input";

  conf = rd_kafka_conf_new();

  if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr,
                        sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%s\n", errstr);
    return 1;
  }
  rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

  rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));

  if (!rk) {
    fprintf(stderr, "%% Failed to create new producer: %s\n", errstr);
    exit(EXIT_FAILURE);
    return 1;
  }

  if (setupEnv()) {
    fprintf(stderr, "failed to configure environment variables\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  if (setupConnection(deviceIP, devicePort)) {
    fprintf(stderr, "failed to setup machine connection!\n");
    return 1;
  };

  cJSON *updates = NULL;

  signal(SIGINT, intHandler);

  if (loopSetup(&updates)) {
    fprintf(stderr, "failed to setup loop\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  double tt;
  struct timespec t0, t1;

  double minimum_interval = 0.5;

  do {
    rd_kafka_resp_err_t err;

    clock_gettime(CLOCK_REALTIME, &t0);

    if (loopTick(updates)) {
      fprintf(stderr, "loop check failed\n");
      exit(EXIT_FAILURE);
      return 1;
    }

    char *serialized = cJSON_PrintUnformatted(updates);

    size_t vlen = strlen(serialized);
    size_t klen = strlen(deviceID);

    if (vlen == 0) {
      rd_kafka_poll(rk, 0 /*non-blocking */);
      continue;
    }

    err = rd_kafka_producev(
        rk,
        RD_KAFKA_V_TOPIC(topic),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_VALUE(serialized, vlen),
        RD_KAFKA_V_KEY(deviceID, klen),
        RD_KAFKA_V_OPAQUE(NULL),
        RD_KAFKA_V_END);

    if (err) {
      fprintf(stderr, "%% Failed to produce to topic %s: %s\n", topic,
              rd_kafka_err2str(err));

      if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
        fprintf(stderr, "error: queue full\n");
      }
    }

    rd_kafka_poll(rk, 0 /*non-blocking*/);

    clock_gettime(CLOCK_REALTIME, &t1);
    tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;

    if (tt < minimum_interval) {
      usleep((long)((minimum_interval - tt) * 1e6));
    }

    free(serialized);
    cJSON_Delete(updates);
    updates = cJSON_CreateArray();

    // sleep(mScanDelay);
  } while (!runningCondition);

  fprintf(stderr, "%% Flushing final messages..\n");
  rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);

  /* If the output queue is still not empty there is an issue
   * with producing messages to the clusters. */
  if (rd_kafka_outq_len(rk) > 0)
    fprintf(stderr, "%% %d message(s) were not delivered\n",
            rd_kafka_outq_len(rk));

  /* Destroy the producer instance */
  rd_kafka_destroy(rk);

  cJSON_Delete(updates);
  exit(EXIT_SUCCESS);
}
