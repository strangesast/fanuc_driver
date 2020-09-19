#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include "./external/fwlib/fwlib32.h"
#include "./external/cJSON/cJSON.h"
#include "./external/librdkafka/src/rdkafka.h"
#define MAXPATH 1024
#define PART_COUNT_PARAMETER 6711

char *AXIS_UNITH[] = {"mm", "inch", "degree", "mm/minute", "inch/minute", "rpm", "mm/round", "inch/round", "%", "Ampere", "Second"};
char *MACHINE_EXECUTION[] = {"ACTIVE", "INTERRUPTED", "STOPPED", "READY"};
char *MACHINE_MODE[] = {"MANUAL", "MANUAL_DATA_INPUT", "AUTOMATIC"};
char *MACHINE_ESTOP[] = {"TRIGGERED", "ARMED"};

short unsigned int fLibHandle;

char mDeviceIP[MAXPATH] = "";
int mDevicePort;

static volatile int runningCondition = 0;

bool jsonEqual(cJSON *a, cJSON *b)
{
  return strcmp(cJSON_PrintUnformatted(a), cJSON_PrintUnformatted(b)) == 0;
}

// get machine properties (unique id, characteristics, software versions)
int readMachineInfo(cJSON *datum, double axesDivisors[MAX_AXIS])
{
  short ret;
  short etherType;
  short etherDevice;
  short pathNumber = 0;
  short len = MAX_AXIS;
  short axisCount = MAX_AXIS;
  short count;
  short types[] = {1 /* actual position */};
  short inprec[MAX_AXIS];
  short outprec[MAX_AXIS];
  const int num = 1;
  char buf[36];
  unsigned long cncIDs[4];
  ODBSYS sysinfo;
  ODBAXDT axisData[MAX_AXIS * num];
  ODBAXISNAME axes[MAX_AXIS];

  ret = cnc_sysinfo(fLibHandle, &sysinfo);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc sysinfo!\n");
    return 1;
  }

  ret = cnc_rdcncid(fLibHandle, cncIDs);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc id!\n");
    return 1;
  }

  ret = cnc_rdetherinfo(fLibHandle, &etherType, &etherDevice);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc ether info!\n");
    return 1;
  }

  // machine id
  sprintf(buf, "%08lx-%08lx-%08lx-%08lx", cncIDs[0] & 0xffffffff, cncIDs[1] & 0xffffffff, cncIDs[2] & 0xffffffff, cncIDs[3] & 0xffffffff);
  cJSON *id_datum = cJSON_CreateString(buf);
  if (id_datum == NULL)
  {
    fprintf(stderr, "failed to allocate id\n");
    return 1;
  }
  cJSON_AddItemToObject(datum, "id", id_datum);

  // max_axis
  cJSON *max_axis_datum = cJSON_CreateNumber(sysinfo.max_axis);
  if (max_axis_datum == NULL)
  {
    fprintf(stderr, "failed to allocate max_axis\n");
    return 1;
  }
  cJSON_AddItemToObject(datum, "max_axis", max_axis_datum);

  // addinfo
  cJSON *addinfo_datum = cJSON_CreateNumber(sysinfo.addinfo);
  if (addinfo_datum == NULL)
  {
    fprintf(stderr, "failed to allocate addinfo\n");
    return 1;
  }
  cJSON_AddItemToObject(datum, "addinfo", addinfo_datum);

  // cnc_type
  sprintf(buf, "%.2s", sysinfo.cnc_type);
  cJSON *cnc_type_datum = cJSON_CreateString(buf);
  if (cnc_type_datum == NULL)
  {
    fprintf(stderr, "failed to allocate cnc_type\n");
  }
  cJSON_AddItemToObject(datum, "cnc_type", cnc_type_datum);

  // mt_type
  sprintf(buf, "%.2s", sysinfo.mt_type);
  cJSON *mt_type_datum = cJSON_CreateString(buf);
  if (mt_type_datum == NULL)
  {
    fprintf(stderr, "failed to allocate mt_type\n");
  }
  cJSON_AddItemToObject(datum, "mt_type", mt_type_datum);

  // series_datum
  sprintf(buf, "%.4s", sysinfo.series);
  cJSON *series_datum = cJSON_CreateString(buf);
  if (series_datum == NULL)
  {
    fprintf(stderr, "failed to allocate series\n");
  }
  cJSON_AddItemToObject(datum, "series", series_datum);

  sprintf(buf, "%.4s", sysinfo.version);
  cJSON *version_datum = cJSON_CreateString(buf);
  if (version_datum == NULL)
  {
    fprintf(stderr, "failed to allocate version\n");
  }
  cJSON_AddItemToObject(datum, "version", version_datum);

  sprintf(buf, "%.2s", sysinfo.axes);
  cJSON *axes_count_datum = cJSON_CreateString(buf);
  if (axes_count_datum == NULL)
  {
    fprintf(stderr, "failed to allocate axes\n");
  }
  cJSON_AddItemToObject(datum, "axes_count", axes_count_datum);

  ret = cnc_rdaxisdata(fLibHandle, 1 /* Position Value */, (short *)types, num, &len, axisData);
  bool hasAxisData = ret == EW_OK;
  if (!hasAxisData)
  {
    fprintf(stderr, "cnc_rdaxisdata returned %d for path %d\n", ret, pathNumber);
  }

  ret = cnc_getfigure(fLibHandle, 0, &count, inprec, outprec);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to get axis scale: %d\n", ret);
    return 1;
  }

  cJSON *axesDatum = cJSON_CreateArray();

  ret = cnc_rdaxisname(fLibHandle, &axisCount, axes);
  if (ret == EW_OK)
  {
    int i = 0;
    for (i = 0; i < axisCount; i++)
    {
      cJSON *axisDatum = cJSON_CreateObject();

      double divisor = pow((long double)10.0, (long double)inprec[i]);
      axesDivisors[i] = divisor;

      sprintf(buf, "%c", axes[i].name);
      cJSON *id_datum = cJSON_CreateString(buf);
      cJSON_AddItemToObject(axisDatum, "id", id_datum);

      cJSON *index_datum = cJSON_CreateNumber(i);
      cJSON_AddItemToObject(axisDatum, "index", index_datum);

      sprintf(buf, "%c", axes[i].suff);
      cJSON *suffix_datum = cJSON_CreateString(buf);
      cJSON_AddItemToObject(axisDatum, "suffix", suffix_datum);

      cJSON *divisor_datum = cJSON_CreateNumber(divisor);
      cJSON_AddItemToObject(axisDatum, "divisor", divisor_datum);

      if (hasAxisData)
      {
        char name[5];
        memset(name, '\0', sizeof(name));
        strncpy(name, axisData[i].name, 4);

        cJSON *name_datum = cJSON_CreateString(name);
        cJSON_AddItemToObject(axisDatum, "name", name_datum);

        cJSON *flag_datum = cJSON_CreateNumber(axisData[i].flag);
        cJSON_AddItemToObject(axisDatum, "flag", flag_datum);

        short unit = axisData[i].unit;
        cJSON *unit_datum = cJSON_CreateNumber(unit);
        cJSON_AddItemToObject(axisDatum, "unit", unit_datum);

        char *unith = AXIS_UNITH[unit];
        cJSON *unith_datum = cJSON_CreateString(unith);
        cJSON_AddItemToObject(axisDatum, "unith", unith_datum);

        cJSON *decimal_datum = cJSON_CreateNumber(axisData[i].dec);
        cJSON_AddItemToObject(axisDatum, "decimal", decimal_datum);
      }
      cJSON_AddItemToArray(axesDatum, axisDatum);
    }
  }
  else
  {
    fprintf(stderr, "Failed to get axis names: %d\n", ret);
    return 1;
  }

  cJSON_AddItemToObject(datum, "axes", axesDatum);

  return 0;
}

