#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include "./fwlib/fwlib32.h"
#include "./avro.h"
#define MAXPATH 1024
#define PART_COUNT_PARAMETER 6711

short unsigned int mFlibHndl;
char mDeviceIP[MAXPATH] = "";
int mDevicePort;
char *AXIS_UNITH[] = {"mm", "inch", "degree", "mm/minute", "inch/minute", "rpm", "mm/round", "inch/round", "%", "Ampere", "Second"};
avro_schema_t machine_detail_schema = NULL;
avro_schema_t machine_status_schema = NULL;
avro_schema_t machine_part_count_schema = NULL;
avro_schema_t machine_dynamic_schema = NULL;
avro_schema_t machine_axis_detail_schema = NULL;
avro_schema_t machine_tool_info_schema = NULL;
avro_schema_t machine_program_schema = NULL;
avro_schema_t datum_schema = NULL;
avro_schema_t execution_schema = NULL;
avro_schema_t mode_schema = NULL;
avro_schema_t estop_schema = NULL;

enum MachineExecution
{
  ACTIVE,
  INTERRUPTED,
  STOPPED,
  READY
};

enum MachineMode
{
  MANUAL,
  MANUAL_DATA_INPUT,
  AUTOMATIC
};

enum MachineEStop
{
  TRIGGERED,
  ARMED
};

static volatile int runningCondition = 0;

int loadAvroSchema(char *fname, avro_schema_t *schema)
{
  FILE *f;
  long fsize;
  char *SCHEMA = NULL;

  f = fopen(fname, "rb");
  if (!f)
  {
    perror(fname);
    return 1;
  }
  fseek(f, 0L, SEEK_END);
  fsize = ftell(f);
  rewind(f);

  SCHEMA = calloc(fsize + 1, sizeof(char));
  if (!SCHEMA)
  {
    fputs("failed to alloc", stderr);
    fclose(f);
    return 1;
  }

  fread(SCHEMA, sizeof(char), fsize, f);
  fclose(f);

  if (avro_schema_from_json_length(SCHEMA, fsize, schema))
  {
    fprintf(stderr, "Unable to parse avro schema file \"%s\": %s\n", fname, avro_strerror());
    free(SCHEMA);
    return 1;
  }

  free(SCHEMA);
  return 0;
}

int printAvroDatum(avro_datum_t *datum)
{
  char *json = NULL;
  if (avro_datum_to_json(*datum, 1, &json))
  {
    fprintf(stderr, "failed to serialize avro datum\n");
    return 1;
  }
  printf("%s\n", json);
  free(json);
  return 0;
}

