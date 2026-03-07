float curR_SimThrs   = 0.0;
float curY_SimThrs   = 0.0;
float curBM1_SimThrs = 0.0;
float curBM2_SimThrs = 0.0;

int volRY_SimThrs = 0;
int volYB_SimThrs = 0;
int volBR_SimThrs = 0;

void Simulation() {
#if defined(SIMULTION)

  volRY = random((long)(storage.undVol+10), (long)(storage.ovrVol-10)) + volRY_SimThrs;
  volYB = random((long)(storage.undVol+10), (long)(storage.ovrVol-10)) + volYB_SimThrs;
  volBR = random((long)(storage.undVol+10), (long)(storage.ovrVol-10)) + volBR_SimThrs;

  if (motSimulation) {
    curR   = (float)random((long)(storage.dryRunM1+1), (long)(storage.ovLRunM1-1)) + curR_SimThrs;
    curY   = (float)random((long)(storage.dryRunM1+1), (long)(storage.ovLRunM1-1)) + curY_SimThrs;
    curBM1 = (float)random((long)(storage.dryRunM1+1), (long)(storage.ovLRunM1-1)) + curBM1_SimThrs;
    curBM2 = (float)random((long)(storage.dryRunM1+1), (long)(storage.ovLRunM1-1)) + curBM2_SimThrs;
  } else {
    curR = 0.0; curY = 0.0; curBM1 = 0.0; curBM2 = 0.0;
  }

#endif
}