int checkMachineInfo(cJSON *updatesArray, double axesDivisors[MAX_AXIS])
{
  static cJSON *last = NULL; // last value pointer, freed on process exit
  cJSON *next = cJSON_CreateObject();
  if (readMachineInfo(next, axesDivisors))
  {
    fprintf(stderr, "failed to read machine info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  if (last == NULL)
  {
    // pass
  }
  else if (!jsonEqual(last, next))
  {
    cJSON_Delete(last);
  }
  else
  {
    cJSON_Delete(next);
    return 0;
  }
  cJSON_AddItemReferenceToArray(updatesArray, next);
  last = next;

  return 0;
}

struct MachineMessage
{
  short number;
  char text[256];
};

int getMachineMessage(bool *changed, struct MachineMessage *next)
{
  static struct MachineMessage last = {};
  OPMSG message;
  short ret = cnc_rdopmsg(fLibHandle, 0, 6 + 256, &message);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to read operator message: %d\n", ret);
    return 1;
  }
  *changed = (last.number != message.datano) || !strcmp(last.text, message.data);
  last.number = message.datano;
  size_t l = sizeof(last.text);
  strncpy(last.text, message.data, l);
  last.text[l - 1] = '\0';
  *next = last;
  return 0;
}

int readMachineMessage(cJSON *datum)
{
  bool *changed = false;
  struct MachineMessage *v;
  if (getMachineMessage(changed, v))
  {
    fprintf(stderr, "failed to read machine message!\n");
    return 1;
  }
  if (changed) {
    if (v->number != -1) {
      cJSON *message_datum = cJSON_CreateObject();
      cJSON *num_datum = cJSON_CreateNumber(v->datano);
      cJSON_AddItemToObject(message_datum, "num", num_datum);
      cJSON *text_datum = cJSON_CreateString(v->text);
      cJSON_AddItemToObject(message_datum, "text", text_datum);
      cJSON_AddItemToObject(datum, "message", message_datum);
    } else {
      cJSON_AddItemToObject(datum, "message", message_datum);
    }
  }

  return 0;
}

struct MachineStatus
{
  short alarm;
  short aut;
  short edit;
  short emergency;
  short hdck;
  short motion;
  short mstb;
  short run;
}

int
getMachineStatus(bool *changed, struct MachineStatus *next)
{
  static struct MachineStatus last = {};
  short ret;
  ODBST status;

  ret = cnc_statinfo(fLibHandle, &status);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Cannot cnc_statinfo: %d", ret);
    return 1;
  }
  /*
     ODBALM alarm;
     ret = cnc_alarm(mFlibHndl, &alarm);
     if (ret != EW_OK)
     {
     fprintf(stderr, "failed to get alarm data!\n");
     exit(EXIT_FAILURE);
     return 1;
     }
     printf("ALARM %d\n", alarm.data);

     ALMINFO alarminfo;
     ret = cnc_rdalminfo(mFlibHndl, 1, short alm_type, short length, &alarminfo)
  */
  *changed = (last.alarm != status.alarm) || (last.aut != status.aut) || (last.edit != status.edit) || (last.emergency != status.emergency) || (last.hdck != status.hdck) || (last.motion != status.motion) || (last.mstb != status.mstb) || (last.run != status.run);
  last.alarm = status.alarm;
  last.aut = status.aut;
  last.edit = status.edit;
  last.emergency = status.emergency;
  last.hdck = status.hdck;
  last.motion = status.motion;
  last.mstb = status.mstb;
  last.run = status.run;
  *next = last;
  return 0;
}