// get machine properties (unique id, characteristics, software versions)
int readMachineInfo(avro_datum_t *datum)
{
  short ret;
  unsigned long cncIDs[4];
  short etherType;
  short etherDevice;
  char buf[36];
  ODBSYS sysinfo;

  ret = cnc_sysinfo(mFlibHndl, &sysinfo);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc sysinfo!\n");
    return 1;
  }

  ret = cnc_rdcncid(mFlibHndl, cncIDs);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc id!\n");
    return 1;
  }

  ret = cnc_rdetherinfo(mFlibHndl, &etherType, &etherDevice);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc ether info!\n");
    return 1;
  }

  sprintf(buf, "%08lx-%08lx-%08lx-%08lx", cncIDs[0] & 0xffffffff, cncIDs[1] & 0xffffffff, cncIDs[2] & 0xffffffff, cncIDs[3] & 0xffffffff);
  avro_datum_t id_datum = avro_string(buf);
  avro_datum_t max_axis_datum = avro_int32(sysinfo.max_axis);
  avro_datum_t addinfo_datum = avro_int32(sysinfo.addinfo);
  sprintf(buf, "%.2s", sysinfo.cnc_type);
  avro_datum_t cnc_type_datum = avro_string(buf);
  sprintf(buf, "%.2s", sysinfo.mt_type);
  avro_datum_t mt_type_datum = avro_string(buf);
  sprintf(buf, "%.4s", sysinfo.series);
  avro_datum_t series_datum = avro_string(buf);
  sprintf(buf, "%.4s", sysinfo.version);
  avro_datum_t version_datum = avro_string(buf);
  sprintf(buf, "%.2s", sysinfo.axes);
  avro_datum_t axes_datum = avro_string(buf);

  if (avro_record_set(*datum, "id", id_datum) ||
      avro_record_set(*datum, "max_axis", max_axis_datum) ||
      avro_record_set(*datum, "addinfo", addinfo_datum) ||
      avro_record_set(*datum, "cnc_type", cnc_type_datum) ||
      avro_record_set(*datum, "mt_type", mt_type_datum) ||
      avro_record_set(*datum, "series", series_datum) ||
      avro_record_set(*datum, "version", version_datum) ||
      avro_record_set(*datum, "axes", axes_datum))
  {
    fprintf(stderr, "Unable to create datum structure\n");
    return 1;
  }

  avro_datum_decref(id_datum);
  avro_datum_decref(max_axis_datum);
  avro_datum_decref(addinfo_datum);
  avro_datum_decref(cnc_type_datum);
  avro_datum_decref(mt_type_datum);
  avro_datum_decref(series_datum);
  avro_datum_decref(version_datum);
  avro_datum_decref(axes_datum);
  return 0;
}

int readMachineMessages(avro_datum_t *datum)
{
  OPMSG messages[6];
  int ret = cnc_rdopmsg(mFlibHndl, 0, 6 + 256, messages);
  if (ret != EW_OK || messages->datano == -1)
  {
    return 1;
  }
  // char buf[32]; // code
  // sprintf(buf, "%d", messages->datano);
  // messages->data // text
  // buf // code
  return 0;
}

int readMachineStatus(avro_datum_t *datum)
{
  short ret;
  ODBST status;

  ret = cnc_statinfo(mFlibHndl, &status);
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

  enum MachineExecution execution = (status.run == 3 || status.run == 4) ? ACTIVE : (status.run == 2 || status.motion == 2 || status.mstb != 0) ? INTERRUPTED : status.run == 0 ? STOPPED : READY;
  avro_datum_t execution_datum = avro_enum(execution_schema, execution);

  enum MachineMode mode = (status.aut == 5 || status.aut == 6) ? MANUAL : (status.aut == 0 || status.aut == 3) ? MANUAL_DATA_INPUT : AUTOMATIC;
  avro_datum_t mode_datum = avro_enum(mode_schema, mode);

  enum MachineEStop estop = status.emergency == 1 ? TRIGGERED : ARMED;
  avro_datum_t estop_datum = avro_enum(estop_schema, estop);

  avro_datum_t aut_datum = avro_int32(status.aut);
  avro_datum_t run_datum = avro_int32(status.run);
  avro_datum_t edit_datum = avro_int32(status.edit);
  avro_datum_t motion_datum = avro_int32(status.motion);
  avro_datum_t mstb_datum = avro_int32(status.mstb);
  avro_datum_t emergency_datum = avro_int32(status.emergency);
  avro_datum_t alarm_datum = avro_int32(status.alarm);

  if (avro_record_set(*datum, "execution", execution_datum) ||
      avro_record_set(*datum, "mode", mode_datum) ||
      avro_record_set(*datum, "estop", estop_datum) ||
      avro_record_set(*datum, "auto", aut_datum) ||
      avro_record_set(*datum, "run", run_datum) ||
      avro_record_set(*datum, "edit", edit_datum) ||
      avro_record_set(*datum, "motion", motion_datum) ||
      avro_record_set(*datum, "mstb", mstb_datum) ||
      avro_record_set(*datum, "emergency", emergency_datum) ||
      avro_record_set(*datum, "alarm", alarm_datum))
  {
    fprintf(stderr, "Unable to create datum structure\n");
    return 1;
  }

  avro_datum_decref(execution_datum);
  avro_datum_decref(mode_datum);
  avro_datum_decref(estop_datum);

  avro_datum_decref(aut_datum);
  avro_datum_decref(run_datum);
  avro_datum_decref(edit_datum);
  avro_datum_decref(motion_datum);
  avro_datum_decref(mstb_datum);
  avro_datum_decref(emergency_datum);
  avro_datum_decref(alarm_datum);

  return 0;
}

