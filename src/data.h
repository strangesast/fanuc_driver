#include "../external/fwlib/fwlib32.h"

typedef struct MachineAxisInfo {
  char id[2];
  short index;
  char suffix[2];
  double divisor;
  char name[5];
  short flag;
  short unit;
  char unith[20];
  short decimal;
} MachineAxisInfo;

typedef struct MachineInfo {
  unsigned long executionDuration;
  char id[36];
  short max_axis;
  short addinfo;
  char cnc_type[3];
  char mt_type[3];
  char series[5];
  char version[5];
  char axes_count_chk[3];
  short axes_count;
  short etherType;
  short etherDevice;
  struct MachineAxisInfo axes[MAX_AXIS];
} MachineInfo;

typedef struct MachineMessage {
  unsigned long executionDuration;
  short number;
  char text[256];
} MachineMessage;

typedef struct MachineRawStatus {
  short alarm;
  short aut;
  short edit;
  short emergency;
  short hdck;
  short motion;
  short mstb;
  short run;
} MachineRawStatus;

typedef struct MachineStatus {
  unsigned long executionDuration;
  char execution[20];
  char mode[20];
  char estop[20];
  MachineRawStatus raw;
} MachineStatus;

typedef struct MachinePartCount {
  unsigned long executionDuration;
  long count;
} MachinePartCount;

typedef struct MachineRawCycleTime {
  long minutes;
  long milliseconds;
} MachineRawCycleTime;

typedef struct MachineCycleTime {
  unsigned long executionDuration;
  long time;  // milliseconds
  MachineRawCycleTime raw;
} MachineCycleTime;

typedef struct MachineDynamic {
  unsigned long executionDuration;
  short cprogram;
  short mprogram;
  short sequence;
  short actf;
  short acts;
  short alarm;
  short dim;
  double absolute[MAX_AXIS];
  double relative[MAX_AXIS];
  double actual[MAX_AXIS];
  double load[MAX_AXIS];
} MachineDynamic;

typedef struct MachineToolInfo {
  unsigned long executionDuration;
  long id;
  long group;
} MachineToolInfo;

typedef struct MachineProgram {
  unsigned long executionDuration;
  short number;
  char header[2048];
} MachineProgram;

int getMachineInfo(MachineInfo *v);
int getMachineMessage(MachineMessage *v);
int getMachineStatus(MachineStatus *v);
int getMachinePartCount(MachinePartCount *v);
int getMachineDynamic(MachineDynamic *v);
int getMachineToolInfo(MachineToolInfo *v);
int getMachineProgram(MachineProgram *v, short programNum);
int setupConnection(char *deviceIP, int devicePort);
void cleanup();