int readMachineStatus(cJSON *datum)
{
  bool *changed = false;
  struct MachineStatus *v;
  if (getMachineStatus(changed, v))
  {
    fprintf(stderr, "failed to read machine status!\n");
    return 1;
  }
  if (changed)
  {
    char *execution = MACHINE_EXECUTION[(v->run == 3 || v->run == 4) ? 0 : (v->run == 2 || v->motion == 2 || v->mstb != 0) ? 1 : v->run == 0 ? 2 : 3];
    cJSON *execution_datum = cJSON_CreateString(execution);
    cJSON_AddItemToObject(datum, "execution", execution_datum);

    char *mode = MACHINE_MODE[(v->aut == 5 || v->aut == 6) ? 0 : (v->aut == 0 || v->aut == 3) ? 1 : 2];
    cJSON *mode_datum = cJSON_CreateString(mode);
    cJSON_AddItemToObject(datum, "mode", mode_datum);

    char *estop = MACHINE_ESTOP[v->emergency == 1 ? 0 : 1];
    cJSON *estop_datum = cJSON_CreateString(estop);
    cJSON_AddItemToObject(datum, "estop", estop_datum);

    cJSON *raw_datum = cJSON_CreateObject();

    cJSON *aut_datum = cJSON_CreateNumber(v->aut);
    cJSON_AddItemToObject(raw_datum, "aut", aut_datum);

    cJSON *run_datum = cJSON_CreateNumber(v->run);
    cJSON_AddItemToObject(raw_datum, "run", run_datum);

    cJSON *edit_datum = cJSON_CreateNumber(v->edit);
    cJSON_AddItemToObject(raw_datum, "edit", edit_datum);

    cJSON *motion_datum = cJSON_CreateNumber(v->motion);
    cJSON_AddItemToObject(raw_datum, "motion", motion_datum);

    cJSON *mstb_datum = cJSON_CreateNumber(v->mstb);
    cJSON_AddItemToObject(raw_datum, "mstb", mstb_datum);

    cJSON *emergency_datum = cJSON_CreateNumber(v->emergency);
    cJSON_AddItemToObject(raw_datum, "emergency", emergency_datum);

    cJSON *alarm_datum = cJSON_CreateNumber(v->alarm);
    cJSON_AddItemToObject(raw_datum, "alarm", alarm_datum);

    cJSON_AddItemToObject(datum, "raw", raw_datum);
  }

  return 0;
}