int readMachinePartCount(avro_datum_t *datum)
{
  short ret;
  short timeType = 3; // 0->Power on time, 1->Operating time, 2->Cutting time, 3->Cycle time, 4->Free purpose,
  IODBTIME time;
  IODBPSD param;

  ret = cnc_rdparam(mFlibHndl, PART_COUNT_PARAMETER, -1, 8, &param);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to read part parameter!\n");
    return 1;
  }

  ret = cnc_rdtimer(mFlibHndl, timeType, &time);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get time for type %d!\n", timeType);
    return 1;
  }
  avro_datum_t count_datum = avro_int64(param.u.ldata);
  avro_datum_t minutes_datum = avro_int64(time.minute);
  avro_datum_t milliseconds_datum = avro_int64(time.msec);

  if (avro_record_set(*datum, "count", count_datum) ||
      avro_record_set(*datum, "minutes", minutes_datum) ||
      avro_record_set(*datum, "milliseconds", milliseconds_datum))
  {
    fprintf(stderr, "Unable to create datum structure\n");
    return 1;
  }

  avro_datum_decref(count_datum);
  avro_datum_decref(minutes_datum);
  avro_datum_decref(milliseconds_datum);

  return 0;
}

int readMachineDynamic(avro_datum_t *datum, double divisors[MAX_AXIS], long *programNum)
{
  short ret;
  ODBDY2 dyn;
  ODBSVLOAD axLoad[MAX_AXIS];
  short num = MAX_AXIS;

  ret = cnc_rddynamic2(mFlibHndl, -1, sizeof(dyn), &dyn);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get cnc dyn data!\n");
    return 1;
  }

  ret = cnc_rdsvmeter(mFlibHndl, &num, axLoad);
  if (ret != EW_OK)
  {
    fprintf(stderr, "cnc_rdsvmeter failed: %d", ret);
    return 1;
  }

  avro_schema_t axis_schema = avro_schema_record("MachineDynamicAxis", NULL);
  avro_schema_t axis_id_schema = avro_schema_string();
  avro_schema_record_field_append(axis_schema, "id", axis_id_schema);
  avro_schema_t axis_absolute_schema = avro_schema_double();
  avro_schema_record_field_append(axis_schema, "absolute", axis_absolute_schema);
  avro_schema_t axis_actual_schema = avro_schema_double();
  avro_schema_record_field_append(axis_schema, "actual", axis_actual_schema);
  avro_schema_t axis_load_schema = avro_schema_double();
  avro_schema_record_field_append(axis_schema, "load", axis_load_schema);

  avro_schema_t axis_array_schema = avro_schema_array(axis_schema);

  avro_datum_t axis_array_datum = avro_array(axis_array_schema);

  int i = 0;
  for (i = 0; i < num; i++)
  {
    char buf[sizeof(char) * 2];
    sprintf(buf, "%c", axLoad[i].svload.name);
    double abs = dyn.pos.faxis.absolute[i] / divisors[i];
    double act = dyn.pos.faxis.machine[i] / divisors[i];
    double load = axLoad[i].svload.data / pow(10.0, axLoad[i].svload.dec);

    avro_datum_t axis_datum = avro_datum_from_schema(axis_schema);

    avro_datum_t axis_id_datum = avro_string(buf);
    avro_record_set(axis_datum, "id", axis_id_datum);

    avro_datum_t axis_absolute_datum = avro_double(abs);
    avro_record_set(axis_datum, "absolute", axis_absolute_datum);

    avro_datum_t axis_actual_datum = avro_double(act);
    avro_record_set(axis_datum, "actual", axis_actual_datum);

    avro_datum_t axis_load_datum = avro_double(load);
    avro_record_set(axis_datum, "load", axis_load_datum);

    avro_datum_decref(axis_id_datum);
    avro_datum_decref(axis_absolute_datum);
    avro_datum_decref(axis_actual_datum);
    avro_datum_decref(axis_load_datum);

    avro_array_append_datum(axis_array_datum, axis_datum);
    avro_datum_decref(axis_datum);
  }
  printf("\n");

  avro_record_set(*datum, "axis", axis_array_datum);

  *programNum = dyn.prgnum;
  avro_datum_t alarm_datum = avro_int64(dyn.alarm);      // alarm status
  avro_datum_t cprogram_datum = avro_int64(dyn.prgnum);  // current program
  avro_datum_t mprogram_datum = avro_int64(dyn.prgmnum); // main program
  avro_datum_t sequence_datum = avro_int64(dyn.seqnum);  // line no
  avro_datum_t actf_datum = avro_int64(dyn.actf);        // actual feedrate
  avro_datum_t acts_datum = avro_int64(dyn.acts);        // actual spindle speed

  if (avro_record_set(*datum, "alarm", alarm_datum) ||
      avro_record_set(*datum, "cprogram", cprogram_datum) ||
      avro_record_set(*datum, "mprogram", mprogram_datum) ||
      avro_record_set(*datum, "sequence", sequence_datum) ||
      avro_record_set(*datum, "actf", actf_datum) ||
      avro_record_set(*datum, "acts", acts_datum))
  {
    fprintf(stderr, "Unable to create datum structure\n");
    return 1;
  }

  avro_datum_decref(alarm_datum);
  avro_datum_decref(cprogram_datum);
  avro_datum_decref(mprogram_datum);
  avro_datum_decref(sequence_datum);
  avro_datum_decref(actf_datum);
  avro_datum_decref(acts_datum);
  avro_datum_decref(axis_array_datum);

  avro_schema_decref(axis_id_schema);
  avro_schema_decref(axis_absolute_schema);
  avro_schema_decref(axis_actual_schema);
  avro_schema_decref(axis_load_schema);

  avro_schema_decref(axis_schema);
  avro_schema_decref(axis_array_schema);

  return 0;
}

