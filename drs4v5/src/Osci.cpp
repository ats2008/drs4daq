/*
 * Osci.cpp
 * DRS oscilloscope main class
 * $Id: Osci.cpp 22326 2016-10-10 14:21:29Z ritt $
 */

#include "DRSOscInc.h"
#include "rb.h"

#ifndef _MSC_VER
#include <sys/time.h>
#endif

int g_rbh;

#define RB_SIZE (1024*1024*4) // 4 MB fits 60 waveforms for four boards 

void GetTimeStamp(TIMESTAMP &ts);

/*------------------------------------------------------------------*/

Osci::Osci(double samplingSpeed, bool mthread)
{
   m_drs = NULL;
   m_thread = NULL;
   m_running = false;
   m_single = false;
   m_armed = false;
   m_samplingSpeed = samplingSpeed;
   m_triggerCell[0] = 0;
   m_writeSR[0] = 0;
   m_waveDepth = 1024;
   m_trgMode = TM_AUTO;
   m_trgNegative = false;
   m_trgDelay = 0;
   for (int i=0 ; i<4 ; i++)
      m_trgLevel[i] = 0;
   for (int b=0 ; b<MAX_N_BOARDS ; b++) {
      m_trgSource[b] = 0;
      m_refClk[b] = false;
      for (int i=0 ; i<4 ; i++)
         m_chnOn[b][i] = false;
   }
   m_clkOn = false;
   m_calibOn = false;
   m_evSerial = 1;
   m_calibrated = true;
   m_calibrated2 = true;
   m_tcalon = true;
   m_rotated = true;
   m_nDRS = 0;
   m_board = 0;
   m_chip = 0;
   m_chnOffset = 0;
   m_chnSection = 0;
   m_spikeRemoval = false;
   m_inputRange = 0;
   m_skipDisplay = false;
   m_multiBoard = false;
   m_debugMsg[0] = 0;

   ScanBoards();
   SelectBoard(0);
   SelectChannel(0, 0);
   
   if (mthread) {
      rb_create(RB_SIZE, 4*MAX_N_BOARDS*(9*1024*2+64), &g_rbh);
      m_thread = new OsciThread(this);
   }
} 

/*------------------------------------------------------------------*/

Osci::~Osci()
{
   if (m_thread) {
      m_thread->StopThread();
      wxThread::Sleep(100);
   }
   if (g_rbh)
      rb_delete(g_rbh);
   delete m_drs;
} 

/*------------------------------------------------------------------*/

void Osci::StopThread(void)
{
   if (m_thread) {
      do {
         m_thread->StopThread();
         wxThread::Sleep(100);
      } while (!m_thread->IsFinished());
   }
}

/*------------------------------------------------------------------*/

void GetTimeStamp(TIMESTAMP &ts)
{
#ifdef _MSC_VER
   SYSTEMTIME t;
   static unsigned int ofs = 0;

   GetLocalTime(&t);
   if (ofs == 0)
      ofs = timeGetTime() - t.wMilliseconds;
   ts.Year         = t.wYear;
   ts.Month        = t.wMonth;
   ts.Day          = t.wDay;
   ts.Hour         = t.wHour;
   ts.Minute       = t.wMinute;
   ts.Second       = t.wSecond;
   ts.Milliseconds = (timeGetTime() - ofs) % 1000;
#else
   struct timeval t;
   struct tm *lt;
   time_t now;

   gettimeofday(&t, NULL);
   time(&now);
   lt = localtime(&now);

   ts.Year         = lt->tm_year+1900;
   ts.Month        = lt->tm_mon+1;
   ts.Day          = lt->tm_mday;
   ts.Hour         = lt->tm_hour;
   ts.Minute       = lt->tm_min;
   ts.Second       = lt->tm_sec;
   ts.Milliseconds = t.tv_usec/1000;
#endif /* OS_UNIX */
}


/*------------------------------------------------------------------*/

int Osci::ScanBoards()
{
   /* pause readout thread if present */
   if (m_thread)
      m_thread->Enable(false);

   if (m_drs)
      delete m_drs;

   m_drs = new DRS();
   m_drs->SortBoards();
   m_nDRS = m_drs->GetNumberOfBoards();
   m_board = 0;

   for (int i=0 ; i< m_nDRS ; i++) {
      DRSBoard *b = m_drs->GetBoard(i);
      b->Init();
      if (b->GetBoardType() == 3) {  // DRS2 board
         b->SetDominoMode(1);
         b->EnableTrigger(1, 0);     // fast trigger
         if (m_samplingSpeed > 2)
            m_samplingSpeed = 2;
         b->SetFrequency(m_samplingSpeed, true);
         m_samplingSpeed = b->GetNominalFrequency();
         b->SetNumberOfChannels(10);
#ifdef _MSC_VER
         b->SetCalibrationDirectory("C:/experiment/calibrations");
#else
         b->SetCalibrationDirectory("/home/meg/meg/online/calibrations");
#endif

         if (i == 0)
            printf("Reading calibration for sampling speed %lg GSPS\n", m_samplingSpeed);

         if (!b->GetResponseCalibration()->ReadCalibration(0) ||
             !b->GetResponseCalibration()->ReadCalibration(1)) {
            wxString str;

            str.Printf(wxT("Cannot read calibration for %1.1lf GSPS VME board slot %2d %s, serial #%d"), 
                m_samplingSpeed,
               (b->GetSlotNumber() >> 1)+2, 
               ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower",
                b->GetBoardSerialNumber());

            wxMessageBox(str, wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, NULL);
         } else {
            printf("Calibration read for VME board slot %2d %s, serial #%04d\n", 
               (b->GetSlotNumber() >> 1)+2, 
               ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower",
                b->GetBoardSerialNumber());
         }

      } else {                       // DRS4 board

         /* obtain default sampling speed from calibration of first board */
         if (i == 0)
            m_samplingSpeed = b->GetCalibratedFrequency();

         b->SetFrequency(m_samplingSpeed, true);
         m_samplingSpeed = b->GetNominalFrequency();
         if (b->GetTransport() == TR_USB2)
            b->SetChannelConfig(0, 8, 8);
         else
            b->SetChannelConfig(7, 8, 8);
         b->SetDecimation(0);
         b->SetDominoMode(1);
         b->SetReadoutMode(1);
         b->SetDominoActive(1);
         if (b->GetBoardType() == 5 || b->GetBoardType() == 7 || b->GetBoardType() == 8 || b->GetBoardType() == 9) {
            b->SetTranspMode(1);     // Evaluation board with build-in trigger
            b->EnableTrigger(0, 1);  // Enable analog trigger
            b->SetTriggerSource(0);  // on CH0
         } else if (b->GetBoardType() == 6) {
            b->SetTranspMode(0);     // VPC Mezzanine board
            b->EnableTrigger(0, 0);  // Disable analog trigger
         }
         b->SetRefclk(0);
         b->SetFrequency(m_samplingSpeed, true);
         b->EnableAcal(0, 0);
         b->EnableTcal(0, 0);
         b->StartDomino();
      }
   }
   
   /* resume readout thread if present */
   if (m_thread)
      m_thread->Enable(true);

   return m_nDRS;
}