struct MachinePartCount
{
  long count;
  long minutes;
  long milliseconds;
};

int getMachinePartCount(bool *changed, struct MachinePartCount *next)
{
  static struct MachinePartCount last = {};
  short ret;
  short timeType = 3; // 0->Power on time, 1->Operating time, 2->Cutting time, 3->Cycle time, 4->Free purpose,
  IODBTIME time;
  IODBPSD param;

  ret = cnc_rdparam(fLibHandle, PART_COUNT_PARAMETER, -1, 8, &param);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to read part parameter!\n");
    return 1;
  }

  ret = cnc_rdtimer(fLibHandle, timeType, &time);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get time for type %d!\n", timeType);
    return 1;
  }

  *changed = (last.count != param.u.ldata) || (last.minutes != time.minute) || (last.milliseconds != time.msec);
  last.count = param.u.ldata;
  last.minutes = time.minute;
  last.milliseconds = time.msec;
  *next = last;

  return 0;
}

int readMachinePartCount(cJSON *datum)
{
  bool *changed = false;
  struct MachinePartCount *v;
  if (getMachinePartCount(changed, v))
  {
    fprintf(stderr, "failed to read machine part count data!\n");
    return 1;
  }
  if (changed)
  {
    // count
    cJSON *count_datum = cJSON_CreateNumber(v->count);
    cJSON_AddItemToObject(datum, "count", count_datum);

    // cycle time
    cJSON *cycle_time_datum = cJSON_CreateObject();

    // milliseconds
    long milliseconds = v->milliseconds + v->minutes * 60 * 1000;
    cJSON *milliseconds_datum = cJSON_CreateNumber(milliseconds);
    cJSON_AddItemToObject(cycle_time_datum, "milliseconds", milliseconds_datum);

    cJSON *raw_cycle_time_datum = cJSON_CreateObject();
    // raw minutes
    cJSON *raw_minutes_datum = cJSON_CreateNumber(v->minutes);
    cJSON_AddItemToObject(raw_cycle_time_datum, "minutes", raw_minutes_datum);

    // raw milliseconds
    cJSON *raw_milliseconds_datum = cJSON_CreateNumber(v->milliseconds);
    cJSON_AddItemToObject(raw_cycle_time_datum, "milliseconds", raw_milliseconds_datum);

    cJSON_AddItemToObject(cycle_time_datum, "raw", raw_cycle_time_datum);
    cJSON_AddItemToObject(datum, "last_cycle_time", cycle_time_datum);
  }
  return 0;
}

