/*
 * DOScreen.h
 * DRS oscilloscope screen header file
 * $Id: DOScreen.h 22327 2016-10-11 13:18:26Z ritt $
 */

class Osci;
class DOFrame;

enum PaintModes {
   kPMWaveform,
   kPMTimeCalibration,
};

class DOScreen : public wxWindow
{
public:
   DOScreen(wxWindow* parent, Osci *osci, DOFrame *frame);
   ~DOScreen();
   void SelectBoard(int b)            { m_board = b; }
   void SetChnOn(int b, int i, int value ) { m_chnon[b][i] = value; }
   int  GetChnOn(int b, int i)        { return m_chnon[b][i];  }
   int  GetCurChn()                   { return m_chn; }
   bool GetSplitMode()                { return m_splitMode; }
   void SetSplitMode(bool flag)       { m_splitMode = flag; }
   void SetPaintMode(int pm)          { m_paintMode = pm; }
   void SetPos(int b, int i, double value) { m_offset[b][i] = value; }
   void SetScale(int b, int i, int sclae);
   void SetHScale(int b, int hscale);
   void SetHScaleInc(int b, int increment);
   int  GetPaintMode()                { return m_paintMode; }
   void SetScreenOffset(int b, int offset)  { m_screenOffset[b] = offset; }
   int  GetScreenSize(int b)          { return m_screenSize[b]; }
   int  GetScreenOffset(int b)        { return m_screenOffset[b]; }
   int  GetScaleIndex(int b, int i)   { return m_scale[b][i];  }
   double GetScale(int b, int i)      { return m_scaleTable[m_scale[b][i]]; }
   double GetOffset(int b, int i)     { return m_offset[b][i]; }
   int  GetHScale(int b)              { return m_hscale[b];    }
   void SetDisplayDateTime(bool flag) { m_displayDateTime = flag; }
   void SetDisplayShowGrid(bool flag) { m_displayShowGrid = flag; }
   void SetDisplayLines(bool flag)    { m_displayLines = flag; }
   void SetDisplayScalers(bool flag)  { m_displayScalers = flag; }
   void SetDisplayMode(int mode, int n) { m_displayMode = mode; m_displayN = n; }
   void SetMathDisplay(int id, bool flag);
   wxDC *GetDC()                      { return m_dc; }
   int  GetX1()                       { return m_x1[m_board]; }
   int  GetX2()                       { return m_x2[m_board]; }
   int  GetY1()                       { return m_y1[m_board]; }
   int  GetY2()                       { return m_y2[m_board]; }
   int  timeToX(float t);
   int  voltToY(float v);
   int  voltToY(int chn, float v);
   double XToTime(int x);
   double YToVolt(int y);
   double YToVolt(int chn, int y);
   double GetT1();
   double GetT2();
   
   static const int m_scaleTable[10];
   static const int m_hscaleTable[13];
   
   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnSize(wxSizeEvent& event);
   
   // drawing routines
   void DrawScope(wxDC& dc, wxCoord w, wxCoord h, bool printing);
   void DrawScopeBottom(wxDC& dc, int board, int x1, int y1, int width, bool printing);
   void DrawWaveforms(wxDC& dc, int wfIndex, bool printing);
   void DrawHisto(wxDC& dc, wxCoord w, wxCoord h, bool printing);
   void SaveHisto(int fd);
   void DrawTcalib(wxDC& dc, wxCoord w, wxCoord h, bool printing);
   void DrawMath(wxDC& dc, wxCoord width, wxCoord height, bool printing);
   void DrawPeriodJitter(wxDC& dc, int chn, bool printing);
   void DrawHAxis(wxDC &dc, int x1, int y1, int width,
                  int minor, int major, int text, int label, int grid, double xmin, double xmax);
   
   void OnMouse(wxMouseEvent& event);
   
private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
   
   // pointer for main Osci object
   Osci *m_osci;
   
   // pointer to DOFrame object
   DOFrame *m_frame;
   
   // fonts
   wxFont m_fontNormal;
   wxFont m_fontFixed;
   wxFont m_fontFixedBold;
   
   // coordinates of total scope area
   int m_sx1, m_sx2, m_sy1, m_sy2;
   
   // coordinates of subpanel area
   int m_x1[MAX_N_BOARDS], m_x2[MAX_N_BOARDS], m_y1[MAX_N_BOARDS], m_y2[MAX_N_BOARDS];

   // split mode
   bool m_splitMode;
   
   // current device context
   wxDC *m_dc;
   
   // stop watch for screen updates
   wxStopWatch m_sw;
   
   // paing mode
   int m_paintMode;
   
   // current board index for drawing
   int m_board;
   
   // curent channel index
   int m_chn;
   
   // offset and size of display area in ns
   int m_screenSize[MAX_N_BOARDS], m_screenOffset[MAX_N_BOARDS];
   
   // cursor variables
   int m_clientHeight, m_clientWidth;
   double m_mouseX;
   double m_mouseY;
   int m_MeasX1, m_MeasX2, m_MeasY1, m_MeasY2;
   int m_BSX1[MAX_N_BOARDS], m_BSX2[MAX_N_BOARDS], m_BSY1[MAX_N_BOARDS], m_BSY2[MAX_N_BOARDS];
   
   double m_xCursorA, m_xCursorB, m_yCursorA, m_yCursorB;
   int m_idxA, m_idxB;
   double m_uCursorA, m_uCursorB, m_tCursorA, m_tCursorB;
   
   // waveform propoerties
   int     m_chnon[MAX_N_BOARDS][4];
   double  m_offset[MAX_N_BOARDS][4];
   int     m_scale[MAX_N_BOARDS][4];
   int     m_hscale[MAX_N_BOARDS];
   
   // math display
   bool    m_mathFlag[2][4];

   // histogram coordinates
   int m_hx1, m_hy1, m_hx2, m_hy2;

   // save button
   int m_savex1, m_savey1, m_savex2, m_savey2;
   
   // histogram x axis
   bool m_histAxisAuto;
   double m_histAxisMin;
   double m_histAxisMax;
   double m_minCursor;
   double m_maxCursor;
   bool m_dragMin, m_dragMax;

   // display properties
   bool m_displayDateTime, m_displayShowGrid, m_displayLines, m_displayScalers;
   int  m_displayMode, m_displayN;
   
   // grid drawing (screen vs. printer)
   void DrawDot(wxDC& dc, wxCoord w, wxCoord h, bool printing);
   
   // find waveform point close to mouse cursor
   bool FindClosestWafeformPoint(int& idx_min, int& x_min, int& y_min);
   
   // optional debug message
   char m_debugMsg[80];
   
};