/*------------------------------------------------------------------*/

void Osci::CheckTimingCalibration()
{
   for (int i=0 ; i< m_nDRS ; i++) {
      DRSBoard *b = m_drs->GetBoard(i);
      if (b->GetDRSType() == 4) {

         if (!b->IsVoltageCalibrationValid()) {
            wxString str;

            if (b->GetTransport() != TR_VME) {
               str.Printf(wxT("Board on USB%d has invalid voltage calibration\nOnly raw data will be displayed"),
                  b->GetSlotNumber());
            } else {
               str.Printf(wxT("VME board in slot %2d %s has invalid voltage calibration\nOnly raw data will be displayed"),
                  (b->GetSlotNumber() >> 1)+2, 
                  ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower");
            }
            wxMessageBox(str, wxT("DRS Oscilloscope Warning"), wxOK | wxICON_EXCLAMATION);
         } else {
            if (!b->IsTimingCalibrationValid()) {
               wxString str;

               if (b->GetTransport() != TR_VME) {
                  if (b->GetCalibratedFrequency() == -1)
                     str.Printf(wxT("Board on USB%d has been timing calibrated with an old method. You must redo the timing calibration to obtain precise timing results."), b->GetSlotNumber());
                  else
                     str.Printf(wxT("Board on USB%d has been timing calibrated at %1.4lg GSPS. You must redo the timing calibration at %1.4lg GSPS to obtain precise timing results."),
                                b->GetSlotNumber(), b->GetCalibratedFrequency(), m_samplingSpeed);
               } else {
                  str.Printf(wxT("VME board in slot %2d %s has been timing calibrated at %1.4lg GSPS. You must redo the timing calibration at %1.4lg GSPS to obtain precise timing results."),
                     (b->GetSlotNumber() >> 1)+2, 
                     ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower",
                      b->GetCalibratedFrequency(), b->GetNominalFrequency());
               }
               wxMessageBox(str, wxT("DRS Oscilloscope Warning"), wxOK | wxICON_EXCLAMATION);
            }
         }
      }
   }
}

/*------------------------------------------------------------------*/

void Osci::SelectBoard(int board)
{
   if (board >= m_nDRS)
      return;

   /* pause readout thread if present */
   if (m_thread)
      m_thread->Enable(false);
   
   m_board = board;
   
   if (m_thread)
      DrainEvents();
   
   /* resume readout thread if present */
   if (m_thread)
      m_thread->Enable(true);
}

/*------------------------------------------------------------------*/

void Osci::SelectChannel(int firstChannel, int chnSection)
{
   if (m_drs->GetNumberOfBoards() == 0)
      return;
   
   /* pause readout thread if present */
   if (m_thread)
      m_thread->Enable(false);
   
   for (int bi=0 ; bi < m_nDRS ; bi++) {
      DRSBoard *b = m_drs->GetBoard(bi);

      /* stop drs_readout state machine to be ready for configuration change */
      if (b->IsBusy()) {
         b->SoftTrigger();
         for (int i=0 ; i<10 && b->IsBusy() ; i++)
            wxMilliSleep(10);
      }
      
      if (b->GetBoardType() == 6 && b->GetTransport() == TR_USB2) {
         if (firstChannel == 0 || firstChannel == 2) {
            if (chnSection == 0)
               b->SetChannelConfig(1, 8, 8);
            else
               b->SetChannelConfig(0, 8, 8);
         } else {
            if (chnSection == 0)
               b->SetChannelConfig(3, 8, 8);
            else
               b->SetChannelConfig(2, 8, 8);
         }
         m_chip       = firstChannel;
         m_chnOffset  = 0;
         m_chnSection = chnSection;
      } else {
         m_chip       = firstChannel;
         m_chnOffset  = chnSection;
         m_chnSection = chnSection;
         
         if (b->GetBoardType() == 3) {
            m_chip      = firstChannel / 2;
            m_chnOffset = (firstChannel % 2)* 4;
         }
         
         if (b->GetBoardType() == 5 || b->GetBoardType() == 7 || b->GetBoardType() == 8 || b->GetBoardType() == 9) {
            if (chnSection == 2)
               b->SetChannelConfig(0, 8, 4);
            else
               b->SetChannelConfig(0, 8, 8);
         } else {
            if (chnSection == 2)
               b->SetChannelConfig(7, 8, 4);
            else
               b->SetChannelConfig(7, 8, 8);
         }
      }
      
      /* resume readout thread if present */
      if (m_thread)
         m_thread->Enable(true);
   }
}

/*------------------------------------------------------------------*/

void Osci::SetMultiBoard(bool flag)
{
   m_multiBoard = flag;
}

/*------------------------------------------------------------------*/

OsciThread::OsciThread(Osci *o) : wxThread()
{
   m_osci = o;
   m_enabled = false;
   m_finished = false;
   m_active = false;
   m_stopThread = false;
   Create();
   Run();
}

/*------------------------------------------------------------------*/

void OsciThread::Enable(bool flag) 
{ 
   m_enabled = flag;
   if (!flag)
      while (m_active)
          wxThread::Sleep(10);
}

/*------------------------------------------------------------------*/

void OsciThread::StopThread()
{
   m_stopThread = true;
   while (!m_finished)
      wxThread::Sleep(10);
}

/*------------------------------------------------------------------*/

bool OsciThread::IsIdle()
{
   if (m_osci->IsRunning() && m_sw1.Time() > 1000)
      return true;

   if (m_osci->IsSingle() && m_osci->IsArmed() && m_sw1.Time() > 1000)
      return true;

   return false;
}

/*------------------------------------------------------------------*/

void OsciThread::ResetSW()
{
   m_sw1.Start();
}

/*------------------------------------------------------------------*/

void *OsciThread::Entry()
{
   int size, index, status, n, m;
   unsigned char *pdata;
   unsigned short *p;
   bool autoTriggered;
   DRSBoard *b;
   TIMESTAMP ts;
   bool skip_event;

   n = m = 0;
   autoTriggered = false;
   do {
      if (m_osci->GetDRS()->GetNumberOfBoards() > 0 && m_enabled) {
         m_active = true;
         if (m_osci->HasTriggered()) {

            skip_event = false;
            n = 0;
            if (!autoTriggered)
               m_sw1.Start(); // we got a real trigger
            autoTriggered = false;

            // wait for space in ring buffer
            do {
               status = rb_get_wp(g_rbh, (void **)&pdata, 100);
               if (status == RB_SUCCESS) {
                  // put number of boards into ring buffer
                  p = (unsigned short *)pdata;
                  *p++ = m_osci->IsMultiBoard() ? m_osci->GetDRS()->GetNumberOfBoards() : 1;

                  for (index = 0 ; index < m_osci->GetDRS()->GetNumberOfBoards() ; index++) {
               
                     b = m_osci->GetBoard(index);
                     if (!m_osci->IsMultiBoard() && m_osci->GetCurrentBoard() != b)
                        continue;
                     
                     // abort event readout if slave board has not triggered
                     if (m_osci->IsMultiBoard())
                        if (index > 0)
                           if (b->IsBusy())
                              skip_event = true;

                     if (skip_event)
                        break;
                     
                     // transfer waveforms
                     b->TransferWaves((unsigned char *)p);
                     size = b->GetWaveformBufferSize();
                     
                     p += size/sizeof(unsigned short);
                     *p++ = b->GetStopCell(0);
                     *p++ = b->GetStopWSR(0);
                     *p++ = (unsigned short)b->GetBoardSerialNumber();
                  }

                  // restart boards in invertd order (master last)
                  for (index = m_osci->GetDRS()->GetNumberOfBoards()-1 ; index>=0 ; index--) {
                     b = m_osci->GetBoard(index);
                     if (!m_osci->IsMultiBoard() && m_osci->GetCurrentBoard() != b)
                        continue;
                     
                     if (m_osci->IsRunning()) {
                        // only if not in multi buffer mode
                        if (!b->IsMultiBuffer())
                           b->StartDomino();
                     }
                  }

                  // in single mode, just clear armed flag
                  if (m_osci->IsSingle())
                     m_osci->SetArmed(false);

                  if (skip_event)
                     break;

                  // add timestamp
                  GetTimeStamp(ts);
                  memcpy(p, &ts, sizeof(ts));
                  p += sizeof(ts)/sizeof(unsigned short);
                  size = (unsigned char*)p - pdata;
                  
                  // commit data to ring buffer
                  rb_increment_wp(g_rbh, size);
               } else
                  wxThread::Sleep(10);

               if (m_stopThread)
                  break;
               
            } while (status != RB_SUCCESS && !TestDestroy());

#ifndef _MSC_VER // Needed for Linux only, otherwise GUI freezes
            if (m++ % 10 == 0)
               wxThread::Yield();
#endif

         } else {
            if (m_osci->GetTriggerMode() == TM_AUTO && m_osci->IsRunning() &&
                m_sw1.Time() > 1000) {
               if (m_sw2.Time() > 300) {
                  wxThread::Sleep(30);  // sleep 3 times a second a bit to test for real trigger
                  m_sw2.Start();
               }
               if (!m_osci->HasTriggered()) {
                  m_osci->SingleTrigger();
                  autoTriggered = true;
               }
            } else {
               n++;
               // sleep once in a while to save CPU
               if (n == 100) {
                  n = 0;
                  wxThread::Sleep(100);
               } else
                  wxThread::Yield();
            }
         }
      } else {
         wxThread::Sleep(10);
         m_active = false;
      }
      
   } while (!m_stopThread);

   m_finished = true;
   
   return NULL;
}

/*------------------------------------------------------------------*/

void Osci::SetRunning(bool flag)
{
   m_running = flag;
   if (m_running)
      Start();
   else {
      Stop();
      if (m_drs->GetNumberOfBoards() > 0)
         DrainEvents();
   }
} 

/*------------------------------------------------------------------*/

void Osci::Enable(bool flag)
{
   if (m_thread)
      m_thread->Enable(flag);
} 

/*------------------------------------------------------------------*/

void Osci::Start()
{
   /* start domino wave */
   if (m_multiBoard) {
      for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
         m_drs->GetBoard(i)->StartDomino();
   } else if (m_drs->GetNumberOfBoards())
      m_drs->GetBoard(m_board)->StartDomino();
   
   m_armed = true;
} 

/*------------------------------------------------------------------*/

void Osci::Stop()
{
   /* stop domino wave */
   if (m_multiBoard) {
      for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
         m_drs->GetBoard(i)->SoftTrigger();
   } else  if (m_drs->GetNumberOfBoards())
      m_drs->GetBoard(m_board)->SoftTrigger();
   m_armed = false;
} 

/*------------------------------------------------------------------*/

void Osci::DrainEvents()
{
   while (HasNewEvent())
      ReadWaveforms();
} 

/*------------------------------------------------------------------*/

void Osci::SetSingle(bool flag)
{
  m_single = flag;
  if (m_thread)
     m_thread->ResetSW();
} 
  
/*------------------------------------------------------------------*/

void Osci::SingleTrigger()
{
   if (m_multiBoard) {
      for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
         m_drs->GetBoard(i)->SoftTrigger();
   } else  if (m_drs->GetNumberOfBoards())
      m_drs->GetBoard(m_board)->SoftTrigger();
}

/*------------------------------------------------------------------*/

bool Osci::HasNewEvent()
{
   int n;

   if (m_drs->GetNumberOfBoards() > 0) {
      if (m_thread) {
         rb_get_buffer_level(g_rbh, &n);
         return n > 0;
      } else {
         if (m_armed && m_drs->GetBoard(m_board)->IsBusy() == 0)
            return true;
         return false;
      }
   } else
      return m_running;
} 

/*------------------------------------------------------------------*/

bool Osci::HasTriggered()
{
   int i;
   
   if (m_drs->GetNumberOfBoards() > 0) {
      if (m_multiBoard) // only first board can trigger
         i = 0;
      else
         i = m_board;
      if (m_running)
         return m_drs->GetBoard(i)->IsEventAvailable() > 0;
      if (m_single && m_armed)
         return m_drs->GetBoard(i)->IsEventAvailable() > 0;
      return false;
   }
   return true;
} 

/*------------------------------------------------------------------*/

bool Osci::IsIdle()
{
   if (m_thread)
      return m_thread->IsIdle();
   return false;
}

/*------------------------------------------------------------------*/

void Osci::ReadWaveforms()
{
unsigned char *pdata;
unsigned short *p;
int size = 0;
DRSBoard * b;

   m_skipDisplay = false;
   m_armed = false;
   m_nBoards = 1;
   
   if (m_drs->GetNumberOfBoards() == 0) {
      for (int w=0 ; w<4 ; w++)
         for (int i=0 ; i<GetWaveformDepth(w) ; i++) {
            m_waveform[0][w][i] = sin(i/m_samplingSpeed/10*M_PI+w*M_PI/4)*100;
            m_waveform[0][w][i] += ((double)rand()/RAND_MAX-0.5)*5;
            m_time[0][w][i] = 1/m_samplingSpeed*i;
         }
      m_waveDepth = kNumberOfBins;
      GetTimeStamp(m_evTimestamp);
   } else {
      int ofs = m_chnOffset;
      int chip = m_chip;

      if (m_drs->GetBoard(m_board)->GetBoardType() == 3) {
         // DRS2 Mezzanine Board 1
         m_drs->GetBoard(m_board)->TransferWaves();
         int tc = m_drs->GetBoard(m_board)->GetTriggerCell(chip);

         m_drs->GetBoard(m_board)->GetTime(chip, 0, m_samplingSpeed, tc, m_time[0][0], m_tcalon, m_rotated);
         m_drs->GetBoard(m_board)->GetWave(chip, 0+ofs, m_waveform[0][0], m_calibrated, tc, !m_rotated);
         m_drs->GetBoard(m_board)->GetWave(chip, 1+ofs, m_waveform[0][1], m_calibrated, tc, !m_rotated);
         m_drs->GetBoard(m_board)->GetWave(chip, 2+ofs, m_waveform[0][2], m_calibrated, tc, !m_rotated);
         if (m_clkOn)
            m_drs->GetBoard(m_board)->GetWave(chip, 9, m_waveform[0][3], m_calibrated, tc, !m_rotated);
         else
            m_drs->GetBoard(m_board)->GetWave(chip, 3+ofs, m_waveform[0][3], m_calibrated, tc, !m_rotated);
      } else if (m_drs->GetBoard(m_board)->GetBoardType() == 5 || m_drs->GetBoard(m_board)->GetBoardType() == 7 ||
                 m_drs->GetBoard(m_board)->GetBoardType() == 8 || m_drs->GetBoard(m_board)->GetBoardType() == 9) {
         
         // DRS4 Evaluation Boards 1.1 + 3.0 + 4.0
         if (m_thread) {
            // get waveforms from ring buffer
            if (rb_get_rp(g_rbh, (void **)&pdata, 0) != RB_SUCCESS)
               return;

            // transfer waveforms to buffer
            size = m_drs->GetBoard(m_board)->GetWaveformBufferSize();
            p = (unsigned short *)pdata;
            m_nBoards = *p++; // number of boards
            
            for (int i=0 ; i<m_nBoards ; i++) {
               memcpy(m_wavebuffer[i], p, size);
               p += size/sizeof(unsigned short);
               m_triggerCell[i] = *p++;
               m_writeSR[i] = *p++;
               m_boardSerial[i] = *p++;
            }

            // transfer timestamp
            memcpy(&m_evTimestamp, p, sizeof(m_evTimestamp));
            p += sizeof(TIMESTAMP)/sizeof(unsigned short);
            size = (unsigned char*)p - pdata;

            // free space in ring buffer
            rb_increment_rp(g_rbh, size);
         } else {
            // get waveforms directly from device
            m_drs->GetBoard(m_board)->TransferWaves(m_wavebuffer[0], 0, 8);
            m_triggerCell[0] = m_drs->GetBoard(m_board)->GetStopCell(chip);
            m_writeSR[0] = m_drs->GetBoard(m_board)->GetStopWSR(chip);
            GetTimeStamp(m_evTimestamp);
         }

         for (int i=0 ; i<m_nBoards ; i++) {
            if (m_nBoards > 1)
               b = m_drs->GetBoard(i);
            else
               b = m_drs->GetBoard(m_board);
            
            // obtain time arrays
            m_waveDepth = b->GetChannelDepth();
            
            for (int w=0 ; w<4 ; w++)
               b->GetTime(0, w*2, m_triggerCell[i], m_time[i][w], m_tcalon, m_rotated);
            
            if (m_clkOn && GetWaveformDepth(0) > kNumberOfBins) {
               for (int j=0 ; j<kNumberOfBins ; j++)
                  m_timeClk[i][j] = m_time[i][0][j] + GetWaveformLength()/2;
            } else {
               for (int j=0 ; j<kNumberOfBins ; j++)
                  m_timeClk[i][j] = m_time[i][0][j];
            }
            
            // decode and calibrate waveforms from buffer
            if (b->GetChannelCascading() == 2) {
               b->GetWave(m_wavebuffer[i], 0, 0, m_waveform[i][0], m_calibrated, m_triggerCell[i], m_writeSR[i], !m_rotated, 0, m_calibrated2);
               b->GetWave(m_wavebuffer[i], 0, 1, m_waveform[i][1], m_calibrated, m_triggerCell[i], m_writeSR[i], !m_rotated, 0, m_calibrated2);
               b->GetWave(m_wavebuffer[i], 0, 2, m_waveform[i][2], m_calibrated, m_triggerCell[i], m_writeSR[i], !m_rotated, 0, m_calibrated2);
               if (m_clkOn && b->GetBoardType() < 9)
                  b->GetWave(m_wavebuffer[i], 0, 8, m_waveform[i][3], m_calibrated, m_triggerCell[i], 0, !m_rotated);
               else
                  b->GetWave(m_wavebuffer[i], 0, 3, m_waveform[i][3], m_calibrated, m_triggerCell[i], m_writeSR[i], !m_rotated, 0, m_calibrated2);
               if (m_spikeRemoval)
                  RemoveSpikes(i, true);
            } else {
               b->GetWave(m_wavebuffer[i], 0, 0+ofs, m_waveform[i][0], m_calibrated, m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
               b->GetWave(m_wavebuffer[i], 0, 2+ofs, m_waveform[i][1], m_calibrated, m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
               b->GetWave(m_wavebuffer[i], 0, 4+ofs, m_waveform[i][2], m_calibrated, m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);
               if (m_clkOn && b->GetBoardType() < 9)
                  b->GetWave(m_wavebuffer[i], 0, 8, m_waveform[i][3], m_calibrated, m_triggerCell[i], 0, !m_rotated);
               else
                  b->GetWave(m_wavebuffer[i], 0, 6+ofs, m_waveform[i][3], m_calibrated, m_triggerCell[i], 0, !m_rotated, 0, m_calibrated2);

               if (m_spikeRemoval)
                  RemoveSpikes(i, false);
            }

            // extrapolate the first two samples (are noisy)
            for (int j=0 ; j<4 ; j++) {
               m_waveform[i][j][1] = 2*m_waveform[i][j][2] - m_waveform[i][j][3];
               m_waveform[i][j][0] = 2*m_waveform[i][j][1] - m_waveform[i][j][2];
            }

         }
      } else if (m_drs->GetBoard(m_board)->GetBoardType() == 6) {
         // DRS4 Mezzanine Board 1
         m_drs->GetBoard(m_board)->TransferWaves(0, 8);
         m_triggerCell[0] = m_drs->GetBoard(m_board)->GetStopCell(chip);
         m_writeSR[0] = m_drs->GetBoard(m_board)->GetStopWSR(chip);
         m_waveDepth = m_drs->GetBoard(m_board)->GetChannelDepth();

         m_drs->GetBoard(m_board)->GetTime(chip, 0, m_samplingSpeed, m_triggerCell[0], m_time[0][0], m_tcalon, m_rotated);
         if (m_clkOn && GetWaveformDepth(0) > kNumberOfBins) {
            for (int i=0 ; i<kNumberOfBins ; i++)
               m_timeClk[0][i] = m_time[0][0][i] + GetWaveformLength()/2;
         } else {
            for (int i=0 ; i<kNumberOfBins ; i++)
               m_timeClk[0][i] = m_time[0][0][i];
         }

         if (m_drs->GetBoard(m_board)->GetChannelCascading() == 2) {
            m_drs->GetBoard(m_board)->GetWave(chip, 0, m_waveform[0][0], m_calibrated, m_triggerCell[0], m_writeSR[0], !m_rotated, 0, m_calibrated2);
            m_drs->GetBoard(m_board)->GetWave(chip, 1, m_waveform[0][1], m_calibrated, m_triggerCell[0], m_writeSR[0], !m_rotated, 0, m_calibrated2);
            m_drs->GetBoard(m_board)->GetWave(chip, 2, m_waveform[0][2], m_calibrated, m_triggerCell[0], m_writeSR[0], !m_rotated, 0, m_calibrated2);
            if (m_clkOn)
               m_drs->GetBoard(m_board)->GetWave(chip, 8, m_waveform[0][3], m_calibrated, m_triggerCell[0], m_writeSR[0], !m_rotated);
            else {
               m_drs->GetBoard(m_board)->GetWave(chip, 3, m_waveform[0][3], m_calibrated, m_triggerCell[0], m_writeSR[0], !m_rotated, 0, m_calibrated2);
               if (m_spikeRemoval)
                  RemoveSpikes(0, true);
            }
         } else {
            m_drs->GetBoard(m_board)->GetWave(chip, 0+ofs, m_waveform[0][0], m_calibrated, m_triggerCell[0], 0, !m_rotated, 0, m_calibrated2);
            m_drs->GetBoard(m_board)->GetWave(chip, 2+ofs, m_waveform[0][1], m_calibrated, m_triggerCell[0], 0, !m_rotated, 0, m_calibrated2);
            m_drs->GetBoard(m_board)->GetWave(chip, 4+ofs, m_waveform[0][2], m_calibrated, m_triggerCell[0], 0, !m_rotated, 0, m_calibrated2);
            if (m_clkOn)
               m_drs->GetBoard(m_board)->GetWave(chip, 8, m_waveform[0][3], m_calibrated, m_triggerCell[0], 0, !m_rotated);
            else {
               m_drs->GetBoard(m_board)->GetWave(chip, 6+ofs, m_waveform[0][3], m_calibrated, m_triggerCell[0], 0, !m_rotated, 0, m_calibrated2);
               if (m_spikeRemoval)
                  RemoveSpikes(0, false);
            }
         }

         /* extrapolate the first two samples (are noisy) */
         for (int i=0 ; i<4 ; i++) {
            m_waveform[0][i][1] = 2*m_waveform[0][i][2] - m_waveform[0][i][3];
            m_waveform[0][i][0] = 2*m_waveform[0][i][1] - m_waveform[0][i][2];
         }
      }

   }

   /* auto-restart in running mode */
   if (m_thread == NULL && m_running)
      Start();
} 

/*------------------------------------------------------------------*/

unsigned char buffer[100000];

int Osci::SaveWaveforms(MXML_WRITER *xml, int fd)
{
   char str[80], name[80];
   unsigned char *p;
   unsigned short d;
   float t;
   int size;
   static unsigned char *buffer;
   static int buffer_size = 0;

   if (xml == NULL && fd == 0)
      return 0;

   if (xml) {
      mxml_start_element(xml, "Event");
      sprintf(str, "%d", m_evSerial);
      mxml_write_element(xml, "Serial", str);
      sprintf(str, "%4d/%02d/%02d %02d:%02d:%02d.%03d", m_evTimestamp.Year, m_evTimestamp.Month,
         m_evTimestamp.Day, m_evTimestamp.Hour, m_evTimestamp.Minute, m_evTimestamp.Second,
         m_evTimestamp.Milliseconds);
      mxml_write_element(xml, "Time", str);
      mxml_write_element(xml, "HUnit", "ns");
      mxml_write_element(xml, "VUnit", "mV");
      
      for (int b=0 ; b<m_nBoards ; b++) {
         sprintf(str, "Board_%d", m_drs->GetBoard(b)->GetBoardSerialNumber());
         mxml_start_element(xml, str);
         sprintf(str, "%d", m_triggerCell[b]);
         mxml_write_element(xml, "Trigger_Cell", str);
         for (int i=0 ; i<4 ; i++) {
            if (m_chnOn[b][i]) {
               sprintf(str, "%u", m_drs->GetBoard(b)->GetScaler(i));
               sprintf(name, "Scaler%d", i);
               mxml_write_element(xml, name, str);
               sprintf(str, "CHN%d", i+1);
               mxml_start_element(xml, str);
               strcpy(str, "\n");
               for (int j=0 ; j<m_waveDepth ; j++) {
                  sprintf(str, "%1.3f,%1.1f", m_time[b][i][j], m_waveform[b][i][j]);
                  mxml_write_element(xml, "Data", str);
               }
               mxml_end_element(xml); // CHNx
            }
         }
         mxml_end_element(xml); //Board
      }

      mxml_end_element(xml); // Event
   }
   
   if (fd) {
      if (buffer_size == 0) {
         buffer_size =   8; // file header + time header
         buffer_size +=  m_nBoards * (4 + 4*(4+m_waveDepth*4)); // bin widths
         buffer_size += 24 + m_nBoards * (8 + 4*(8+m_waveDepth*2));
         buffer = (unsigned char *)malloc(buffer_size);
      }
      
      p = buffer;
      
      if (m_evSerial == 1) {
         memcpy(p, "DRS2", 4); // File identifier and version
         p += 4;
         
         // time calibration header
         memcpy(p, "TIME", 4);
         p += 4;

         for (int b=0 ; b<m_nBoards ; b++) {
            // store board serial number
            sprintf((char *)p, "B#");
            p += 2;
            *(unsigned short *)p = m_drs->GetBoard(b)->GetBoardSerialNumber();
            p += sizeof(unsigned short);

            for (int i=0 ; i<4 ; i++) {
               if (m_chnOn[b][i]) {
                  sprintf((char *)p, "C%03d", i+1);
                  p += 4;
                  float tcal[2048];
                  m_drs->GetBoard(b)->GetTimeCalibration(0, i*2, 0, tcal, 0);
                  for (int j=0 ; j<m_waveDepth ; j++) {
                     // save binary time as 32-bit float value
                     if (m_waveDepth == 2048) {
                        t = (tcal[j % 1024]+tcal[(j+1) % 1024])/2;
                        j++;
                     } else
                        t = tcal[j];
                     *(float *)p = t;
                     p += sizeof(float);
                  }
               }
            }
         }
      }
      
      memcpy(p, "EHDR", 4);
      p += 4;
      *(int *)p = m_evSerial;
      p += sizeof(int);
      *(unsigned short *)p = m_evTimestamp.Year;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Month;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Day;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Hour;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Minute;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Second;
      p += sizeof(unsigned short);
      *(unsigned short *)p = m_evTimestamp.Milliseconds;
      p += sizeof(unsigned short);
      *(unsigned short *)p = (unsigned short)(m_inputRange * 1000); // range
      p += sizeof(unsigned short);
      
      for (int b=0 ; b<m_nBoards ; b++) {

         // store board serial number
         sprintf((char *)p, "B#");
         p += 2;
         *(unsigned short *)p = m_drs->GetBoard(b)->GetBoardSerialNumber();
         p += sizeof(unsigned short);
         
         // store trigger cell
         sprintf((char *)p, "T#");
         p += 2;
         *(unsigned short *)p = m_triggerCell[b];
         p += sizeof(unsigned short);
         
         for (int i=0 ; i<4 ; i++) {
            if (m_chnOn[b][i]) {
               sprintf((char *)p, "C%03d", i+1);
               p += 4;

               unsigned int s = m_drs->GetBoard(b)->GetScaler(i);
               memcpy(p, &s, sizeof(int));
               p += sizeof(int);

               for (int j=0 ; j<m_waveDepth ; j++) {
                  // save binary date as 16-bit value:
                  // 0 = -0.5V,  65535 = +0.5V    for range 0
                  // 0 = -0.05V, 65535 = +0.95V   for range 0.45
                  if (m_waveDepth == 2048) {
                     // in cascaded mode, save 1024 values as averages of the 2048 values
                     d = (unsigned short)(((m_waveform[b][i][j]+m_waveform[b][i][j+1])/2000.0 - m_inputRange + 0.5) * 65535);
                     *(unsigned short *)p = d;
                     p += sizeof(unsigned short);
                     j++;
                  } else {
                     d = (unsigned short)((m_waveform[b][i][j]/1000.0 - m_inputRange + 0.5) * 65535);
                     *(unsigned short *)p = d;
                     p += sizeof(unsigned short);
                  }
               }
            }
         }
      }
      
      size = p-buffer;
      assert(size <= buffer_size);
      int n = write(fd, buffer, size);
      if (n != size)
         return -1;
   }

   m_evSerial++;
   
   return 1;
}

/*------------------------------------------------------------------*/

void Osci::SetSamplingSpeed(double freq)
{
   if (m_drs->GetNumberOfBoards() == 0) {
      m_samplingSpeed = freq;
      return;
   }
   
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++) {
      DRSBoard *b = m_drs->GetBoard(i);

      b->SetFrequency(freq, true);
      m_samplingSpeed = b->GetNominalFrequency();
      wxMilliSleep(10);

      if (b->GetDRSType() == 4 && !b->IsPLLLocked()) {
         wxString str;

#ifdef HAVE_VME
         if (b->GetTransport() == 1)
            str.Printf(wxT("PLLs did not lock on VME board slot %2d %s, serial #%d"), 
               (b->GetSlotNumber() >> 1)+2, ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower", 
               b->GetBoardSerialNumber());
         else
#endif
            str.Printf(wxT("PLLs did not lock on USB board #%d, serial #%d"), 
               b->GetSlotNumber(), b->GetBoardSerialNumber());

         wxMessageBox(str, wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, NULL);
      }
   }
}

/*------------------------------------------------------------------*/

double Osci::GetSamplingSpeed()
{ 
   if (m_drs->GetNumberOfBoards() > 0) {
      DRSBoard *b = m_drs->GetBoard(m_board);
      return b->GetNominalFrequency();
   }
   return m_samplingSpeed;
}

/*------------------------------------------------------------------*/

double Osci::GetTrueSamplingSpeed()
{
   if (m_drs->GetNumberOfBoards() > 0)
      return m_drs->GetBoard(m_board)->GetTrueFrequency();
   return m_samplingSpeed;
}

/*------------------------------------------------------------------*/

double Osci::GetMaxSamplingSpeed()
{ 
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
      if (m_drs->GetBoard(i)->GetDRSType() == 2)
         return 4;
   return 5;
}

/*------------------------------------------------------------------*/

double Osci::GetMinSamplingSpeed()
{ 
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
      if (m_drs->GetBoard(i)->GetDRSType() == 4) {
         if (m_drs->GetBoard(i)->GetBoardSerialNumber() == 2146 ||
             m_drs->GetBoard(i)->GetBoardSerialNumber() == 2205 ||
             m_drs->GetBoard(i)->GetBoardSerialNumber() == 2208)
            return 0.5; // special modified boards for RFBeta & Slow Muons
         return 0.7;
      }
   return 0.5;
}

/*------------------------------------------------------------------*/

int Osci::GetWaveformDepth(int channel)
{ 
   if (channel == 3 && m_clkOn && m_waveDepth > kNumberOfBins)
      return m_waveDepth - kNumberOfBins; // clock chnnael has only 1024 bins

   return m_waveDepth; 
}

/*------------------------------------------------------------------*/

float *Osci::GetTime(int b, int channel)
{
   if (m_drs->GetNumberOfBoards() > 0) {
      if (m_drs->GetBoard(m_board)->GetBoardType() < 9) {
         if (channel == 3 && m_clkOn)
            return (float *)m_timeClk[b];
      }
   }

   return (float *)m_time[b][channel];
}

/*------------------------------------------------------------------*/

bool Osci::IsTCalibrated(void)
{
   return m_drs->GetBoard(m_board)->IsTimingCalibrationValid();
}

/*------------------------------------------------------------------*/

bool Osci::IsVCalibrated(void)
{
   return m_drs->GetBoard(m_board)->IsVoltageCalibrationValid();
}

/*------------------------------------------------------------------*/

bool Osci::GetTimeCalibration(int chip, int channel, int mode, float *time, bool force)
{
   if (!force && !IsTCalibrated())
      return false;

   m_drs->GetBoard(m_board)->GetTimeCalibration(chip, channel, mode, time, force);
   return true;
}

/*------------------------------------------------------------------*/

void Osci::SetTriggerLevel(double level)
{
   m_trgLevel[0] = m_trgLevel[1] = m_trgLevel[2] = m_trgLevel[3] = level;

   // only change trigger of current board
   if (m_drs->GetNumberOfBoards() > 0)
      m_drs->GetBoard(m_board)->SetTriggerLevel(level);
}

/*------------------------------------------------------------------*/

void Osci::SetTriggerPolarity(bool negative)
{
   m_trgNegative = negative;
   
   if (m_drs->GetNumberOfBoards() > 0)
      m_drs->GetBoard(m_board)->SetTriggerPolarity(negative);
}

/*------------------------------------------------------------------*/

void Osci::SetIndividualTriggerLevel(int channel, double level)
{
   m_trgLevel[channel] = level;
   
   if (m_drs->GetNumberOfBoards() > 0)
      m_drs->GetBoard(m_board)->SetIndividualTriggerLevel(channel, level);
}

/*------------------------------------------------------------------*/

void Osci::SetTriggerDelay(int delay)
{
   m_trgDelay = delay;

   if (m_drs->GetNumberOfBoards() > 0)
      m_drs->GetBoard(m_board)->SetTriggerDelayPercent(delay);
}

/*------------------------------------------------------------------*/

int Osci::GetTriggerDelay()
{
   if (m_drs->GetNumberOfBoards() > 0)
      return m_drs->GetBoard(m_board)->GetTriggerDelay();
   else
      return 0;
}

/*------------------------------------------------------------------*/

double Osci::GetTriggerDelayNs()
{
   if (m_drs->GetNumberOfBoards() > 0)
      return m_drs->GetBoard(m_board)->GetTriggerDelayNs();
   else
      return 0;
}

/*------------------------------------------------------------------*/

void Osci::SetTriggerSource(int source)
{
   m_trgSource[m_board] = source;

   if (m_drs->GetNumberOfBoards() > 0) {
      if (m_drs->GetBoard(m_board)->GetBoardType() == 8 || m_drs->GetBoard(m_board)->GetBoardType() == 9) {
         m_drs->GetBoard(m_board)->EnableTrigger(1, 0); // enable trigger
         m_drs->GetBoard(m_board)->SetTriggerSource(1 << source); // simple or of single channel
      } else {
         if (source == 4)
            m_drs->GetBoard(m_board)->EnableTrigger(1, 0); // external trigger
         else {
            m_drs->GetBoard(m_board)->EnableTrigger(0, 1); // analog trigger
            m_drs->GetBoard(m_board)->SetTriggerSource(source);
         }
      }
   }
}

/*------------------------------------------------------------------*/

void Osci::SetTriggerConfig(int tc)
{
   if (m_drs->GetBoard(m_board)->GetBoardType() == 8 || m_drs->GetBoard(m_board)->GetBoardType() == 9) {
      m_drs->GetBoard(m_board)->EnableTrigger(1, 0); // enable trigger
      m_drs->GetBoard(m_board)->SetTriggerSource(tc);
   }
}

/*------------------------------------------------------------------*/

void Osci::SetChnOn(int board, int chn, bool flag)
{
   m_chnOn[board][chn] = flag;
}

/*------------------------------------------------------------------*/

void Osci::SetClkOn(bool flag)
{
   m_clkOn = flag; 
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++) {
      m_drs->GetBoard(i)->EnableTcal(flag ? 1 : 0, 0);
      if (m_drs->GetBoard(i)->GetBoardType() == 5 || m_drs->GetBoard(i)->GetBoardType() == 7 ||
          m_drs->GetBoard(i)->GetBoardType() == 8 || m_drs->GetBoard(i)->GetBoardType() == 9)
         m_drs->GetBoard(i)->SelectClockSource(0); // select sync. clock
   }
}

/*------------------------------------------------------------------*/

void Osci::SetRefclk(int board, bool flag)
{
   m_refClk[board] = flag;
   // only change clock of current board and not all boards to allow daisy-chaining
   if (m_drs->GetNumberOfBoards()) {
      if (m_drs->GetBoard(board)->GetBoardType() == 6 ||
          m_drs->GetBoard(board)->GetBoardType() == 8 ||
          m_drs->GetBoard(board)->GetBoardType() == 9)
            m_drs->GetBoard(board)->SetRefclk(flag);
   }
}

/*------------------------------------------------------------------*/

void Osci::SetCalibVoltage(bool flag, double voltage)
{
   m_calibOn = flag;
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
      m_drs->GetBoard(i)->EnableAcal(flag, voltage);
}

/*------------------------------------------------------------------*/

void Osci::SetInputRange(double center)
{
   m_inputRange = center;
   for (int i=0 ; i< m_drs->GetNumberOfBoards() ; i++)
      m_drs->GetBoard(i)->SetInputRange(center);
}

/*------------------------------------------------------------------*/

double Osci::GetCalibratedInputRange()
{
   return m_drs->GetBoard(m_board)->GetCalibratedInputRange();
}

/*------------------------------------------------------------------*/

unsigned int Osci::GetScaler(int channel)
{
   if (m_drs->GetNumberOfBoards() > 0)
      return m_drs->GetBoard(m_board)->GetScaler(channel);
   return 0;
}

/*------------------------------------------------------------------*/

void Osci::CorrectTriggerPoint(double t)
{
   int i, n, min_i;
   double min_dt, dt, t0, t1, trigPoint[2*kNumberOfBins];
   float  *pt;

   /*---- shift first channel according to trigger point ----*/
   
   if (m_trgSource[0] == 3 && m_clkOn)
      pt = m_timeClk[0];
   else
      pt = m_time[0][m_trgSource[0]];

   if (m_trgSource[0] < 4) {
      // search and store all points
      for (i = n = 0 ; i<m_waveDepth-1 ; i++) {
         if (m_trgNegative) {
            if (m_waveform[0][m_trgSource[0]][i] >= m_trgLevel[m_trgSource[0]]*1000 &&
                m_waveform[0][m_trgSource[0]][i+1] < m_trgLevel[m_trgSource[0]]*1000) {

               dt = pt[i+1] - pt[i];
               dt = dt * 1 / (1 + (m_trgLevel[m_trgSource[0]]*1000-m_waveform[0][m_trgSource[0]][i+1])/(m_waveform[0][m_trgSource[0]][i]-m_trgLevel[m_trgSource[0]]*1000));
               trigPoint[n++] = pt[i] + dt;
            }
         } else {
            if (m_waveform[0][m_trgSource[0]][i] <= m_trgLevel[m_trgSource[0]]*1000 &&
                m_waveform[0][m_trgSource[0]][i+1] > m_trgLevel[m_trgSource[0]]*1000) {

               dt = pt[i+1] - pt[i];
               dt = dt * 1 / (1 + (m_waveform[0][m_trgSource[0]][i+1]-m_trgLevel[m_trgSource[0]]*1000)/(m_trgLevel[m_trgSource[0]]*1000-m_waveform[0][m_trgSource[0]][i]));
               trigPoint[n++] = pt[i] + dt;
            }
         }
      }

      if (n > 2) {
         /* search trigger point closest to trigger in case of many trigger points (like a sine wave) */
         min_dt = 1e6;
         min_i  = -1;
         for (i=0 ; i<n ; i++) {
            if (fabs(trigPoint[i] - t) < min_dt) {
               min_dt = fabs(trigPoint[i] - t);
               min_i = i;
            }
         }
      } else {
         /* choose first trigger point, to avoid jumping of the trigger position after correction */
         min_i = 0;
      }

      /* correct times to shift waveform to trigger point */
      if (n > 0) {
         dt = trigPoint[min_i] - t;
         
         for (int b=0 ; b<m_nBoards ; b++) {
            for (int w=0 ; w<4 ; w++)
               for (i=0 ; i<m_waveDepth ; i++)
                  m_time[b][w][i] -= dt;
            for (i=0 ; i<kNumberOfBins ; i++)
               m_timeClk[b][i] -= dt;
         }
      }
   }
   
   /*---- shift other channels according to cell #0 in multi-board mode ----*/
   
   t0 = m_time[0][0][(1024-m_triggerCell[0]) % 1024];
                     
   for (int b=1 ; b<m_nBoards ; b++) {
      if (!m_refClk[b])
         continue; // skip boards if no external refclk
      
      t1 = m_time[b][0][(1024-m_triggerCell[b]) % 1024];
      dt = t1 - t0;
      if (dt < 0)
         dt += 1024/m_drs->GetBoard(0)->GetTrueFrequency();
      
      for (int w=0 ; w<4 ; w++)
         for (i=0 ; i<m_waveDepth ; i++)
            m_time[b][w][i] -= dt;
      for (i=0 ; i<kNumberOfBins ; i++)
         m_timeClk[b][i] -= dt;
   }
   
}

/*------------------------------------------------------------------*/

int spos[1024];

void Osci::RemoveSpikes(int b, bool cascading)
{
   int i, j, k, l, c, ofs, nChn;
   double x, y;
   int sp[8][10];
   int rsp[10], rot_sp[10];
   int n_sp[8], n_rsp;
   float wf[8][2048];
   int  nNeighbor, nSymmetric;

   nChn = cascading ? 8 : 4;

   /* rotate waveform back relative to cell #0 */
   if (cascading) {
      if (m_rotated) {
         for (i=0 ; i<nChn ; i++)
            for (j=0 ; j<1024 ; j++)
               wf[i][(j+m_triggerCell[b]) % 1024] = m_waveform[b][i/2][(i%2)*1024+j];
      } else {
         for (i=0 ; i<nChn ; i++)
            for (j=0 ; j<1024 ; j++)
               wf[i][j] = m_waveform[b][i/2][(i%2)*1024+j];
      }
   } else {
      if (m_rotated) {
         for (i=0 ; i<nChn ; i++)
            for (j=0 ; j<1024 ; j++)
               wf[i][(j+m_triggerCell[b]) % 1024] = m_waveform[b][i][j];
      } else {
         for (i=0 ; i<nChn ; i++)
            for (j=0 ; j<1024 ; j++)
               wf[i][j] = m_waveform[b][i][j];
      }
   }


   memset(sp, 0, sizeof(sp));
   memset(n_sp, 0, sizeof(n_sp));
   memset(rsp, 0, sizeof(rsp));
   n_rsp = 0;

   /* find spikes with special high-pass filter */
   for (j=0 ; j<1024 ; j++) {
      for (i=0 ; i<nChn ; i++) {
         if (-wf[i][j]+wf[i][(j+1) % 1024]+wf[i][(j+2) % 1024]-wf[i][(j+3) % 1024] > 20) {
            if (n_sp[i] < 10) // record maximum of 10 spikes
               sp[i][n_sp[i]++] = j;
            else
               return;        // too many spikes -> something wrong
            spos[j]++;
         }
         if (-wf[i][j]+wf[i][(j+1) % 1024]+wf[i][(j+2) % 1024]-wf[i][(j+3) % 1024] < -20) {
            if (n_sp[i] < 10) // record maximum of 10 spikes
               sp[i][n_sp[i]++] = j;
            else
               return;        // too many spikes -> something wrong
            spos[j]++;
         }
      }
   }

   /* find spikes at cell #0 and #1023 */
   for (i=0 ; i<nChn ; i++) {
      if (wf[i][0]+wf[i][1]-2*wf[i][2] > 20) {
         if (n_sp[i] < 10)
            sp[i][n_sp[i]++] = 0;
      }
      if (-2*wf[i][1021]+wf[i][1022]+wf[i][1023] > 20) {
         if (n_sp[i] < 10)
            sp[i][n_sp[i]++] = 1020;
      }
   }

   /* go through all spikes and look for symmetric spikes and neighbors */
   for (i=0 ; i<nChn ; i++) {
      for (j=0 ; j<n_sp[i] ; j++) {
         /* check if this spike has a symmetric partner in any channel */
         for (k=nSymmetric=0 ; k<nChn ; k++) {
               for (l=0 ; l<n_sp[k] ; l++)
                  if (sp[i][j] == (1020-sp[k][l]+1024) % 1024) {
                     nSymmetric++;
                     break;
                  }
            }

         /* check if this spike has same spike in any other channels */
         for (k=nNeighbor=0 ; k<nChn ; k++)
            if (i != k) {
               for (l=0 ; l<n_sp[k] ; l++)
                  if (sp[i][j] == sp[k][l]) {
                     nNeighbor++;
                     break;
                  }
            }

         if (nSymmetric + nNeighbor >= 2) {
            /* if at least two matching spikes, treat this as a real spike */
            for (k=0 ; k<n_rsp ; k++)
               if (rsp[k] == sp[i][j])
                  break;
            if (n_rsp < 10 && k == n_rsp)
               rsp[n_rsp++] = sp[i][j];
         }
      }
   }

   /* rotate spikes according to trigger cell */
   if (m_rotated) {
      for (i=0 ; i<n_rsp ; i++)
         rot_sp[i] = (rsp[i] - m_triggerCell[b] + 1024) % 1024;
   } else {
      for (i=0 ; i<n_rsp ; i++)
         rot_sp[i] = rsp[i];
   }

   /* recognize spikes if at least one channel has it */
   for (k=0 ; k<n_rsp ; k++) {
      for (i=0 ; i<nChn ; i++) {

         if (cascading) {
            c = i/2;
            ofs = (i%2)*1024;
         } else {
            c = i;
            ofs = 0;
         }
         if (k < n_rsp-1 && rsp[k] == 0 && rsp[k+1] == 1020) {
            /* remove double spike */
            j = rot_sp[k] > rot_sp[k+1] ? rot_sp[k+1] : rot_sp[k];
            x = m_waveform[b][c][ofs+(j+1) % 1024];
            y = m_waveform[b][c][ofs+(j+6) % 1024];
            if (fabs(x-y) < 15) {
               m_waveform[b][c][ofs+(j+2) % 1024] = x + 1*(y-x)/5;
               m_waveform[b][c][ofs+(j+3) % 1024] = x + 2*(y-x)/5;
               m_waveform[b][c][ofs+(j+4) % 1024] = x + 3*(y-x)/5;
               m_waveform[b][c][ofs+(j+5) % 1024] = x + 4*(y-x)/5;
            } else {
               m_waveform[b][c][ofs+(j+2) % 1024] -= 14.8f;
               m_waveform[b][c][ofs+(j+3) % 1024] -= 14.8f;
               m_waveform[b][c][ofs+(j+4) % 1024] -= 14.8f;
               m_waveform[b][c][ofs+(j+5) % 1024] -= 14.8f;
            }
         } else {
            /* remove single spike */
            x = m_waveform[b][c][ofs+rot_sp[k]];
            y = m_waveform[b][c][ofs+(rot_sp[k]+3) % 1024];

            if (fabs(x-y) < 15) {
               m_waveform[b][c][ofs+(rot_sp[k]+1) % 1024] = x + 1*(y-x)/3;
               m_waveform[b][c][ofs+(rot_sp[k]+2) % 1024] = x + 2*(y-x)/3;
            } else {
               m_waveform[b][c][ofs+(rot_sp[k]+1) % 1024] -= 14.8f;
               m_waveform[b][c][ofs+(rot_sp[k]+2) % 1024] -= 14.8f;
            }
         }
      }
      if (k < n_rsp-1 && rsp[k] == 0 && rsp[k+1] == 1020)
         k++; // skip second half of double spike
   }

   /* uncomment to show unfixed spikes
   m_skipDisplay = true;
   for (i=0 ; i<1024 ; i++)
      for (j=0 ; j<nChn ; j++) {
         if (m_waveform[j][i] > 10 || m_waveform[j][i] < -10) {
            m_skipDisplay = false;
            break;
         }
   }
   */
}

/*------------------------------------------------------------------*/
