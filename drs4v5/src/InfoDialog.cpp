/*
 * InfoDialog.cpp
 * Info Dialog class
 * $Id: InfoDialog.cpp 17646 2011-05-11 15:21:02Z ritt $
 */

#include "DRSOscInc.h"

extern char svn_revision[];
extern char drsosc_version[];

InfoDialog::InfoDialog(wxWindow* parent)
:
InfoDialog_fb( parent )
{
   wxString str;

   DOFrame *frame = (DOFrame*)parent;
   DRSBoard *b = frame->GetOsci()->GetCurrentBoard();

   int t = b->GetBoardType();
   if (t == 5)
      str.Printf(wxT("5 (Eval. 2.0)"));
   else if (t == 6)
      str.Printf(wxT("6 (Mezz. 1.4)"));
   else if (t == 7)
      str.Printf(wxT("7 (Eval. 3.0)"));
   else if (t == 8)
      str.Printf(wxT("8 (Eval. 4.0)"));
   else
      str.Printf(wxT("%d"), t);
   m_stBoardType->SetLabel(str);

   str.Printf(wxT("DRS%d"), b->GetDRSType());
   m_stDRSType->SetLabel(str);

   str.Printf(wxT("%d"), b->GetBoardSerialNumber());
   m_stSerialNumber->SetLabel(str);

   str.Printf(wxT("%d"), b->GetFirmwareVersion());
   m_stFirmwareRevision->SetLabel(str);

   str.Printf(wxT("%1.1lf"), b->GetTemperature());
   m_stTemperature->SetLabel(str);

   str.Printf(wxT("%1.2lgV...%1.2lgV"), b->GetInputRange()-0.5, b->GetInputRange()+0.5);
   m_stInputRange->SetLabel(str);

   str.Printf(wxT("%1.2lgV...%1.2lgV"), b->GetCalibratedInputRange()-0.5, b->GetCalibratedInputRange()+0.5);
   m_stCalibratedRange->SetLabel(str);

   str.Printf(wxT("%1.3lf GSPS"), b->GetCalibratedFrequency());
   m_stCalibratedFrequency->SetLabel(str);

   double freq;
   b->ReadFrequency(0, &freq);
   str.Printf(wxT("%1.3lf GSPS"), freq);
   m_stFrequency->SetLabel(str);
}