// qty, units, precision, name of axes
int readMachineAxis(avro_datum_t *arrayDatum, double divisors[])
{
  short ret;
  const int num = 1;
  short pathNumber = 0;
  short types[] = {1 /* actual position */};
  short len = MAX_AXIS;
  short axisCount = MAX_AXIS;
  char buf[10];
  short count, inprec[MAX_AXIS], outprec[MAX_AXIS];
  ODBAXDT axisData[MAX_AXIS * num];
  ODBAXISNAME axes[MAX_AXIS];

  ret = cnc_rdaxisdata(mFlibHndl, 1 /* Position Value */, (short *)types, num, &len, axisData);
  bool hasAxisData = ret == EW_OK;
  if (!hasAxisData)
  {
    fprintf(stderr, "cnc_rdaxisdata returned %d for path %d\n", ret, pathNumber);
  }

  ret = cnc_getfigure(mFlibHndl, 0, &count, inprec, outprec);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to get axis scale: %d\n", ret);
    return 1;
  }

  ret = cnc_rdaxisname(mFlibHndl, &axisCount, axes);
  if (ret == EW_OK)
  {
    int i = 0;
    for (i = 0; i < axisCount; i++)
    {
      avro_datum_t datum = avro_datum_from_schema(machine_axis_detail_schema);
      double divisor = pow((long double)10.0, (long double)inprec[i]);
      divisors[i] = divisor;
      sprintf(buf, "%c", axes[i].name);
      avro_datum_t id_datum = avro_string(buf);
      avro_datum_t index_datum = avro_int32(i);
      sprintf(buf, "%c", axes[i].suff);
      avro_datum_t suffix_datum = avro_string(buf);
      avro_datum_t divisor_datum = avro_double(divisor);
      if (avro_record_set(datum, "id", id_datum) ||
          avro_record_set(datum, "index", index_datum) ||
          avro_record_set(datum, "suffix", suffix_datum) ||
          avro_record_set(datum, "divisor", divisor_datum))
      {
        fprintf(stderr, "failed to set axis info\n");
      }
      if (hasAxisData)
      {
        char name[5];
        memset(name, '\0', sizeof(name));
        strncpy(name, axisData[i].name, 4);
        // sprintf(name, "%.4s", axisData[i].name);
        short unit = axisData[i].unit;
        char *unith = AXIS_UNITH[unit];

        avro_datum_t name_datum = avro_string(name);
        avro_datum_t flag_datum = avro_int32(axisData[i].flag);
        avro_datum_t unit_datum = avro_int32(unit);
        avro_datum_t unith_datum = avro_string(unith);
        avro_datum_t decimal_datum = avro_int32(axisData[i].dec);

        if (avro_record_set(datum, "name", name_datum) ||
            avro_record_set(datum, "flag", flag_datum) ||
            avro_record_set(datum, "unit", unit_datum) ||
            avro_record_set(datum, "unith", unith_datum) ||
            avro_record_set(datum, "decimal", unit_datum))
        {
          fprintf(stderr, "failed to set axis data\n");
        }

        avro_datum_decref(name_datum);
        avro_datum_decref(flag_datum);
        avro_datum_decref(unit_datum);
        avro_datum_decref(unith_datum);
        avro_datum_decref(decimal_datum);
      }
      avro_datum_decref(id_datum);
      avro_datum_decref(index_datum);
      avro_datum_decref(suffix_datum);
      avro_datum_decref(divisor_datum);
      if (avro_array_append_datum(*arrayDatum, datum))
      {
        fprintf(stderr, "failed to append to axis array\n");
        return 1;
      }
    }
  }
  else
  {
    fprintf(stderr, "Failed to get axis names: %d\n", ret);
    return 1;
  }

  return 0;
}

