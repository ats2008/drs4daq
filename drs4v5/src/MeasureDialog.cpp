/*
 * ConfigDialog.cpp
 * Modal Measurement Configuration Dialog class
 * $Id: MeasureDialog.cpp 21271 2014-02-17 15:20:55Z ritt $
 */

#include "DRSOscInc.h"

MeasureDialog::MeasureDialog( wxWindow* parent )
:
MeasureDialog_fb( parent )
{
   m_frame = (DOFrame *)parent;
}

void MeasureDialog::OnClose( wxCommandEvent& event )
{
   this->Hide();
}

void MeasureDialog::OnButton( wxCommandEvent& event )
{
   if (event.IsChecked() && event.GetId() >= ID_VS1 && event.GetId() <= ID_VS4)
      wxMessageBox(wxT("Please use cursor A to set the location of the vertical slice"),
                   wxT("DRS Oscilloscope"), wxOK | wxICON_INFORMATION, this);

   if (event.IsChecked() && event.GetId() >= ID_CHRG1 && event.GetId() <= ID_CHRG4)
      wxMessageBox(wxT("Please use cursors A and B to define the integration region"),
                   wxT("DRS Oscilloscope"), wxOK | wxICON_INFORMATION, this);

   if (event.IsChecked() && event.GetId() >= ID_HS1 && event.GetId() <= ID_HS4)
      wxMessageBox(wxT("Please use cursor A to set the location of the horizontal slice"),
                   wxT("DRS Oscilloscope"), wxOK | wxICON_INFORMATION, this);

   m_frame->SetMeasurement(event.GetId(), event.IsChecked());
}

void MeasureDialog::OnStat( wxCommandEvent& event )
{
   m_frame->SetStat(event.IsChecked());
}

void MeasureDialog::OnHist( wxCommandEvent& event )
{
   m_frame->SetHist(event.IsChecked());
}

void MeasureDialog::OnStatNAverage( wxCommandEvent& event )
{
   wxString str = m_cbNAverage->GetValue();
   char buf[100];
   strcpy( buf, (const char*)str.mb_str(wxConvUTF8) );
   m_frame->SetStatNStat(atoi(buf));
}

void MeasureDialog::OnIndicator( wxCommandEvent& event )
{
   m_frame->SetIndicator(event.IsChecked());
}

void MeasureDialog::OnStatReset( wxCommandEvent& event )
{
   m_frame->StatReset();
}
