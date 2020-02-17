/*
 * ConfigDialog.cpp
 * Modeless Configuration Dialog class
 * $Id: ConfigDialog.cpp 22325 2016-10-07 14:05:49Z ritt $
 */

#include "DRSOscInc.h"

ConfigDialog::ConfigDialog( wxWindow* parent )
:
ConfigDialog_fb( parent )
{
   m_frame = (DOFrame *)parent;
   m_osci  = m_frame->GetOsci();

   fCalMode       = 0;
   m_board        = 0;
   m_firstChannel = 0;
   m_chnSection   = m_frame->GetOsci()->GetChnSection();
   
   if (m_frame->GetMultiBoard()) {
      m_cbMulti->SetValue(true);
      m_cbTrgCorr->SetValue(false);
      m_cbTrgCorr->Enable(false);
      m_frame->SetDisplayTrgCorr(false);
   }

   m_cbClkOn->SetValue(m_frame->GetClkOn());
   if (m_osci->GetNumberOfBoards() == 0) {
      m_cbClkOn->SetLabel(wxT("Connect reference clock to channel #4"));
   } else {
      if (m_frame->GetOsci()->GetBoard(0)->GetBoardType() == 9)
         m_cbClkOn->SetLabel(wxT("Connect reference clock to all channels"));
      else
         m_cbClkOn->SetLabel(wxT("Connect reference clock to channel #4"));
   }
   
   if (m_frame->GetRange() == 0) {
      m_rbRange->SetSelection(0);
      m_slCal->SetRange(-500, 500);
   } else if (m_frame->GetRange() == 0.45) {
      m_rbRange->SetSelection(1);
      m_slCal->SetRange(-50, 950);
   } else if (m_frame->GetRange() == 0.5) {
      m_rbRange->SetSelection(2);
      m_slCal->SetRange(-0, 1000);
   }

   if (m_chnSection == 2)
      m_rbChHalf->SetSelection(2);

   wxString wxstr;
   wxstr.Printf(wxT("%1.4lg"), m_frame->GetReqSamplingSpeed());
   m_tbFreq->SetValue(wxstr);
   wxstr.Printf(wxT("%1.4lg GSPS"), m_frame->GetActSamplingSpeed());
   m_stActFreq->SetLabel(wxstr);

   m_cbLocked->SetValue(m_frame->IsFreqLocked());
   m_cbSpikes->SetValue(m_frame->GetSpikeRemovel());

   if (m_osci->GetNumberOfBoards() == 0) {
      m_cbTCalOn->SetValue(true);
      m_cbTCalOn->Enable(true);
   } else {
      m_cbTCalOn->SetValue(m_frame->GetOsci()->IsTCalibrated());
      m_cbTCalOn->Enable(m_frame->GetOsci()->IsTCalibrated());
   }

   PopulateBoards();
}

void ConfigDialog::PopulateBoards()
{
   wxString wxstr;

   m_cbBoard->Clear();

   for (int i=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
      DRSBoard *b = m_osci->GetBoard(i);

#ifdef HAVE_VME
      if (b->GetTransport() == 1)
         wxstr.Printf(wxT("VME DRS%d slot %2d%s serial %d"), 
            b->GetDRSType(), (b->GetSlotNumber() >> 1)+2, 
            ((b->GetSlotNumber() & 1) == 0) ? "up" : "lo", 
            b->GetBoardSerialNumber());
      else
#endif
         wxstr.Printf(wxT("USB DRS%d serial %d"), b->GetDRSType(), b->GetBoardSerialNumber());

      m_cbBoard->Append(wxstr);
   }

   m_cbBoard->Select(m_board);
   UpdateControls();
}

