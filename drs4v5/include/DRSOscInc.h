/*
 * DRSOscInc.h
 * Collection of all DRS oscilloscope header include files
 * $Id: DRSOscInc.h 20924 2013-03-21 13:31:33Z ritt $
 */

#define MAX_N_BOARDS 16

#include "wx/wx.h"
#include "wx/dcbuffer.h"
#include "wx/print.h"
#include "wx/numdlg.h"
#include "mxml.h"
#include "DRS.h"
#include "DRSOsc.h"
#include "Osci.h"
#include "Measurement.h"
#include "ConfigDialog.h"
#include "DisplayDialog.h"
#include "AboutDialog.h"
#include "InfoDialog.h"
#include "MeasureDialog.h"
#include "TriggerDialog.h"
#include "DOScreen.h"
#include "DOFrame.h"
#include "EPThread.h"

double ss_nan();
int ss_isnan(double x);
void ss_sleep(int ms);



