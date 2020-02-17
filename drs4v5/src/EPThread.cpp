/*
 * EPThread.cpp
 * DRS oscilloscope event processing thread
 * $Id: EPThread.cpp 21511 2014-10-17 08:02:30Z ritt $
 */

#include "DRSOscInc.h"
#include "rb.h"

extern wxCriticalSection *g_epcs;

bool g_finished = false;
/*------------------------------------------------------------------*/

EPThread::EPThread(DOFrame *f) : wxThread()
{
   m_frame = f;
   m_osci = f->GetOsci();
   m_finished = false;
   m_stopThread = false;
   m_active = false;
   m_enabled = true;
   Create();
   Run();
} 

/*------------------------------------------------------------------*/

EPThread::~EPThread()
{
} 

/*------------------------------------------------------------------*/

void EPThread::ClearWaveforms()
{
   while (m_osci->HasNewEvent());
   memset(m_time, 0, sizeof(m_time));
   memset(m_waveform, 0, sizeof(m_waveform));
}

/*------------------------------------------------------------------*/

void EPThread::StopThread()
{
   m_stopThread = true;
   do {
     wxThread::Sleep(10);
   } while (!g_finished); // cannot access m_finished here under Widnows
}

/*------------------------------------------------------------------*/

void EPThread::Enable(bool flag)
{
   m_enabled = flag;
   if (!flag)
      while (m_active)
         wxThread::Sleep(10);
}

/*------------------------------------------------------------------*/

void *EPThread::Entry()
{
   int status;

   do {
      if (m_enabled) {
         m_active = true;
         if (m_osci->HasNewEvent()) {
            m_osci->ReadWaveforms();
            if (m_frame->GetRearm()) {
               m_osci->Start();
               m_frame->SetRearm(false);
            }
            
            if (m_frame->GetTrgCorr())
               m_osci->CorrectTriggerPoint(m_frame->GetTrgPosition(0));
            
            g_epcs->Enter();
            status = 0;
            if (m_frame->GetWFFile() || m_frame->GetWFfd()) {
               status = m_osci->SaveWaveforms(m_frame->GetWFFile(), m_frame->GetWFfd());
               m_frame->IncrementSaved();
            }
            g_epcs->Leave();
            
            if (status < 0)
               m_frame->CloseWFFile(true);
            
            if (m_frame->GetWFFile() || m_frame->GetWFfd())
               if (m_frame->GetNSaved() >= m_frame->GetNSaveMax())
                  m_frame->CloseWFFile(false);
            
            m_frame->EvaluateMeasurements();
            m_frame->IncrementAcquisitions();
            
            // copy event from oscilloscope
            int n = m_osci->IsMultiBoard() ? m_osci->GetNumberOfBoards() : 1;
            
            g_epcs->Enter();
            for (int i=0 ; i<n ; i++) {
               for (int j=0 ; j<4 ; j++) {
                  memcpy(m_time[i][j], m_osci->GetTime(i, j), m_osci->GetWaveformDepth(j)*sizeof(float));
                  memcpy(m_waveform[i][j], m_osci->GetWaveform(i, j), m_osci->GetWaveformDepth(j)*sizeof(float));
               }
            }
            g_epcs->Leave();
         } else
            wxThread::Sleep(10);
      } else {
         wxThread::Sleep(10);
         m_active = false;
      }
      
   } while (!m_stopThread);

   m_finished = true;
   g_finished = true;

   return NULL;
}