void ConfigDialog::UpdateControls()
{
   if (m_osci->GetNumberOfBoards() < 2) {
      m_cbMulti->Enable(false);
   } else
      m_cbMulti->Enable(true);
   
   if (m_osci->GetNumberOfBoards() == 0 || 
       m_osci->GetBoard(m_board)->GetBoardType() == 5 ||
       m_osci->GetBoard(m_board)->GetBoardType() == 7 ||
       m_osci->GetBoard(m_board)->GetBoardType() == 8 ||
       m_osci->GetBoard(m_board)->GetBoardType() == 9) {
      if (m_osci->GetNumberOfBoards() > 0) {
         m_cbExtRefclk->Enable(m_osci->GetBoard(m_board)->GetBoardType() == 8 || m_osci->GetBoard(m_board)->GetBoardType() == 9);
         m_cbExtRefclk->Show(m_osci->GetBoard(m_board)->GetBoardType() == 8 || m_osci->GetBoard(m_board)->GetBoardType() == 9);
      }
   } else {
      m_cbExtRefclk->Enable(true);
      m_cbExtRefclk->Show(true);
   }

   if (m_osci->GetNumberOfBoards() > 0) {
      if (m_osci->GetBoard(m_board)->GetBoardType() == 5 ||
          m_osci->GetBoard(m_board)->GetBoardType() == 6 ||
          m_osci->GetBoard(m_board)->Is2048ModeCapable()) {
         m_rbChHalf->Enable(true);
      } else {
         if (m_osci->GetBoard(m_board)->GetBoardSerialNumber() == 2146 ||
             m_osci->GetBoard(m_board)->GetBoardSerialNumber() == 2205 ||
             m_osci->GetBoard(m_board)->GetBoardSerialNumber() == 2208 ||
             m_osci->GetBoard(m_board)->GetBoardSerialNumber() == 2253 ||
             m_osci->GetBoard(m_board)->GetBoardSerialNumber() == 2287) {
            m_rbChHalf->Enable(true);  // special boards modified for RFBeta & Slow Muons
         } else {
            m_rbChHalf->Enable(false);
         }
      }
   } else {
      m_rbChHalf->Enable(false);
   }

   if (m_osci->GetNumberOfBoards() == 0) {
      m_cbTCalOn->SetValue(true);
      m_cbTCalOn->Enable(true);
   } else {
      m_cbCalibrated->SetValue(m_frame->GetOsci()->IsVCalibrated());
      m_cbCalibrated->Enable(m_frame->GetOsci()->IsVCalibrated());
      m_cbCalibrated2->SetValue(m_frame->GetOsci()->IsVCalibrated());
      m_cbCalibrated2->Enable(m_frame->GetOsci()->IsVCalibrated());

      m_cbTCalOn->SetValue(m_frame->GetOsci()->IsTCalibrated());
      m_cbTCalOn->Enable(m_frame->GetOsci()->IsTCalibrated());
      m_cbExtRefclk->SetValue(m_osci->GetBoard(m_board)->GetRefclk() == 1);

      if ((m_osci->GetBoard(m_board)->GetBoardType() == 8 || m_osci->GetBoard(m_board)->GetBoardType() == 9)
           && (!m_osci->IsMultiBoard() || m_board == 0))
         m_frame->EnableTriggerConfig(true);
      else
         m_frame->EnableTriggerConfig(false);
   }
}