int readMachineDynamic(cJSON *datum, double divisors[MAX_AXIS], long *programNum)
{
  short ret;
  ODBDY2 dyn;
  ODBSVLOAD axLoad[MAX_AXIS];
  short num = MAX_AXIS;

  // alarm status, program number, sequence number, actual feed rate,
  // actual spindle speed, absolute/machine/relative position, distance to go
  ret = cnc_rddynamic2(fLibHandle, -1, sizeof(dyn), &dyn);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc dyn data!\n");
    return 1;
  }

  // servo load
  ret = cnc_rdsvmeter(fLibHandle, &num, axLoad);
  if (ret != EW_OK)
  {
    fprintf(stderr, "cnc_rdsvmeter failed: %d\n", ret);
    return 1;
  }

  cJSON *absolute_datum = cJSON_CreateArray();
  cJSON *relative_datum = cJSON_CreateArray();
  cJSON *actual_datum = cJSON_CreateArray();
  cJSON *load_datum = cJSON_CreateArray();

  int i = 0;
  for (i = 0; i < num; i++)
  {
    double absolute = dyn.pos.faxis.absolute[i] / divisors[i];
    cJSON *each_absolute_datum = cJSON_CreateNumber(absolute);
    cJSON_AddItemToArray(absolute_datum, each_absolute_datum);

    double relative = dyn.pos.faxis.relative[i] / divisors[i];
    cJSON *each_relative_datum = cJSON_CreateNumber(relative);
    cJSON_AddItemToArray(relative_datum, each_relative_datum);

    double actual = dyn.pos.faxis.machine[i] / divisors[i];
    cJSON *each_actual_datum = cJSON_CreateNumber(actual);
    cJSON_AddItemToArray(actual_datum, each_actual_datum);

    double load = axLoad[i].svload.data / pow(10.0, axLoad[i].svload.dec);
    cJSON *each_load_datum = cJSON_CreateNumber(load);
    cJSON_AddItemToArray(load_datum, each_load_datum);
  }

  cJSON_AddItemToObject(datum, "absolute", absolute_datum);
  cJSON_AddItemToObject(datum, "actual", actual_datum);
  cJSON_AddItemToObject(datum, "load", load_datum);

  *programNum = dyn.prgnum;
  // current program
  cJSON *cprogram_datum = cJSON_CreateNumber(dyn.prgnum);
  cJSON_AddItemToObject(datum, "cprogram", cprogram_datum);

  // main program
  cJSON *mprogram_datum = cJSON_CreateNumber(dyn.prgmnum);
  cJSON_AddItemToObject(datum, "mprogram", mprogram_datum);

  // line no
  cJSON *sequence_datum = cJSON_CreateNumber(dyn.seqnum);
  cJSON_AddItemToObject(datum, "sequence", sequence_datum);

  // actual feedrate
  cJSON *actf_datum = cJSON_CreateNumber(dyn.actf);
  cJSON_AddItemToObject(datum, "actf", actf_datum);

  // actual spindle speed
  cJSON *acts_datum = cJSON_CreateNumber(dyn.acts);
  cJSON_AddItemToObject(datum, "acts", acts_datum);

  // alarm status
  cJSON *alarm_datum = cJSON_CreateNumber(dyn.alarm);
  cJSON_AddItemToObject(datum, "alarm", alarm_datum);

  return 0;
}

