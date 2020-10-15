#define _GNU_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include "./data.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../external/fwlib/fwlib32.h"
#define PART_COUNT_PARAMETER 6711
#define BILLION 1000000000.0

char *AXIS_UNITH[] = {"mm",          "inch",   "degree",   "mm/minute",
                      "inch/minute", "rpm",    "mm/round", "inch/round",
                      "%",           "Ampere", "Second"};
char *MACHINE_EXECUTION[] = {"ACTIVE", "INTERRUPTED", "STOPPED", "READY"};
char *MACHINE_MODE[] = {"MANUAL", "MANUAL_DATA_INPUT", "AUTOMATIC"};
char *MACHINE_ESTOP[] = {"TRIGGERED", "ARMED"};

short axisCount = MAX_AXIS;
short unsigned int libh;
double divisors[MAX_AXIS];

// get machine properties (unique id, characteristics, software versions)
int getMachineInfo(MachineInfo *v) {
  short etherType;
  short etherDevice;
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
  unsigned long tt;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (cnc_sysinfo(libh, &sysinfo) != EW_OK ||
      cnc_rdcncid(libh, cncIDs) != EW_OK ||
      cnc_rdetherinfo(libh, &etherType, &etherDevice) != EW_OK) {
    fprintf(stderr, "Failed to get cnc sysinfo!\n");
    return 1;
  }

  // machine id
  sprintf(v->id, "%08lx-%08lx-%08lx-%08lx", cncIDs[0], cncIDs[1], cncIDs[2],
          cncIDs[3]);
  v->max_axis = sysinfo.max_axis;
  v->addinfo = sysinfo.addinfo;
  sprintf(v->cnc_type, "%.2s", sysinfo.cnc_type);
  sprintf(v->mt_type, "%.2s", sysinfo.mt_type);
  sprintf(v->series, "%.4s", sysinfo.series);
  sprintf(v->version, "%.4s", sysinfo.version);
  sprintf(v->axes_count_chk, "%.2s", sysinfo.axes);
  v->etherType = etherType;
  v->etherDevice = etherDevice;

  bool hasAxisData =
      cnc_rdaxisdata(libh, 1 /* Position Value */, (short *)types, num, &len,
                     axisData) == EW_OK;

  if (!hasAxisData) {
    fprintf(stderr, "cnc_rdaxisdata failed.");
  }

  if (cnc_getfigure(libh, 0, &count, inprec, outprec) != EW_OK ||
      cnc_rdaxisname(libh, &axisCount, axes) != EW_OK) {
    fprintf(stderr, "Failed to get axis info\n");
    return 1;
  }

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

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;

  return 0;
}