void ConfigDialog::OnBoardSelect( wxCommandEvent& event )
{
   if (event.GetId() == ID_MULTI) {
      if (m_cbMulti->IsChecked()) {
         wxString str;
         
         str.Printf(wxT("In a multi-board configuration, the Trigger and Clock singals must be conected. Please read the manual for details. Turn on multi-board mode?"));
         
         if (wxMessageBox(str, wxT("DRS Oscilloscope Info"), wxOK | wxCANCEL | wxICON_EXCLAMATION) == wxOK) {
            m_osci->Enable(false);
            m_osci->SetMultiBoard(true);
            for (int i=1 ; i<m_osci->GetNumberOfBoards() ; i++) {
               DRSBoard *b = m_frame->GetOsci()->GetBoard(i);
               m_frame->SetTriggerSource(i, 4);       // select external trigger
               m_frame->SetTriggerPolarity(i, false); // positive trigger

               if (b->GetFirmwareVersion() < 21260) {
                  wxMessageBox(wxT("For this operation V5 boards with firmware revision >= 21260 is required"),
                               wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
               } else {
                  if (b->GetScaler(5) < 300000) {
                     str.Printf(wxT("No clock signal connected to CLK IN of board #%d"), i);
                     wxMessageBox(str, wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
                     m_frame->SetRefclk(i, false);
                  } else {
                     m_frame->SetRefclk(i, true);
                  }
               }
            }
            m_osci->Enable(true);
            m_osci->SelectBoard(m_board);
            m_osci->SelectChannel(m_firstChannel, m_chnSection);
            m_frame->SetDisplayTrgCorr(false);
            m_cbTrgCorr->SetValue(false);
            m_cbTrgCorr->Enable(false);
         } else {
            m_cbMulti->SetValue(false);
         }
      } else {
         m_osci->Enable(false);
         m_osci->SetMultiBoard(false);
         m_osci->Enable(true);
         m_osci->SelectBoard(m_board);
         m_osci->SelectChannel(m_firstChannel, m_chnSection);
         m_cbTrgCorr->Enable(true);
      }
      m_frame->SetMultiBoard(m_cbMulti->IsChecked());
      m_frame->SelectBoard(m_board); // cause control update
      if (!m_cbMulti->IsChecked())
         m_frame->SetSplitMode(false);
      
   } else {
      m_board = m_cbBoard->GetSelection();
      m_osci->SelectBoard(m_board);
      m_osci->SelectChannel(m_firstChannel, m_chnSection);
      m_frame->SelectBoard(m_board);
      UpdateControls();
   }
}

// called from DOFrame if one selects another board
void ConfigDialog::SelectBoard(int i)
{
   m_board = i;
   m_cbBoard->SetSelection(i);
   m_osci->SelectBoard(m_board);
   m_osci->SelectChannel(m_firstChannel, m_chnSection);
   m_frame->UpdateStatusBar();
   UpdateControls();
}


void ConfigDialog::OnRescan( wxCommandEvent& event )
{
   m_frame->EnableEPThread(false);
   m_osci->ScanBoards();
   PopulateBoards();
   if (m_board >= m_osci->GetNumberOfBoards())
      m_board = m_firstChannel = m_chnSection = 0;

   m_osci->SelectBoard(m_board);
   m_frame->EnableEPThread(true);
   m_frame->UpdateStatusBar();
}

void ConfigDialog::OnInfo( wxCommandEvent& event )
{
   InfoDialog id(m_frame);
   id.ShowModal();
}

void ConfigDialog::OnChannelHalf( wxCommandEvent& event )
{
   if (event.GetId() == ID_CH_HALF)
      m_chnSection = m_rbChHalf->GetSelection();

   m_frame->SetSource(m_board, m_firstChannel, m_chnSection);
   m_osci->SelectBoard(m_board);
   m_osci->SelectChannel(m_firstChannel, m_chnSection);
}

void ConfigDialog::OnInputRange( wxCommandEvent& event )
{
   if (m_rbRange->GetSelection() == 0) {
      m_frame->GetOsci()->SetInputRange(0);
      m_frame->SetRange(0);
      m_slCal->SetRange(-500, 500);
   } else if (m_rbRange->GetSelection() == 1) {
      m_frame->GetOsci()->SetInputRange(0.45);
      m_frame->SetRange(0.45);
      m_slCal->SetRange(-50, 950);
   } else if (m_rbRange->GetSelection() == 2) {
      m_frame->GetOsci()->SetInputRange(0.5);
      m_frame->SetRange(0.5);
      m_slCal->SetRange(0, 1000);
   }
   OnCalEnter(event);
}

void ConfigDialog::OnCalOn( wxCommandEvent& event )
{
   if (event.IsChecked()) {
      m_frame->GetOsci()->SetCalibVoltage(true, m_slCal->GetValue()/1000.0);
   } else {
      m_frame->GetOsci()->SetCalibVoltage(false, 0);
   }
}

void ConfigDialog::OnCalEnter( wxCommandEvent& event )
{
   if (!m_teCal->IsEmpty()) {
      long value;
      m_teCal->GetValue().ToLong(&value);
      if (m_frame->GetRange() == 0) {
         if (value < -500)
            value = -500;
         if (value > 500)
            value = 500;
      } else if (m_frame->GetRange() == 0.45) {
         if (value < -50)
            value = -50;
         if (value > 950)
            value = 950;
      } else if (m_frame->GetRange() == 0.5) {
         if (value < 0)
            value = 0;
         if (value > 1000)
            value = 1000;
      }
      m_slCal->SetValue(value);
      m_teCal->SetValue(wxString::Format(wxT("%ld"), value));
      if (m_cbCalOn->IsChecked())
         m_frame->GetOsci()->SetCalibVoltage(true, value/1000.0);
   }

   /* check for calibration */
   if (m_osci->GetNumberOfBoards() > 0 &&
       fabs(m_frame->GetRange() - m_frame->GetOsci()->GetCalibratedInputRange()) > 0.001) {
      wxString str;

      str.Printf(wxT("This board was calibrated for an input range of\n   %1.2lg V ... %1.2lg V\nYou must execute a new voltage calibration to use this board for the new input range"), 
         m_frame->GetOsci()->GetCalibratedInputRange()-0.5, m_frame->GetOsci()->GetCalibratedInputRange()+0.5);
      wxMessageBox(str, wxT("DRS Oscilloscope Warning"), wxOK | wxICON_EXCLAMATION, this);
   }
}

void ConfigDialog::OnCalSlider( wxScrollEvent& event )
{
   m_teCal->SetValue(wxString::Format(wxT("%d"), m_slCal->GetValue()));

   if (m_cbCalOn->IsChecked())
      m_frame->GetOsci()->SetCalibVoltage(true, m_slCal->GetValue()/1000.0);

}

void ConfigDialog::Progress(int prog)
{
   if (fCalMode == 1)
      m_gaugeCalVolt->SetValue(prog);
   else {
      m_gaugeCalTime->SetValue(prog);
      m_frame->SetProgress(prog);
      m_frame->Refresh();
      m_frame->Update();
   }

   /* produces flickers with V 2.9.2
   this->Refresh();
   this->Update(); */
}

void ConfigDialog::OnButtonCalVolt( wxCommandEvent& event )
{
   fCalMode = 1;
   if (m_frame->GetOsci()->GetNumberOfBoards()) {

      m_frame->GetTimer()->Stop();
      m_frame->GetOsci()->Enable(false); // turn off readout thread

      DRSBoard *b = m_frame->GetOsci()->GetCurrentBoard();

      if (b->GetTransport() == TR_USB2 && b->GetBoardType() == 6) {
         wxMessageBox(wxT("Voltage calibration not possible with Mezzanine Board through USB"),
                         wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, this);
         return;
      }

      wxMessageBox(wxT("Please disconnect any signal from input to continue calibration"),
                      wxT("DRS Oscilloscope Info"), wxOK | wxICON_INFORMATION, this);
      m_frame->Refresh();
      m_frame->Update();

      /* remember current settings */
      double acalVolt   = b->GetAcalVolt();
      int    acalMode   = b->GetAcalMode();
      int    tcalFreq   = b->GetTcalFreq();
      int    tcalLevel  = b->GetTcalLevel();
      int    tcalSource = b->GetTcalSource();
      int    flag1      = b->GetTriggerEnable(0);
      int    flag2      = b->GetTriggerEnable(1);
      int    trgSource  = b->GetTriggerSource();
      int    trgDelay   = b->GetTriggerDelay();
      double range      = b->GetInputRange();
      int    config     = b->GetReadoutChannelConfig();
      int    casc       = b->GetChannelCascading();

      wxBusyCursor cursor;
      b->CalibrateVolt(this);

      /* restore old values */
      b->EnableAcal(acalMode, acalVolt);
      b->EnableTcal(tcalFreq, tcalLevel);
      b->SelectClockSource(tcalSource);
      b->EnableTrigger(flag1, flag2);
      b->SetTriggerSource(trgSource);
      b->SetTriggerDelayPercent(trgDelay);
      b->SetInputRange(range);
      if (casc == 2)
         b->SetChannelConfig(config, 8, 4);
      else
         b->SetChannelConfig(config, 8, 8);

      if (b->GetBoardType() == 5)
         b->SetTranspMode(1); // Evaluation board with build-in trigger
      else
         b->SetTranspMode(1); // VPC Mezzanine board

      UpdateControls();
      
      m_frame->GetTimer()->Start(100);
      m_frame->GetOsci()->Start();
      m_frame->GetOsci()->Enable(true);
   }
   Progress(0);
}

void ConfigDialog::OnButtonCalTime( wxCommandEvent& event )
{
   fCalMode = 2;
   if (m_frame->GetOsci()->GetNumberOfBoards()) {
      if (m_frame->GetOsci()->GetCurrentBoard()->GetFirmwareVersion() < 13279)
         wxMessageBox(wxT("Firmware revision 13279 or later\nrequired for timing calibration"),
                         wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
      else if (m_frame->GetOsci()->GetInputRange() != 0)
         wxMessageBox(wxT("Timing calibration can only be done\nat the -0.5V to +0.5V input range"),
                      wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);

      else {

         DRSBoard *b = m_frame->GetOsci()->GetCurrentBoard();
         m_frame->GetOsci()->Enable(false); // turn off readout thread

         /* remember current settings */
         double acalVolt   = b->GetAcalVolt();
         int    acalMode   = b->GetAcalMode();
         int    tcalFreq   = b->GetTcalFreq();
         int    tcalLevel  = b->GetTcalLevel();
         int    tcalSource = b->GetTcalSource();
         int    flag1      = b->GetTriggerEnable(0);
         int    flag2      = b->GetTriggerEnable(1);
         int    trgSource  = b->GetTriggerSource();
         int    trgDelay   = b->GetTriggerDelay();
         double range      = b->GetInputRange();
         int    config     = b->GetReadoutChannelConfig();

         m_frame->SetPaintMode(kPMTimeCalibration);

         wxBusyCursor cursor;
         int status = b->CalibrateTiming(this);

         if (!status)
            wxMessageBox(wxT("Error performing timing calibration, please check waveforms and redo voltage calibration."),
                         wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
         else
            wxMessageBox(wxT("Timing calibration successfully finished."),
                         wxT("DRS Oscilloscope"), wxOK, this);

         m_frame->SetPaintMode(kPMWaveform);

         /* restore old values */
         b->EnableAcal(acalMode, acalVolt);
         b->EnableTcal(tcalFreq, tcalLevel);
         b->SelectClockSource(tcalSource);
         b->EnableTrigger(flag1, flag2);
         b->SetTriggerSource(trgSource);
         b->SetTriggerDelayPercent(trgDelay);
         b->SetInputRange(range);
         b->SetChannelConfig(config, 8, 8);

         if (b->GetBoardType() == 5)
            b->SetTranspMode(1); // Evaluation board with build-in trigger
         else
            b->SetTranspMode(1); // VPC Mezzanine board

         FreqChange(); // update enable flag for timing calibration check box
         m_frame->GetTimer()->Start(100);
         m_frame->GetOsci()->Start();
         m_frame->GetOsci()->Enable(true);
      }
      Progress(0);
   }
}

void ConfigDialog::OnClkOn( wxCommandEvent& event )
{
   m_frame->SetClkOn(event.IsChecked());
}

void ConfigDialog::OnDateTime( wxCommandEvent& event )
{
   m_frame->SetDisplayDateTime(event.IsChecked());
}

void ConfigDialog::OnShowGrid( wxCommandEvent& event )
{
   m_frame->SetDisplayShowGrid(event.IsChecked());
}

void ConfigDialog::OnDisplayWaveforms( wxCommandEvent& event )
{
   if (event.GetId() == ID_DISP_CALIBRATED)
      m_frame->SetDisplayCalibrated(event.IsChecked());
   if (event.GetId() == ID_DISP_CALIBRATED2)
      m_frame->SetDisplayCalibrated2(event.IsChecked());
   if (event.GetId() == ID_DISP_ROTATED)
      m_frame->SetDisplayRotated(event.IsChecked());
   if (event.GetId() == ID_DISP_TCALIBRATED)
      m_frame->SetDisplayTCalOn(event.IsChecked());
   if (event.GetId() == ID_DISP_TRGCORR)
      m_frame->SetDisplayTrgCorr(event.IsChecked());
   if (event.GetId() == ID_REFCLK) {
      if (event.IsChecked()) {
         // check if clock is connected to CLK in
         if (m_frame->GetOsci()->GetNumberOfBoards() > 0) {
            if (m_frame->GetOsci()->GetCurrentBoard()->GetFirmwareVersion() < 21260) {
               wxMessageBox(wxT("For this operation a V5 board with firmware revision >= 21260 is required"),
                            wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
               m_cbExtRefclk->SetValue(false);
            } else {
               if (m_frame->GetOsci()->GetScaler(5) < 300000) {
                  wxMessageBox(wxT("No clock signal connected to CLK IN"),
                               wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
                  m_cbExtRefclk->SetValue(false);
               } else
                  m_frame->SetRefclk(m_board, true);
            }
         }
      } else
         m_frame->SetRefclk(m_board, false);
      FreqChange();
   }
}

void ConfigDialog::OnRemoveSpikes( wxCommandEvent& event )
{
   m_frame->SetSpikeRemoval(event.IsChecked());
}

void ConfigDialog::FreqChange()
{
   wxString wxstr;
   wxstr.Printf(wxT("%1.4lg"), m_frame->GetReqSamplingSpeed());
   m_tbFreq->SetValue(wxstr);
   wxstr.Printf(wxT("%1.4lg GSPS"), m_frame->GetActSamplingSpeed());
   m_stActFreq->SetLabel(wxstr);

   if (m_osci->GetNumberOfBoards() == 0) {
      m_cbTCalOn->SetValue(true);
      m_cbTCalOn->Enable(true);
   } else {
      m_cbTCalOn->SetValue(m_frame->GetOsci()->IsTCalibrated());
      m_cbTCalOn->Enable(m_frame->GetOsci()->IsTCalibrated());
   }
}

void ConfigDialog::OnFreq( wxCommandEvent& event )
{
   wxString wxstr = m_tbFreq->GetValue();
   double freq = 0;
   wxstr.ToDouble(&freq);
   m_frame->SetSamplingSpeed(freq);
   wxstr.Printf(wxT("%1.4lg GSPS"), m_frame->GetActSamplingSpeed());
   m_stActFreq->SetLabel(wxstr);

   if (m_osci->GetNumberOfBoards() == 0) {
      m_cbTCalOn->SetValue(true);
      m_cbTCalOn->Enable(true);
   } else {
      m_cbTCalOn->SetValue(m_frame->GetOsci()->IsTCalibrated());
      m_cbTCalOn->Enable(m_frame->GetOsci()->IsTCalibrated());
   }
}

void ConfigDialog::OnLock( wxCommandEvent& event )
{
   m_frame->SetFreqLock(event.IsChecked());
}

void ConfigDialog::OnClose( wxCommandEvent& event )
{
   this->Hide();
}
