int curR_SimThrs = 0;
int curY_SimThrs = 0;
int curBM1_SimThrs = 0;

int volRY_SimThrs = 0;
int volYB_SimThrs = 0;
int volBR_SimThrs = 0;

void Simulation() {
#if defined(SIMULTION)

  volRY = random(storage.undVol+10, storage.ovrVol-10)+volRY_SimThrs;
  volYB = random(storage.undVol+10, storage.ovrVol-10)+volYB_SimThrs;
  volBR = random(storage.undVol+10, storage.ovrVol-10)+volBR_SimThrs;

  if (motSimulation) {
    curR = random(storage.dryRunM1 +1, storage.ovLRunM1-1) + curR_SimThrs;
    curY = random(storage.dryRunM1 +1, storage.ovLRunM1-1) + curY_SimThrs;
    curBM1 =random(storage.dryRunM1 +1, storage.ovLRunM1-1) + curBM1_SimThrs;
  }

#endif
}