int readMachineToolInfo(avro_datum_t *datum)
{
  short ret;
  static bool toolManagementEnabled = true;
  static bool useModalToolData = false;
  if (toolManagementEnabled)
  {
    // ODBTLIFE4 toolId2;
    // short ret = cnc_toolnum(aFlibhndl, 0, 0, &toolId2);

    ODBTLIFE3 toolId;
    ret = cnc_rdntool(mFlibHndl, 0, &toolId);
    if (ret == EW_OK && toolId.data != 0)
    {
      avro_datum_t tool_id_datum = avro_int64(toolId.data);
      avro_datum_t tool_group_datum = avro_int32(toolId.datano);

      if (avro_record_set(*datum, "id", tool_id_datum) ||
          avro_record_set(*datum, "group", tool_group_datum))
      {
        fprintf(stderr, "Failed to set datum for toolid\n");
      }

      avro_datum_decref(tool_id_datum);
      avro_datum_decref(tool_group_datum);
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
    short ret = cnc_modal(mFlibHndl, 108, 1, &command);
    if (ret == EW_OK)
    {
      long toolId = command.modal.aux.aux_data;
      avro_datum_t tool_id_datum = avro_int64(toolId);
      if (avro_record_set(*datum, "id", tool_id_datum))
      {
        fprintf(stderr, "Failed to set datum for toolid\n");
      }
      avro_datum_decref(tool_id_datum);
    }
    else
    {
      fprintf(stderr, "cnc_modal failed for T: %d", ret);
      useModalToolData = false;
    }
  }

  return 0;
}

int readMachineProgram(short programNum, avro_datum_t *datum)
{
  short ret;
  // max length of "header", could read entire file
  char program[2048];

  ret = cnc_upstart(mFlibHndl, programNum);
  if (ret == EW_OK)
  {
    long len = sizeof(program) - 1; // One for the \0 terminator
    do
    {
      ret = cnc_upload3(mFlibHndl, &len, program);
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
  ret = cnc_upend3(mFlibHndl);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to close header read!\n");
    return 1;
  }
  avro_datum_t number_datum = avro_int32(programNum);
  avro_datum_t header_datum = avro_string(program);
  if (
      avro_record_set(*datum, "number", number_datum) ||
      avro_record_set(*datum, "header", header_datum))
  {
    fprintf(stderr, "Failed to set datum for toolid\n");
    return 1;
  }
  avro_datum_decref(number_datum);
  avro_datum_decref(header_datum);

  return 0;
}

void cleanup()
{
  printf("cleaning up...\n");

  avro_schema_decref(execution_schema);
  avro_schema_decref(mode_schema);
  avro_schema_decref(estop_schema);

  avro_schema_decref(machine_detail_schema);
  avro_schema_decref(machine_status_schema);
  avro_schema_decref(machine_part_count_schema);
  avro_schema_decref(machine_dynamic_schema);
  avro_schema_decref(machine_axis_detail_schema);
  avro_schema_decref(machine_tool_info_schema);
  avro_schema_decref(machine_program_schema);
  avro_schema_decref(datum_schema);

  short ret = cnc_freelibhndl(mFlibHndl);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to free library handle!\n");
  }
}

void setupAvro()
{
  execution_schema = avro_schema_enum("Execution");
  avro_schema_enum_symbol_append(execution_schema, "ACTIVE");
  avro_schema_enum_symbol_append(execution_schema, "INTERRUPTED");
  avro_schema_enum_symbol_append(execution_schema, "STOPPED");
  avro_schema_enum_symbol_append(execution_schema, "READY");

  mode_schema = avro_schema_enum("Mode");
  avro_schema_enum_symbol_append(mode_schema, "MANUAL");
  avro_schema_enum_symbol_append(mode_schema, "MANUAL_DATA_INPUT");
  avro_schema_enum_symbol_append(mode_schema, "AUTOMATIC");

  estop_schema = avro_schema_enum("EStop");
  avro_schema_enum_symbol_append(estop_schema, "TRIGGERED");
  avro_schema_enum_symbol_append(estop_schema, "ARMED");

  if (loadAvroSchema("./avsc/MachineDetail.avsc", &machine_detail_schema) ||
      loadAvroSchema("./avsc/MachineStatus.avsc", &machine_status_schema) ||
      loadAvroSchema("./avsc/MachineCycleTime.avsc", &machine_part_count_schema) ||
      loadAvroSchema("./avsc/MachineDynamic.avsc", &machine_dynamic_schema) ||
      loadAvroSchema("./avsc/MachineAxisDetail.avsc", &machine_axis_detail_schema) ||
      loadAvroSchema("./avsc/MachineToolInfo.avsc", &machine_tool_info_schema) ||
      loadAvroSchema("./avsc/MachineProgram.avsc", &machine_program_schema))
  {
    fprintf(stderr, "failed to read avro schema!\n");
    exit(EXIT_FAILURE);
  }

  datum_schema = avro_schema_record("Datum", NULL);
  avro_schema_record_field_append(datum_schema, "timestamp", avro_schema_long());
  avro_schema_record_field_append(datum_schema, "machine_id", avro_schema_string());

  avro_schema_t datum_data_schema = avro_schema_union();
  avro_schema_union_append(datum_data_schema, machine_detail_schema);
  avro_schema_union_append(datum_data_schema, machine_status_schema);
  avro_schema_union_append(datum_data_schema, machine_part_count_schema);
  avro_schema_union_append(datum_data_schema, machine_dynamic_schema);
  avro_schema_union_append(datum_data_schema, machine_axis_detail_schema);
  avro_schema_union_append(datum_data_schema, machine_tool_info_schema);
  avro_schema_union_append(datum_data_schema, machine_program_schema);

  avro_schema_t datum_data_array_schema = avro_schema_array(datum_data_schema);
  avro_schema_record_field_append(datum_schema, "data", datum_data_array_schema);

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

void intHandler(int dummy)
{
  runningCondition = 1;
}

int main()
{
  short ret;

  long programNum = 0;
  long lastProgramNum = programNum;
  double divisors[MAX_AXIS];

  setupEnv();
  setupAvro();

  // TODO: merge this with machineInfo
  avro_schema_t machineAxisArraySchema = avro_schema_array(machine_axis_detail_schema);
  avro_datum_t machineAxisArrayDatum = avro_array(machineAxisArraySchema);

  avro_datum_t machineInfoDatum = avro_datum_from_schema(machine_detail_schema);
  avro_datum_t machineDynamicDatum = avro_datum_from_schema(machine_dynamic_schema);
  avro_datum_t machinePartCountDatum = avro_datum_from_schema(machine_part_count_schema);
  avro_datum_t machineStatusDatum = avro_datum_from_schema(machine_status_schema);
  avro_datum_t machineToolInfoDatum = avro_datum_from_schema(machine_tool_info_schema);

  avro_datum_t updatesArrayDatum = avro_datum_from_schema(datum_schema);
  avro_datum_t updatesArrayDatum = NULL;
  avro_record_get(updatesDatum, "data", &updatesArrayDatum);

  printf("using %s:%d\n", mDeviceIP, mDevicePort);

  signal(SIGINT, intHandler);

  // mandatory logging.
  ret = cnc_startupprocess(0, "focas.log");
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to create log file!\n");
    return 1;
  }

  // library handle.  needs to be closed when finished.
  ret = cnc_allclibhndl3(mDeviceIP, mDevicePort, 10, &mFlibHndl);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to connect to cnc!\n");
    return 1;
  }
  atexit(cleanup);

  // set to first path, may be default / not necessary
  ret = cnc_setpath(mFlibHndl, 0);
  if (ret != EW_OK)
  {
    fprintf(stderr, "failed to get set path!\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  // get values, add to array
  // loop:
  //   if changed, add to array

  if (readMachineInfo(&machineInfoDatum))
  {
    fprintf(stderr, "failed to read machine info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  // add to array
  avro_array_append_datum(updatesArrayDatum, machinePartCountDatum);
  // printAvroDatum(&machineInfoDatum);
  avro_datum_decref(machineInfoDatum);

  if (readMachineAxis(&machineAxisArrayDatum, divisors))
  {
    fprintf(stderr, "failed to read machine axis data!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  avro_array_append_datum(updatesArrayDatum, machineAxisArraySchema);
  // printAvroDatum(&machineAxisArrayDatum);
  avro_datum_decref(machineAxisArrayDatum);

  // do
  // {
  avro_datum_t dt;

  dt = avro_datum_from_schema(machine_part_count_schema);
  if (readMachinePartCount(&dt))
  {
    fprintf(stderr, "failed to read machine part count data!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  if (!avro_datum_equal(dt, machinePartCountDatum))
  {
    avro_datum_decref(machinePartCountDatum);
    machinePartCountDatum = dt;
    avro_array_append_datum(updatesArrayDatum, machinePartCountDatum);
  }
  else
  {
    avro_datum_decref(dt);
  }

  dt = avro_datum_from_schema(machine_status_schema);
  if (readMachineStatus(&dt))
  {
    fprintf(stderr, "failed to read machine status!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  if (!avro_datum_equal(dt, machineStatusDatum))
  {
    avro_datum_decref(machineStatusDatum);
    machineStatusDatum = dt;
    avro_array_append_datum(updatesArrayDatum, machineStatusDatum);
  }
  else
  {
    avro_datum_decref(dt);
  }

  dt = avro_datum_from_schema(machine_tool_info_schema);
  if (readMachineToolInfo(&dt))
  {
    fprintf(stderr, "failed to get tool info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  if (!avro_datum_equal(dt, machineToolInfoDatum))
  {
    avro_datum_decref(machineToolInfoDatum);
    machineToolInfoDatum = dt;
    avro_array_append_datum(updatesArrayDatum, machineToolInfoDatum);
  }
  else
  {
    avro_datum_decref(dt);
  }

  if (!readMachineDynamic(&machineDynamicDatum, divisors, &programNum))
  {
    fprintf(stderr, "failed to read dynamic info!\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  avro_array_append_datum(updatesArrayDatum, machineDynamicDatum);

  if (programNum != lastProgramNum)
  {
    avro_datum_t machineProgramDatum = avro_datum_from_schema(machine_program_schema);
    readMachineProgram(programNum, &machineProgramDatum);
    lastProgramNum = programNum;

    avro_array_append_datum(updatesArrayDatum, machineProgramDatum);
    avro_datum_decref(machineProgramDatum);
  }

  printAvroDatum(&updatesArrayDatum);

  // avro_datum_t cprogramDatum = NULL;
  // avro_record_get(machineDynamicDatum, "cprogram", &cprogramDatum);
  // int64_t cprogram;
  // avro_int64_get(cprogramDatum, &cprogram);
  // printf("cprogram: %lld\n", cprogram);

  avro_datum_decref(updatesDatum);
  avro_datum_decref(updatesArrayDatum);

  // updatesDatum = avro_datum_from_schema(datum_schema);
  // updatesArrayDatum = NULL;
  // avro_record_get(updatesDatum, "data", &updatesArrayDatum);

  // sleep(1);
  // } while (!runningCondition);

  avro_schema_decref(machineAxisArraySchema);
  avro_datum_decref(machineAxisArrayDatum);
  avro_datum_decref(machineInfoDatum);
  avro_datum_decref(machineDynamicDatum);
  avro_datum_decref(machinePartCountDatum);
  avro_datum_decref(machineStatusDatum);
  avro_datum_decref(machineToolInfoDatum);
  avro_record_get(updatesDatum, "data", &updatesArrayDatum);

  avro_datum_decref(machineDynamicDatum);

  /*
  */

  /* load?
  ODBSVLOAD axLoad[MAX_AXIS];
  short num = MAX_AXIS;
  ret = cnc_rdsvmeter(mFlibHndl, &num, axLoad);
  if (ret != EW_OK)
  {
    gLogger->error("cnc_rdsvmeter failed for path %d: %d", pathNumber, ret);
    return false;
  }
  */

  // mProgramNum = dyn.prgnum;
  // }

  // getCondition(mFlibHndl, dyn.alarm);

  /*
  struct odbtlife4 toolId2;
  ret = cnc_toolnum(mFlibHndl, 0, 0, &toolId2);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to read tool number!\n");
    exit(EXIT_FAILURE);
    return 1;
  }

  struct odbtlife3 toolId;
  ret = cnc_rdntool(mFlibHndl, 0, &toolId);
  if (ret != EW_OK)
  {
    fprintf(stderr, "Failed to read tool number!\n");
    exit(EXIT_FAILURE);
    return 1;
  }
  */

  /*
    gatherDeviceData();
    sleep(mScanDelay);

    connect();
    configure();
    configMacrosAndPMC();
    initializeDeviceDatum();    
    getPathData();
    getMessages();
    getMacros();
    getPMC();
    getCounts();

    loop:
    getProgramInfo() // read current block (too much)
    getStatus() // ACTIVE / STOPPED etc
    getAxisData() // position, load
    getSpindleData() // speed, load
    getToolData() // active tool, life (tool id, group)

  */
}