int getMachineMessage(MachineMessage *v) {
  struct timespec t0, t1;
  unsigned long tt;
  OPMSG message;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
  if (cnc_rdopmsg(libh, 0, 6 + 256, &message) != EW_OK) {
    fprintf(stderr, "Failed to read operator message.\n");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;
  v->number = message.datano;
  sprintf(v->text, "%s", message.data);
  return 0;
}

int getMachineStatus(MachineStatus *v) {
  struct timespec t0, t1;
  unsigned long tt;
  ODBST status;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (cnc_statinfo(libh, &status) != EW_OK) {
    fprintf(stderr, "Cannot get cnc_statinfo.\n");
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
  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
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
  struct timespec t0, t1;
  unsigned long tt;
  IODBPSD param;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (cnc_rdparam(libh, PART_COUNT_PARAMETER, ALL_AXES, 8, &param) != EW_OK) {
    fprintf(stderr, "Failed to read part parameter!\n");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;
  v->count = param.u.ldata;
  return 0;
}

int getMachineCycleTime(MachineCycleTime *v) {
  short timeType = 3;  // 0->Power on time, 1->Operating time, 2->Cutting
                       // time, 3->Cycle time, 4->Free purpose,
  unsigned long tt;
  struct timespec t0, t1;
  IODBTIME time;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (cnc_rdtimer(libh, timeType, &time) != EW_OK) {
    fprintf(stderr, "Failed to get time for type %d!\n", timeType);
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;

  v->time = time.minute * 60 * 1000 + time.msec;
  v->raw.minutes = time.minute;
  v->raw.milliseconds = time.msec;
  return 0;
}

int getMachineDynamic(MachineDynamic *v) {
  short num = MAX_AXIS;
  struct timespec t0, t1;
  unsigned long tt;
  ODBDY2 dyn;
  ODBSVLOAD axLoad[num];

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (cnc_rddynamic2(libh, ALL_AXES, sizeof(dyn), &dyn) != EW_OK ||
      cnc_rdsvmeter(libh, &num, axLoad) != EW_OK) {
    fprintf(stderr, "Failed to get cnc dyn / load data!\n");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
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
  static bool toolManagementEnabled = true;
  static bool useModalToolData = false;
  struct timespec t0, t1;
  unsigned long tt;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  if (toolManagementEnabled) {
    // ODBTLIFE4 toolId2;
    // short ret = cnc_toolnum(aFlibhndl, 0, 0, &toolId2);

    ODBTLIFE3 toolId;
    if (cnc_rdntool(libh, 0, &toolId) == EW_OK && toolId.data != 0) {
      v->id = toolId.data;
      v->group = toolId.datano;
    } else {
      fprintf(stderr, "Cannot use cnc_rdntool. Trying modal method\n");
      toolManagementEnabled = false;
      useModalToolData = true;
    }
  }

  if (useModalToolData) {
    ODBMDL command;
    // 108 is T command read
    if (cnc_modal(libh, 108, 1, &command) == EW_OK) {
      v->id = command.modal.aux.aux_data;
      v->group = 0;
    } else {
      fprintf(stderr, "cnc_modal failed for T\n");
      useModalToolData = false;
    }
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;

  return 0;
}

int getMachineProgram(MachineProgram *v, short programNum) {
  short ret;
  // max length of "header", could read entire file
  char program[2048];
  struct timespec t0, t1;
  unsigned long tt;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  ret = cnc_upstart(libh, programNum);
  if (ret == EW_OK || ret == EW_BUSY) {
    long len = sizeof(program) - 1;  // One for the \0 terminator
    do {
      ret = cnc_upload3(libh, &len, program);
      if (ret == EW_OK) {
        program[len] = '\0';
        int lineCount = 0;
        // iterate chars until non-comment ("(") character reached.
        for (char *cp = program; *cp != '\0'; ++cp) {
          if (*cp == '\n') {
            char f = *(cp + 1);
            // allow for empty lines, comments, macros
            if (lineCount > 0 && f != '(' && f != '\n' && f != ' ' &&
                f != '#') {
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

  if (cnc_upend3(libh) != EW_OK) {
    fprintf(stderr, "Failed to close header read!\n");
    return 1;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;

  v->number = programNum;
  sprintf(v->header, "%s", program);
  return 0;
}

int getMachineBlock(MachineBlock *v) {
  short ret;
  struct timespec t0, t1;
  unsigned long tt;
  char buf[1024];
  unsigned short len = sizeof(buf);
  short num;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

  ret = cnc_rdexecprog(libh, (unsigned short *)&len, &num, buf);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed read exec prog: %d!\n", ret);
    return 1;
  }
  buf[len] = '\0';
  for (int i = 0; i < len; i++) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      break;
    }
  }

  strncpy(v->block, buf, len);

  clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

  tt = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
  v->executionDuration = tt;

  return 0;
}

int setupConnection(char *deviceIP, int devicePort) {
  printf("using %s:%d\n", deviceIP, devicePort);

  // mandatory logging.
  if (cnc_startupprocess(0, "focas.log") != EW_OK) {
    fprintf(stderr, "Failed to create log file!\n");
    return 1;
  }

  // library handle.  needs to be closed when finished.
  if (cnc_allclibhndl3(deviceIP, devicePort, 10 /* timeout (seconds) */,
                       &libh) != EW_OK) {
    fprintf(stderr, "Failed to connect to cnc!\n");
    return 1;
  }

  // set to first path, may be default / not necessary
  if (cnc_setpath(libh, 0) != EW_OK) {
    fprintf(stderr, "failed to get set path!\n");
    return 1;
  }
  return 0;
}

void cleanup() {
  printf("cleaning up...\n");

  if (cnc_freelibhndl(libh) != EW_OK) {
    fprintf(stderr, "Failed to free library handle!\n");
  }
}
