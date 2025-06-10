#ifndef SIPMCONTROL_H_
#define SIPMCONTROL_H_

int gui_set(int boardNum, int chanNum, double desVolt);
int gui_monitor(int boardNum, int chanNum, double *current, double *refVolt, int *nakFlag);

#endif // SIPMCONTROL_H_
