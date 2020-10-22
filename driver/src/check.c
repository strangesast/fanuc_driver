#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../external/cJSON/cJSON.h"
#include "./common.h"
#include "./data.h"

extern char deviceIP[MAXPATH];
extern char deviceID[MAXLEN];
extern short programNum;
extern long partCount;

int checkMachineInfo(cJSON *updates, cJSON *meta) {
  static MachineInfo *lv = NULL;

  MachineInfo *v = malloc(sizeof *v);

  // check new value
  if (getMachineInfo(v)) {
    free(v);
    fprintf(stderr, "failed to get machine info\n");
    return 1;
  }

  if (lv == NULL || lv->id != v->id) {
    cJSON *id_datum = cJSON_CreateString(v->id);
    cJSON_AddItemToObject(updates, "id", id_datum);
    memset(deviceID, 0, MAXLEN);
    strncpy(deviceID, v->id, MAXLEN - 1);
  }
  if (lv == NULL || lv->max_axis != v->max_axis) {
    cJSON *max_axis_datum = cJSON_CreateNumber(v->max_axis);
    cJSON_AddItemToObject(updates, "max_axis", max_axis_datum);
  }
  if (lv == NULL || lv->addinfo != v->addinfo) {
    cJSON *addinfo_datum = cJSON_CreateNumber(v->addinfo);
    cJSON_AddItemToObject(updates, "addinfo", addinfo_datum);
  }
  if (lv == NULL || lv->cnc_type != v->cnc_type) {
    cJSON *cnc_type_datum = cJSON_CreateString(v->cnc_type);
    cJSON_AddItemToObject(updates, "cnc_type", cnc_type_datum);
  }
  if (lv == NULL || lv->mt_type != v->mt_type) {
    cJSON *mt_type_datum = cJSON_CreateString(v->mt_type);
    cJSON_AddItemToObject(updates, "mt_type", mt_type_datum);
  }
  if (lv == NULL || lv->series != v->series) {
    cJSON *series_datum = cJSON_CreateString(v->series);
    cJSON_AddItemToObject(updates, "series", series_datum);
  }
  if (lv == NULL || lv->version != v->version) {
    cJSON *version_datum = cJSON_CreateString(v->version);
    cJSON_AddItemToObject(updates, "version", version_datum);
  }
  if (lv == NULL || lv->axes_count_chk != v->axes_count_chk) {
    cJSON *axes_count_chk_datum = cJSON_CreateString(v->axes_count_chk);
    cJSON_AddItemToObject(updates, "axes_count_chk", axes_count_chk_datum);
  }
  if (lv == NULL || lv->axes_count != v->axes_count) {
    cJSON *axes_count_datum = cJSON_CreateNumber(v->axes_count);
    cJSON_AddItemToObject(updates, "axes_count", axes_count_datum);
  }
  if (lv == NULL || lv->etherType != v->etherType) {
    cJSON *ether_type_datum = cJSON_CreateNumber(v->etherType);
    cJSON_AddItemToObject(updates, "ether_type", ether_type_datum);
  }
  if (lv == NULL || lv->etherDevice != v->etherDevice) {
    cJSON *ether_device_datum = cJSON_CreateNumber(v->etherDevice);
    cJSON_AddItemToObject(updates, "ether_device", ether_device_datum);
  }
  bool atLeastOneAxesChanged = false;
  cJSON *axes_datum = cJSON_CreateArray();
  for (int i = 0; i < v->axes_count; i++) {
    cJSON *axis_datum = cJSON_CreateObject();
    if (lv == NULL || lv->axes[i].id != v->axes[i].id) {
      cJSON *axis_id_datum = cJSON_CreateString(v->axes[i].id);
      cJSON_AddItemToObject(axis_datum, "id", axis_id_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || lv->axes[i].index != v->axes[i].index) {
      cJSON *axis_index_datum = cJSON_CreateNumber(v->axes[i].index);
      cJSON_AddItemToObject(axis_datum, "index", axis_index_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || strncmp(lv->axes[i].suffix, v->axes[i].suffix, 2) != 0) {
      cJSON *axis_suffix_datum = cJSON_CreateString(v->axes[i].suffix);
      cJSON_AddItemToObject(axis_datum, "suffix", axis_suffix_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || lv->axes[i].divisor != v->axes[i].divisor) {
      cJSON *axis_divisor_datum = cJSON_CreateNumber(v->axes[i].divisor);
      cJSON_AddItemToObject(axis_datum, "divisor", axis_divisor_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || strncmp(lv->axes[i].name, v->axes[i].name, 5) != 0) {
      cJSON *axis_name_datum = cJSON_CreateString(v->axes[i].name);
      cJSON_AddItemToObject(axis_datum, "name", axis_name_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || lv->axes[i].flag != v->axes[i].flag) {
      cJSON *axis_flag_datum = cJSON_CreateNumber(v->axes[i].flag);
      cJSON_AddItemToObject(axis_datum, "flag", axis_flag_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || lv->axes[i].unit != v->axes[i].unit) {
      cJSON *axis_unit_datum = cJSON_CreateNumber(v->axes[i].unit);
      cJSON_AddItemToObject(axis_datum, "unit", axis_unit_datum);
      cJSON *axis_unith_datum = cJSON_CreateString(v->axes[i].unith);
      cJSON_AddItemToObject(axis_datum, "unith", axis_unith_datum);
      atLeastOneAxesChanged = true;
    }
    if (lv == NULL || lv->axes[i].decimal != v->axes[i].decimal) {
      cJSON *axis_decimal_datum = cJSON_CreateNumber(v->axes[i].decimal);
      cJSON_AddItemToObject(axis_datum, "decimal", axis_decimal_datum);
      atLeastOneAxesChanged = true;
    }
    cJSON_AddItemToArray(axes_datum, axis_datum);
  }
  if (atLeastOneAxesChanged) {
    cJSON_AddItemToObject(updates, "axes", axes_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "info", ed_meta_datum);

  if (lv != NULL) {
    free(lv);  // free old version
  }
  lv = v;  // update to new version
  return 0;
}

int checkMachinePartCount(cJSON *updates, cJSON *meta) {
  static MachinePartCount *lv = NULL;

  MachinePartCount *v = malloc(sizeof *v);

  if (getMachinePartCount(v)) {
    fprintf(stderr, "failed to read machine part count\n");
    return 1;
  }

  if (lv == NULL || (lv->count != v->count)) {
    partCount = v->count;
    cJSON *count_datum = cJSON_CreateNumber(v->count);
    cJSON_AddItemToObject(updates, "part_count", count_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "part_count", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }

  lv = v;

  return 0;
}

int checkMachineCycleTime(cJSON *updates, cJSON *meta) {
  static MachineCycleTime *lv = NULL;
  static long lastPartCount = -1;

  if (lastPartCount != partCount) {
    MachineCycleTime *v = malloc(sizeof *v);

    if (getMachineCycleTime(v)) {
      fprintf(stderr, "failed to read machine cycle time\n");
      return 1;
    }

    cJSON *cycle_time_datum = cJSON_CreateNumber(v->time);
    cJSON_AddItemToObject(updates, "cycle_time", cycle_time_datum);

    cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
    cJSON_AddItemToObject(meta, "cycle_time", ed_meta_datum);

    if (lv != NULL) {
      free(lv);
    }

    lastPartCount = partCount;
    lv = v;
  }

  return 0;
}

int checkMachineProgramName(cJSON *updates, cJSON *meta) {
  static MachineProgramName *lv = NULL;

  MachineProgramName *v = malloc(sizeof *v);

  if (getMachineProgramName(v)) {
    free(v);
    fprintf(stderr, "failed to read machine program name\n");
    return 1;
  }

  if (lv == NULL || lv->number != v->number) {
    cJSON *program_number_datum = cJSON_CreateNumber(v->number);
    cJSON_AddItemToObject(updates, "program_number_alt", program_number_datum);
  }
  if (lv == NULL || strncmp(lv->name, v->name, 36) != 0) {
    cJSON *program_name_datum;
    program_name_datum = cJSON_CreateString(v->name);
    cJSON_AddItemToObject(updates, "program_name_alt", program_name_datum);
  }
  if (lv == NULL || strncmp(lv->path, v->path, 256) != 0) {
    cJSON *program_path_datum;
    program_path_datum = cJSON_CreateString(v->path);
    cJSON_AddItemToObject(updates, "program_path_alt", program_path_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "program_name", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}

int checkMachineMessage(cJSON *updates, cJSON *meta) {
  static MachineMessage *lv = NULL;

  MachineMessage *v = malloc(sizeof *v);

  if (getMachineMessage(v)) {
    free(v);
    fprintf(stderr, "failed to read machine message\n");
    return 1;
  }

  if (lv == NULL || lv->number != v->number) {
    cJSON *message_number_datum = cJSON_CreateNumber(v->number);
    cJSON_AddItemToObject(updates, "message_number", message_number_datum);
  }
  if (lv == NULL || strncmp(lv->text, v->text, 256) != 0) {
    cJSON *message_text_datum;
    if (v->number != -1) {
      message_text_datum = cJSON_CreateString(v->text);
    } else {
      message_text_datum = cJSON_CreateNull();
    }
    cJSON_AddItemToObject(updates, "message_text", message_text_datum);
  }
  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "message", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}

int checkMachineStatus(cJSON *updates, cJSON *meta) {
  static MachineStatus *lv = NULL;

  MachineStatus *v = malloc(sizeof *v);

  if (getMachineStatus(v)) {
    free(v);
    fprintf(stderr, "failed to read machine status\n");
    return 1;
  }
  if (lv == NULL || strcmp(lv->execution, v->execution) != 0) {
    cJSON *execution_datum = cJSON_CreateString(v->execution);
    cJSON_AddItemToObject(updates, "execution", execution_datum);
  }
  if (lv == NULL || strcmp(lv->mode, v->mode) != 0) {
    cJSON *mode_datum = cJSON_CreateString(v->mode);
    cJSON_AddItemToObject(updates, "mode", mode_datum);
  }
  if (lv == NULL || strcmp(lv->mode, v->mode) != 0) {
    cJSON *estop_datum = cJSON_CreateString(v->estop);
    cJSON_AddItemToObject(updates, "estop", estop_datum);
  }
  if (lv == NULL || lv->raw.alarm != v->raw.alarm) {
    cJSON *alarm_datum = cJSON_CreateNumber(v->raw.alarm);
    cJSON_AddItemToObject(updates, "alarm", alarm_datum);
  }
  if (lv == NULL || lv->raw.aut != v->raw.aut) {
    cJSON *aut_datum = cJSON_CreateNumber(v->raw.aut);
    cJSON_AddItemToObject(updates, "aut", aut_datum);
  }
  if (lv == NULL || lv->raw.edit != v->raw.edit) {
    cJSON *edit_datum = cJSON_CreateNumber(v->raw.edit);
    cJSON_AddItemToObject(updates, "edit", edit_datum);
  }
  if (lv == NULL || lv->raw.emergency != v->raw.emergency) {
    cJSON *emergency_datum = cJSON_CreateNumber(v->raw.emergency);
    cJSON_AddItemToObject(updates, "emergency", emergency_datum);
  }
  if (lv == NULL || lv->raw.hdck != v->raw.hdck) {
    cJSON *hdck_datum = cJSON_CreateNumber(v->raw.hdck);
    cJSON_AddItemToObject(updates, "hdck", hdck_datum);
  }
  if (lv == NULL || lv->raw.motion != v->raw.motion) {
    cJSON *motion_datum = cJSON_CreateNumber(v->raw.motion);
    cJSON_AddItemToObject(updates, "motion", motion_datum);
  }
  if (lv == NULL || lv->raw.mstb != v->raw.mstb) {
    cJSON *mstb_datum = cJSON_CreateNumber(v->raw.mstb);
    cJSON_AddItemToObject(updates, "mstb", mstb_datum);
  }
  if (lv == NULL || lv->raw.run != v->raw.run) {
    cJSON *run_datum = cJSON_CreateNumber(v->raw.run);
    cJSON_AddItemToObject(updates, "run", run_datum);
  }
  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "status", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}

int checkMachineDynamic(cJSON *updates, cJSON *meta) {
  static MachineDynamic *lv = NULL;

  MachineDynamic *v = malloc(sizeof *v);

  if (getMachineDynamic(v)) {
    free(v);
    fprintf(stderr, "failed to read machine dynamic\n");
    return 1;
  }

  cJSON *absolute_datum;
  cJSON *relative_datum;
  cJSON *actual_datum;
  cJSON *load_datum;
  // false by default?
  bool abs = false, rel = false, act = false, load = false;

  for (int i = 0; i < v->dim; i++) {
    if (lv == NULL || lv->absolute[i] != v->absolute[i]) {
      abs = true;
    }
    if (lv == NULL || lv->relative[i] != v->relative[i]) {
      rel = true;
    }
    if (lv == NULL || lv->actual[i] != v->actual[i]) {
      act = true;
    }
    if (lv == NULL || lv->load[i] != v->load[i]) {
      load = true;
    }
  }
  if (abs) {
    absolute_datum = cJSON_CreateArray();
  }
  if (rel) {
    relative_datum = cJSON_CreateArray();
  }
  if (act) {
    actual_datum = cJSON_CreateArray();
  }
  if (load) {
    load_datum = cJSON_CreateArray();
  }
  for (int i = 0; i < v->dim; i++) {
    if (abs) {
      cJSON *d = cJSON_CreateNumber(v->absolute[i]);
      cJSON_AddItemToArray(absolute_datum, d);
    }
    if (rel) {
      cJSON *d = cJSON_CreateNumber(v->relative[i]);
      cJSON_AddItemToArray(relative_datum, d);
    }
    if (act) {
      cJSON *d = cJSON_CreateNumber(v->actual[i]);
      cJSON_AddItemToArray(actual_datum, d);
    }
    if (load) {
      cJSON *d = cJSON_CreateNumber(v->load[i]);
      cJSON_AddItemToArray(load_datum, d);
    }
  }
  if (abs) {
    cJSON_AddItemToObject(updates, "absolute", absolute_datum);
  }
  if (rel) {
    cJSON_AddItemToObject(updates, "relative", relative_datum);
  }
  if (act) {
    cJSON_AddItemToObject(updates, "actual", actual_datum);
  }
  if (load) {
    cJSON_AddItemToObject(updates, "load", load_datum);
  }
  // current program
  if (lv == NULL || lv->cprogram != v->cprogram) {
    programNum = v->cprogram;
    cJSON *cprogram_datum = cJSON_CreateNumber(v->cprogram);
    cJSON_AddItemToObject(updates, "cprogram", cprogram_datum);
  }
  // main program
  if (lv == NULL || lv->mprogram != v->mprogram) {
    cJSON *mprogram_datum = cJSON_CreateNumber(v->mprogram);
    cJSON_AddItemToObject(updates, "mprogram", mprogram_datum);
  }
  // line no
  if (lv == NULL || lv->sequence != v->sequence) {
    cJSON *sequence_datum = cJSON_CreateNumber(v->sequence);
    cJSON_AddItemToObject(updates, "sequence", sequence_datum);
  }
  // actual feedrate
  if (lv == NULL || lv->actf != v->actf) {
    cJSON *actf_datum = cJSON_CreateNumber(v->actf);
    cJSON_AddItemToObject(updates, "actf", actf_datum);
  }
  // actual spindle speed
  if (lv == NULL || lv->acts != v->acts) {
    cJSON *acts_datum = cJSON_CreateNumber(v->acts);
    cJSON_AddItemToObject(updates, "acts", acts_datum);
  }
  // alarm status
  if (lv == NULL || lv->alarm != v->alarm) {
    cJSON *alarm_datum = cJSON_CreateNumber(v->alarm);
    cJSON_AddItemToObject(updates, "alarm", alarm_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "dynamic", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}

int checkMachineToolInfo(cJSON *updates, cJSON *meta) {
  static MachineToolInfo *lv = NULL;

  MachineToolInfo *v = malloc(sizeof *v);

  if (getMachineToolInfo(v)) {
    fprintf(stderr, "failed to read machine tool info\n");
    return 1;
  }

  if (lv == NULL || lv->id != v->id) {
    cJSON *tool_id_datum = cJSON_CreateNumber(v->id);
    cJSON_AddItemToObject(updates, "tool_id", tool_id_datum);
  }
  if (lv == NULL || lv->group != v->group) {
    cJSON *tool_group_datum = cJSON_CreateNumber(v->group);
    cJSON_AddItemToObject(updates, "tool_group", tool_group_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "tool", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}

int checkMachineProgram(cJSON *updates, cJSON *meta) {
  static MachineProgram *lv = NULL;
  static short lastProgramNum = 0;

  if (lastProgramNum != programNum) {
    MachineProgram *v = malloc(sizeof *v);

    if (getMachineProgram(v, programNum)) {
      free(v);
      fprintf(stderr, "failed to check machine program: %d\n", programNum);
      return 1;
    }

    cJSON *number_datum = cJSON_CreateNumber(v->number);
    cJSON_AddItemToObject(updates, "program_number", number_datum);

    cJSON *header_datum = cJSON_CreateString(v->header);
    cJSON_AddItemToObject(updates, "program_header", header_datum);

    cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
    cJSON_AddItemToObject(meta, "program", ed_meta_datum);

    if (lv != NULL) {
      free(lv);
    }

    lv = v;
    lastProgramNum = programNum;
  }
  return 0;
}

int checkMachineBlock(cJSON *updates, cJSON *meta) {
  static MachineBlock *lv = NULL;

  MachineBlock *v = malloc(sizeof *v);

  if (getMachineBlock(v)) {
    fprintf(stderr, "failed to read machine block\n");
    return 1;
  }

  if (lv == NULL || strcmp(lv->block, v->block) != 0) {
    cJSON *block_datum = cJSON_CreateString(v->block);
    cJSON_AddItemToObject(updates, "block", block_datum);
  }

  if (lv == NULL || lv->blkNum != v->blkNum) {
    cJSON *block_num_datum = cJSON_CreateNumber(v->blkNum);
    cJSON_AddItemToObject(updates, "block_num", block_num_datum);
  }

  cJSON *ed_meta_datum = cJSON_CreateNumber(v->executionDuration);
  cJSON_AddItemToObject(meta, "block", ed_meta_datum);

  if (lv != NULL) {
    free(lv);
  }
  lv = v;

  return 0;
}
