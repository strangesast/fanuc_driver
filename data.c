#include "./data.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "./external/fwlib/fwlib32.h"
#define PART_COUNT_PARAMETER 6711
#define BILLION 1000000000.0

char *AXIS_UNITH[] = {"mm",          "inch",   "degree",   "mm/minute",
                      "inch/minute", "rpm",    "mm/round", "inch/round",
                      "%",           "Ampere", "Second"};
char *MACHINE_EXECUTION[] = {"ACTIVE", "INTERRUPTED", "STOPPED", "READY"};
char *MACHINE_MODE[] = {"MANUAL", "MANUAL_DATA_INPUT", "AUTOMATIC"};
char *MACHINE_ESTOP[] = {"TRIGGERED", "ARMED"};

short axisCount = MAX_AXIS;
short unsigned int fLibHandle;
double divisors[MAX_AXIS];

// get machine properties (unique id, characteristics, software versions)
int getMachineInfo(MachineInfo *v) {
  short ret;
  short etherType;
  short etherDevice;
  short pathNumber = 0;
  short len = MAX_AXIS;
  short count;
  short types[] = {1 /* actual position */};
  short inprec[MAX_AXIS];
  short outprec[MAX_AXIS];
  const int num = 1;
  unsigned long cncIDs[4];
  ODBSYS sysinfo;
  ODBAXDT axisData[MAX_AXIS * num];
  ODBAXISNAME axes[MAX_AXIS];

  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  ret = cnc_sysinfo(fLibHandle, &sysinfo);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get cnc sysinfo!\n");
    return 1;
  }

  ret = cnc_rdcncid(fLibHandle, cncIDs);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get cnc id!\n");
    return 1;
  }

  ret = cnc_rdetherinfo(fLibHandle, &etherType, &etherDevice);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get cnc ether info!\n");
    return 1;
  }
  // machine id
  sprintf(v->id, "%08x-%08x-%08x-%08x", (int32_t)cncIDs[0], (int32_t)cncIDs[1],
          (int32_t)cncIDs[2], (int32_t)cncIDs[3]);
  v->max_axis = sysinfo.max_axis;
  v->addinfo = sysinfo.addinfo;
  sprintf(v->cnc_type, "%.2s", sysinfo.cnc_type);
  sprintf(v->mt_type, "%.2s", sysinfo.mt_type);
  sprintf(v->series, "%.4s", sysinfo.series);
  sprintf(v->version, "%.4s", sysinfo.version);
  sprintf(v->axes_count_chk, "%.2s", sysinfo.axes);
  v->etherType = etherType;
  v->etherDevice = etherDevice;

  ret = cnc_rdaxisdata(fLibHandle, 1 /* Position Value */, (short *)types, num,
                       &len, axisData);
  bool hasAxisData = ret == EW_OK;
  if (!hasAxisData) {
    fprintf(stderr, "cnc_rdaxisdata returned %d for path %d\n", ret,
            pathNumber);
  }

  ret = cnc_getfigure(fLibHandle, 0, &count, inprec, outprec);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get axis scale: %d\n", ret);
    return 1;
  }

  ret = cnc_rdaxisname(fLibHandle, &axisCount, axes);
  if (ret == EW_OK) {
    v->axes_count = axisCount;
    for (int i = 0; i < axisCount; i++) {
      double divisor = pow((long double)10.0, (long double)inprec[i]);
      divisors[i] = divisor;

      sprintf(v->axes[i].id, "%c", axes[i].name);
      v->axes[i].index = i;
      sprintf(v->axes[i].suffix, "%c", axes[i].suff);
      v->axes[i].divisor = divisor;

      if (hasAxisData) {
        sprintf(v->axes[i].name, "%s", axisData[i].name);
        v->axes[i].flag = axisData[i].flag;
        short unit = axisData[i].unit;
        v->axes[i].unit = unit;
        char *unith = AXIS_UNITH[unit];
        strncpy(v->axes[i].unith, unith, 20);
        v->axes[i].decimal = axisData[i].dec;
      }
    }
  } else {
    fprintf(stderr, "Failed to get axis names: %d\n", ret);
    return 1;
  }

  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  return 0;
}