int readMachineToolInfo(cJSON *datum)
{
  short ret;
  static bool toolManagementEnabled = true;
  static bool useModalToolData = false;
  if (toolManagementEnabled)
  {
    // ODBTLIFE4 toolId2;
    // short ret = cnc_toolnum(aFlibhndl, 0, 0, &toolId2);

    ODBTLIFE3 toolId;
    ret = cnc_rdntool(fLibHandle, 0, &toolId);
    if (ret == EW_OK && toolId.data != 0)
    {
      cJSON *id_datum = cJSON_CreateNumber(toolId.data);
      cJSON_AddItemToObject(datum, "id", id_datum);

      cJSON *group_datum = cJSON_CreateNumber(toolId.datano);
      cJSON_AddItemToObject(datum, "group", group_datum);

      cJSON *method_datum = cJSON_CreateString("mgmt");
      cJSON_AddItemToObject(datum, "method", method_datum);
    }
    else
    {
      fprintf(stderr, "Cannot use cnc_rdntool: %d. Trying modal method\n", ret);
      toolManagementEnabled = false;
      useModalToolData = true;
    }
  }

  if (useModalToolData)
  {
    ODBMDL command;
    short ret = cnc_modal(fLibHandle, 108, 1, &command);
    if (ret == EW_OK)
    {
      long toolId = command.modal.aux.aux_data;
      cJSON *id_datum = cJSON_CreateNumber(toolId);
      cJSON_AddItemToObject(datum, "id", id_datum);

      cJSON *method_datum = cJSON_CreateString("modal");
      cJSON_AddItemToObject(datum, "method", method_datum);
    }
    else
    {
      fprintf(stderr, "cnc_modal failed for T: %d\n", ret);
      useModalToolData = false;
    }
  }

  return 0;
}

int readMachineProgram(short programNum, cJSON *datum)
{
  short ret;
  // max length of "header", could read entire file
  char program[2048];

  cJSON *number_datum = cJSON_CreateNumber(programNum);
  cJSON_AddItemToObject(datum, "number", number_datum);

  ret = cnc_upstart(fLibHandle, programNum);
  if (ret == EW_OK)
  {
    long len = sizeof(program) - 1; // One for the \0 terminator
    do
    {
      ret = cnc_upload3(fLibHandle, &len, program);
      if (ret == EW_OK)
      {
        program[len] = '\0';
        int lineCount = 0;
        // iterate chars until non-comment ("(") character reached.
        for (char *cp = program; *cp != '\0' && lineCount < 40; ++cp)
        {
          if (*cp == '\n')
          {
            char f = *(cp + 1);
            if (lineCount > 0 && f != '(')
            {
              *cp = '\0';
              break;
            }
            *cp = ' ';
            lineCount++;
          }
        }
      }
    } while (ret == EW_BUFFER);
  }
  else if (ret == EW_DATA)
  {
    sprintf(program, "(ERR: program %d not found on cnc)", programNum);
    cJSON *header_datum = cJSON_CreateString(program);
    cJSON_AddItemToObject(datum, "header", header_datum);

    fprintf(stderr, "failed to initiate program read: %d\n", ret);
    return 0;
  }
  ret = cnc_upend3(fLibHandle);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to close header read!\n");
    return 1;
  }

  cJSON *header_datum = cJSON_CreateString(program);
  cJSON_AddItemToObject(datum, "header", header_datum);

  return 0;
}

void cleanup()
{
  printf("cleaning up...\n");
  short ret = cnc_freelibhndl(fLibHandle);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to free library handle!\n");
  }
}

void setupEnv()
{
  char *pTmp;
  if ((pTmp = getenv("DEVICE_IP")) != NULL)
  {
    strncpy(mDeviceIP, pTmp, MAXPATH - 1);
  }
  else
  {
    strcpy(mDeviceIP, "127.0.0.1");
  }

  mDevicePort = 8193;
}

void setupMachineConnection(char *deviceIP, char *devicePort)
{
  short ret;

  printf("using %s:%d\n", mDeviceIP, mDevicePort);

  // mandatory logging.
  ret = cnc_startupprocess(0, "focas.log");
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to create log file!\n");
    return 1;
  }

  // library handle.  needs to be closed when finished.
  ret = cnc_allclibhndl3(deviceIP, devicePort, 10 /* timeout (seconds) */, &fLibHandle);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to connect to cnc!\n");
    return 1;
  }
  atexit(cleanup);

  // set to first path, may be default / not necessary
  ret = cnc_setpath(fLibHandle, 0);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get set path!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
}

void intHandler(int dummy)
{
  runningCondition = 1;
}

