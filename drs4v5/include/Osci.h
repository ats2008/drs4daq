/*
 * Osci.h
 * DRS oscilloscope header file
 * $Id: Osci.h 21496 2014-09-26 14:55:49Z ritt $
 */

typedef struct {
   unsigned short Year;
   unsigned short Month;
   unsigned short Day;
   unsigned short Hour;
   unsigned short Minute;
   unsigned short Second;
   unsigned short Milliseconds;
} TIMESTAMP;

#define TM_AUTO    0
#define TM_NORMAL  1

/*------------------------------------------------------------------*/

class Osci;

class OsciThread : public wxThread
{
public:
   OsciThread(Osci *o);
   bool IsIdle();
   void *Entry();
   void ResetSW();
   void Enable(bool flag);
   void StopThread();
   bool IsFinished() { return m_finished; }

private:
   Osci *m_osci;
   wxStopWatch m_sw1, m_sw2;
   bool m_enabled;
   bool m_active;
   bool m_finished;
   bool m_stopThread;
};

/*------------------------------------------------------------------*/

class Osci
{
public:
   Osci(double samplingSpeed = 5, bool mthread = true);
   ~Osci();
   void      StopThread(void);
   int       ScanBoards();
   int       GetNumberOfBoards()    { return m_drs->GetNumberOfBoards(); }
   DRSBoard *GetBoard(int i)        { return m_drs->GetBoard(i); }
   DRSBoard *GetCurrentBoard()      { return m_drs->GetBoard(m_board); }
   int       GetCurrentBoardIndex() { return m_board; }
   DRS      *GetDRS()               { return m_drs; }
   bool      GetError(char *str, int size) { return m_drs->GetError(str, size); }
   void      CheckTimingCalibration();
   void      SelectBoard(int board);
   void      SelectChannel(int firstChannel, int chnSection);
   void      SetMultiBoard(bool multi);
   bool      IsMultiBoard()         { return m_multiBoard; }
   void      SetRunning(bool flag);
   void      Enable(bool flag);
   bool      IsRunning()            { return m_running; }
   void      SetSingle(bool flag);
   bool      IsSingle()             { return m_single; }
   void      SetArmed(bool flag)    { m_armed = flag; }
   bool      IsArmed()              { return m_armed; }
   bool      IsIdle();
   int       GetWaveformDepth(int channel);
   double    GetWaveformLength()    { return m_waveDepth / GetSamplingSpeed(); }
   float    *GetWaveform(int b, int i)    { return (float *)m_waveform[b][i]; }
   float    *GetTime(int board, int channel);
   int       GetChip()              { return m_chip; }
   void      SetSamplingSpeed(double freq);
   double    GetSamplingSpeed();
   double    GetTrueSamplingSpeed();
   double    GetMinSamplingSpeed();
   double    GetMaxSamplingSpeed();
   bool      IsTCalibrated();
   bool      IsVCalibrated();
   bool      GetTimeCalibration(int chip, int channel, int mode, float *time, bool force=false);
   void      Start();
   void      Stop();
   void      DrainEvents();
   void      SingleTrigger();
   void      ReadWaveforms();
   int       SaveWaveforms(MXML_WRITER *, int);
   bool      HasTriggered();
   bool      HasNewEvent();
   void      SetTriggerLevel(double level);
   void      SetTriggerPolarity(bool negative);
   void      SetIndividualTriggerLevel(int i, double level);
   void      SetTriggerDelay(int delay);
   int       GetTriggerDelay();
   double    GetTriggerDelayNs();
   void      SetTriggerMode(int mode) { m_trgMode = mode; }
   int       GetTriggerMode() { return m_trgMode; }
   void      SetTriggerSource(int source);
   int       GetTriggerSource() { return m_trgSource[m_board]; }
   void      SetTriggerConfig(int tc);
   void      SetRefclk(int board, bool flag);
   void      SetChnOn(int board, int chn, bool flag);
   void      SetClkOn(bool flag);
   void      SetEventSerial(int serial) { m_evSerial = serial; }
   void      SetCalibVoltage(bool flag, double voltage);
   void      SetInputRange(double center);
   double    GetInputRange() { return m_inputRange; }
   double    GetCalibratedInputRange();
   unsigned int GetScaler(int channel);
   void      SetCalibrated(bool flag) { m_calibrated = flag; }
   void      SetCalibrated2(bool flag) { m_calibrated2 = flag; }
   void      SetTCalOn(bool flag) { m_tcalon = flag; }
   bool      IsTCalOn() { return m_tcalon; }
   void      SetRotated(bool flag) { m_rotated = flag; }
   void      SetSpikeRemoval(bool flag) { m_spikeRemoval = flag; }
   void      CorrectTriggerPoint(double t);
   void      RemoveSpikes(int board, bool cascading);
   int       CheckWaveforms();
   bool      SkipDisplay(void) { return m_skipDisplay; }
   int       GetChnSection(void) { return m_chnSection; }
   char      *GetDebugMsg(void) { return m_debugMsg; }

private:
   DRS      *m_drs;
   OsciThread *m_thread;
   bool      m_running;
   bool      m_single;
   bool      m_armed;
   double    m_samplingSpeed;
   int       m_nBoards;
   float     m_waveform[MAX_N_BOARDS][4][2048];
   float     m_refwaveform[2048];
   unsigned char m_wavebuffer[MAX_N_BOARDS][9*1024*2];
   float     m_time[MAX_N_BOARDS][4][2048];
   float     m_timeClk[MAX_N_BOARDS][1024];
   int       m_triggerCell[MAX_N_BOARDS];
   int       m_writeSR[MAX_N_BOARDS];
   int       m_boardSerial[MAX_N_BOARDS];
   int       m_waveDepth;
   int       m_trgMode;
   int       m_trgSource[MAX_N_BOARDS];
   bool      m_trgNegative;
   int       m_trgDelay;
   double    m_trgLevel[4];
   bool      m_chnOn[MAX_N_BOARDS][4];
   bool      m_clkOn;
   bool      m_refClk[MAX_N_BOARDS];
   bool      m_calibOn;
   int       m_evSerial;
   TIMESTAMP m_evTimestamp;
   bool      m_calibrated;
   bool      m_calibrated2;
   bool      m_tcalon;
   bool      m_rotated;
   int       m_nDRS;
   int       m_board;
   int       m_chip;
   int       m_chnOffset;
   int       m_chnSection;
   bool      m_spikeRemoval;
   double    m_inputRange;
   bool      m_skipDisplay;
   bool      m_multiBoard;
   char      m_debugMsg[256];
};

/*------------------------------------------------------------------*/