int getMachineMessage(MachineMessage *v) {
  short ret;
  OPMSG message;
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);
  ret = cnc_rdopmsg(fLibHandle, 0, 6 + 256, &message);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to read operator message: %d\n", ret);
    return 1;
  }

  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;
  v->number = message.datano;
  sprintf(v->text, "%s", message.data);
  return 0;
}

int getMachineStatus(MachineStatus *v) {
  short ret;
  ODBST status;
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  ret = cnc_statinfo(fLibHandle, &status);
  if (ret != EW_OK) {
    fprintf(stderr, "Cannot cnc_statinfo: %d", ret);
    return 1;
  }
  /*
  ODBALM alarm;
  ret = cnc_alarm(mFlibHndl, &alarm);
  if (ret != EW_OK)
  {
  fprintf(stderr, "Failed to get alarm data!\n");
  exit(EXIT_FAILURE);
  return 1;
  }
  printf("ALARM %d\n", alarm.data);

  ALMINFO alarminfo;
  ret = cnc_rdalminfo(mFlibHndl, 1, short alm_type, short length, &alarminfo)
  */
  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  short aut = status.aut;
  short emergency = status.emergency;
  short motion = status.motion;
  short mstb = status.mstb;
  short run = status.run;
  char *execution =
      MACHINE_EXECUTION[(run == 3 || run == 4)
                            ? 0
                            : (run == 2 || motion == 2 || mstb != 0)
                                  ? 1
                                  : run == 0 ? 2 : 3];
  char *mode =
      MACHINE_MODE[(aut == 5 || aut == 6) ? 0 : (aut == 0 || aut == 3) ? 1 : 2];
  char *estop = MACHINE_ESTOP[emergency == 1 ? 0 : 1];
  strncpy(v->execution, execution, 20);
  strncpy(v->mode, mode, 20);
  strncpy(v->estop, estop, 20);
  v->raw.alarm = status.alarm;
  v->raw.aut = aut;
  v->raw.edit = status.edit;
  v->raw.emergency = emergency;
  v->raw.hdck = status.hdck;
  v->raw.motion = motion;
  v->raw.mstb = mstb;
  v->raw.run = run;
  return 0;
}

int getMachinePartCount(MachinePartCount *v) {
  short ret;
  IODBPSD param;
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  ret = cnc_rdparam(fLibHandle, PART_COUNT_PARAMETER, ALL_AXES, 8, &param);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to read part parameter!\n");
    return 1;
  }

  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;
  v->count = param.u.ldata;
  return 0;
}

int getMachineCycleTime(MachineCycleTime *v) {
  short ret;
  short timeType = 3;  // 0->Power on time, 1->Operating time, 2->Cutting time,
                       // 3->Cycle time, 4->Free purpose,
  IODBTIME time;
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  ret = cnc_rdtimer(fLibHandle, timeType, &time);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get time for type %d!\n", timeType);
    return 1;
  }
  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  v->time = time.minute * 60 * 1000 + time.msec;
  v->raw.minutes = time.minute;
  v->raw.milliseconds = time.msec;
  return 0;
}

int getMachineDynamic(MachineDynamic *v) {
  short ret;
  short num = MAX_AXIS;
  struct timespec t0, t1;
  double tt;
  ODBDY2 dyn;
  ODBSVLOAD axLoad[num];

  clock_gettime(CLOCK_REALTIME, &t0);

  // alarm status, program number, sequence number, actual feed rate,
  // actual spindle speed, absolute/machine/relative position, distance to go
  ret = cnc_rddynamic2(fLibHandle, ALL_AXES, sizeof(dyn), &dyn);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to get cnc dyn data!\n");
    return 1;
  }

  // servo load
  ret = cnc_rdsvmeter(fLibHandle, &num, axLoad);
  if (ret != EW_OK) {
    fprintf(stderr, "cnc_rdsvmeter failed: %d\n", ret);
    return 1;
  }

  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  for (int i = 0; i < num; i++) {
    v->absolute[i] = dyn.pos.faxis.absolute[i] / divisors[i];
    v->relative[i] = dyn.pos.faxis.relative[i] / divisors[i];
    v->actual[i] = dyn.pos.faxis.machine[i] / divisors[i];
    v->load[i] = axLoad[i].svload.data / pow(10.0, axLoad[i].svload.dec);
  }

  v->dim = axisCount;
  v->cprogram = dyn.prgnum;
  v->mprogram = dyn.prgmnum;
  v->sequence = dyn.seqnum;
  v->actf = dyn.actf;
  v->acts = dyn.acts;
  v->alarm = dyn.alarm;

  return 0;
}