int main(int argc, char **argv)
{
  short ret;

  long programNum = 0;
  long lastProgramNum = programNum;
  double divisors[MAX_AXIS];

  rd_kafka_t *rk;        /* Producer instance handle */
  rd_kafka_conf_t *conf; /* Temporary configuration object */
  char errstr[512];      /* librdkafka API error reporting buffer */
  char buf[512];         /* Message value temporary buffer */
  const char *brokers;   /* Argument: broker list */
  const char *topic;     /* Argument: topic to produce to */

  /*
   * Argument validation
   */
  if (argc != 3)
  {
    fprintf(stderr, "%% Usage: %s <broker> <topic>\n", argv[0]);
    return 1;
  }

  brokers = argv[1];
  topic = argv[2];

  /*
   * Create Kafka client configuration place-holder
   */
  conf = rd_kafka_conf_new();

  /* Set bootstrap broker(s) as a comma-separated list of
   * host or host:port (default port 9092).
   * librdkafka will use the bootstrap brokers to acquire the full
   * set of brokers from the cluster. */
  if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
                        errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
  {
    fprintf(stderr, "%s\n", errstr);
    return 1;
  }

  setupEnv();
  setupMachineConnection(mDeviceIP, mDevicePort);

  signal(SIGINT, intHandler);

  // get values, add to array
  // loop:
  //   if changed, add to array

  cJSON *updates = cJSON_CreateArray();

  cJSON *machineInfo = cJSON_CreateObject();
  if (readMachineInfo(machineInfo, divisors))
  {
    fprintf(stderr, "failed to read machine info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  cJSON_AddItemToArray(updates, machineInfo);

  cJSON *partCountDatum = cJSON_CreateObject();
  cJSON *machineStatus = cJSON_CreateObject();
  cJSON *machineToolInfo = cJSON_CreateObject();
  cJSON *machineDynamic = NULL;

  cJSON *dt;
  do
  {
    dt = cJSON_CreateObject();
    if (readMachinePartCount(dt))
    {
      fprintf(stderr, "failed to read machine part count data!\n");
      exit(EXIT_FAILURE);
      return 1;
    }
    if (!jsonEqual(dt, partCountDatum))
    {
      cJSON_Delete(partCountDatum);
      cJSON_AddItemReferenceToArray(updates, dt);
      partCountDatum = dt;
    }
    else
    {
      cJSON_Delete(dt);
    }

    dt = cJSON_CreateObject();
    if (readMachineStatus(dt))
    {
      fprintf(stderr, "failed to read machine status!\n");
      exit(EXIT_FAILURE);
      return 1;
    }
    if (!jsonEqual(dt, machineStatus))
    {
      cJSON_Delete(machineStatus);
      cJSON_AddItemReferenceToArray(updates, dt);
      machineStatus = dt;
    }
    else
    {
      cJSON_Delete(dt);
    }

    dt = cJSON_CreateObject();
    if (readMachineToolInfo(dt))
    {
      fprintf(stderr, "failed to get tool info!\n");
      exit(EXIT_FAILURE);
      return 1;
    }
    if (!jsonEqual(dt, machineToolInfo))
    {
      cJSON_Delete(machineToolInfo);
      cJSON_AddItemReferenceToArray(updates, dt);
      machineToolInfo = dt;
    }
    else
    {
      cJSON_Delete(dt);
    }

    machineDynamic = cJSON_CreateObject();
    if (readMachineDynamic(machineDynamic, divisors, &programNum))
    {
      fprintf(stderr, "failed to read dynamic info!\n");
      exit(EXIT_FAILURE);
      return 1;
    }
    cJSON_AddItemToArray(updates, machineDynamic);

    if (programNum != lastProgramNum)
    {
      cJSON *machineProgram = cJSON_CreateObject();
      if (readMachineProgram(programNum, machineProgram))
      {
        fprintf(stderr, "failed to read program header!\n");
        exit(EXIT_FAILURE);
        return 1;
      }
      lastProgramNum = programNum;
      cJSON_AddItemToArray(updates, machineProgram);
    }

    char *string = cJSON_PrintUnformatted(updates);
    if (string == NULL)
    {
      fprintf(stderr, "Failed to print monitor.\n");
    }
    printf("%s\n\n", string);

    cJSON_Delete(updates);
    updates = cJSON_CreateArray();

  } while (!runningCondition);

  // sleep(mScanDelay);
}
