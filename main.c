#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "./fwlib/fwlib32.h"

int main() {
  int ret;
  unsigned short mFlibHndl;

  char mDeviceIP[] = "127.0.0.1";
  int mDevicePort = 8193;


  ret = cnc_startupprocess(3, "focas.log");
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to create log file!\n");
    return 1;
  }

  ret = cnc_allclibhndl3(mDeviceIP, mDevicePort, 10, &mFlibHndl);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to connect to cnc!\n");
    return 1;
  }

  ODBSYS sysinfo;
  ret = cnc_sysinfo(mFlibHndl, &sysinfo);
  if (ret != EW_OK) {
    fprintf(stderr, "failed to get cnc sysinfo!\n");
    return 1;
  }
  printf("addinfo:  %d\n", sysinfo.addinfo);
  printf("max_axis: %d\n", sysinfo.max_axis);
  printf("cnc_type: %.*s\n", sizeof(sysinfo.cnc_type), sysinfo.cnc_type);
  printf("mt_type:  %.*s\n", sizeof(sysinfo.mt_type), sysinfo.mt_type);
  printf("series:   %.*s\n", sizeof(sysinfo.series), sysinfo.series);
  printf("version:  %.*s\n", sizeof(sysinfo.version), sysinfo.version);
  printf("axes:     %.*s\n", sizeof(sysinfo.axes), sysinfo.axes);

  struct iodbpsd param;
  int num = 6711;

  ret = cnc_rdparam(mFlibHndl, num, -1, 8, &param);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to read part parameter! %d\n", num);
  }
  printf("size: %d\n", sizeof(param.u.ldata));
  printf("part count: %ld\n", param.u.ldata);

  struct odbtlife4 toolId2;

  ret = cnc_toolnum(mFlibHndl, 0, 0, &toolId2);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to read tool number!\n");
    return 1;
  }

  struct odbtlife3 toolId;
  ret = cnc_rdntool(mFlibHndl, 0, &toolId);
  if (ret != EW_OK) {
    fprintf(stderr, "Failed to read tool number!\n");
    return 1;
  }

  ret = cnc_freelibhndl(mFlibHndl);
  if (ret != EW_OK) {
      fprintf(stderr, "Failed to free library handle!\n");
      return 1;
  }
  return 0;
}