int getMachineToolInfo(MachineToolInfo *v) {
  short ret;
  static bool toolManagementEnabled = true;
  static bool useModalToolData = false;
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  if (toolManagementEnabled) {
    // ODBTLIFE4 toolId2;
    // short ret = cnc_toolnum(aFlibhndl, 0, 0, &toolId2);

    ODBTLIFE3 toolId;
    ret = cnc_rdntool(fLibHandle, 0, &toolId);
    if (ret == EW_OK && toolId.data != 0) {
      v->id = toolId.data;
      v->group = toolId.datano;
    } else {
      fprintf(stderr, "Cannot use cnc_rdntool: %d. Trying modal method\n", ret);
      toolManagementEnabled = false;
      useModalToolData = true;
    }
  }

  if (useModalToolData) {
    ODBMDL command;
    // 108 is T command read
    short ret = cnc_modal(fLibHandle, 108, 1, &command);
    if (ret == EW_OK) {
      v->id = command.modal.aux.aux_data;
      v->group = 0;
    } else {
      fprintf(stderr, "cnc_modal failed for T: %d\n", ret);
      useModalToolData = false;
    }
  }
  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  return 0;
}

int getMachineProgram(MachineProgram *v, short programNum) {
  short ret;
  // max length of "header", could read entire file
  char program[2048];
  struct timespec t0, t1;
  double tt;

  clock_gettime(CLOCK_REALTIME, &t0);

  ret = cnc_upstart(fLibHandle, programNum);
  if (ret == EW_OK || ret == EW_BUSY) {
    long len = sizeof(program) - 1;  // One for the \0 terminator
    do {
      ret = cnc_upload3(fLibHandle, &len, program);
      if (ret == EW_OK) {
        program[len] = '\0';
        int lineCount = 0;
        // iterate chars until non-comment ("(") character reached.
        for (char *cp = program; *cp != '\0'; ++cp) {
          if (*cp == '\n') {
            char f = *(cp + 1);
            // allow for empty lines, comments, macros
            if (lineCount > 0 && f != '(' && f != '\n' && f != ' ' && f != '#') {
              *cp = '\0';
              break;
            }
            *cp = ' ';
            lineCount++;
          }
        }
      }
    } while (ret == EW_BUFFER);
  } else if (ret == EW_DATA) {
    v->number = programNum;
    sprintf(v->header, "(ERR: program %d not found on cnc)", programNum);
    fprintf(stderr, "Failed to initiate program read: %d\n", ret);
    return 0;
  } else {
    fprintf(stderr, "Failed to get program %d: %d\n", programNum, ret);
  }
  ret = cnc_upend3(fLibHandle);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to close header read!\n");
    return 1;
  }
  clock_gettime(CLOCK_REALTIME, &t1);
  tt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / BILLION;
  v->executionDuration = tt;

  v->number = programNum;
  sprintf(v->header, "%s", program);
  return 0;
}

int setupConnection(char *deviceIP, int devicePort) {
  short ret;

  printf("using %s:%d\n", deviceIP, devicePort);

  // mandatory logging.
  ret = cnc_startupprocess(0, "focas.log");
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to create log file!\n");
    return 1;
  }

  // library handle.  needs to be closed when finished.
  ret = cnc_allclibhndl3(deviceIP, devicePort, 10 /* timeout (seconds) */,
                         &fLibHandle);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to connect to cnc!\n");
    return 1;
  }

  // set to first path, may be default / not necessary
  ret = cnc_setpath(fLibHandle, 0);
  if (ret != EW_OK) {
    fprintf(stderr, "failed to get set path!\n");
    return 1;
  }
  return 0;
}

void cleanup() {
  short ret;
  printf("cleaning up...\n");

  ret = cnc_freelibhndl(fLibHandle);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to free library handle!\n");
  }
}
