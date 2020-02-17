/*
 * EPThread.h
 * DRS oscilloscope event processor header file
 * $Id: EPThread.h 21263 2014-02-07 16:38:07Z ritt $
 */

class EPThread : public wxThread
{
public:
   EPThread(DOFrame *o);
   ~EPThread();
   void *Entry();
   float *GetTime(int b, int c)     { return m_time[b][c]; }
   float *GetWaveform(int b, int c) { return m_waveform[b][c]; }
   void ClearWaveforms();
   void Enable(bool flag);
   void StopThread();
   bool IsFinished()                { return m_finished; }

private:
   DOFrame *m_frame;
   Osci    *m_osci;
   bool     m_stopThread;
   bool     m_enabled;
   bool     m_active;
   bool     m_finished;
   float    m_waveform[MAX_N_BOARDS][4][2048];
   float    m_time[MAX_N_BOARDS][4][2048];
};
