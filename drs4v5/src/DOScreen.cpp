/*
 * doscreen.cpp
 * DRS Oscilloscope screen class
 * $Id: DOScreen.cpp 22327 2016-10-11 13:18:26Z ritt $
 */

#include "DRSOscInc.h"

/*------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(DOScreen, wxWindow)
   EVT_PAINT        (DOScreen::OnPaint)
   EVT_SIZE         (DOScreen::OnSize)
   EVT_MOUSE_EVENTS (DOScreen::OnMouse)
   EVT_MOUSEWHEEL   (DOScreen::OnMouse)
END_EVENT_TABLE()

/*------------------------------------------------------------------*/

const int DOScreen::m_scaleTable[10]  = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 };
const int DOScreen::m_hscaleTable[13] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };

/*------------------------------------------------------------------*/

DOScreen::DOScreen(wxWindow *parent, Osci *osci, DOFrame *frame)
        : wxWindow(parent, wxID_ANY)
{
   m_osci = osci;
   m_frame  = frame;
   m_splitMode = 0;
   m_paintMode = kPMWaveform;
   m_histAxisAuto = true;
   m_histAxisMin = m_histAxisMax = 0;
   m_dragMin = m_dragMax = false;
   m_displayDateTime = false;
   m_displayShowGrid = true;
   m_displayLines = true;
   m_displayScalers = false;
   m_displayMode = ID_DISPSAMPLE;
   m_displayN = 16;
   m_clientWidth = m_clientHeight = 0;
   m_mouseX = m_mouseY = 0;
   m_MeasX1 = m_MeasX2 = m_MeasY1 = m_MeasY2 = 0;
   m_board = 0;
   
   for (int b=0 ; b<MAX_N_BOARDS ; b++) {
      m_screenSize[b] = m_screenOffset[b] = 0;
      m_hscale[b] = 0;
      for (int i=0 ; i<4 ; i++) {
         m_chnon[b][i]  = false;
         m_offset[b][i] = 0;
         m_scale[b][i]  = 6;
      }
   }
   memset(m_mathFlag, 0, sizeof(m_mathFlag));
   m_debugMsg[0] = 0;
   
   // create fonts
   
#ifdef _MSC_VER
   m_fontNormal = wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
   m_fontFixed = wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   m_fontFixedBold = wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
#else

#ifdef __MAC_10_9 // fix for OSX Mavericks
   m_fontNormal = wxFont(10, wxFONTFAMILY_DEFAULT,
                             wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("LucidaGrande"));
   m_fontFixed = wxFont(14, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   m_fontFixedBold = wxFont(14, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
#else
   m_fontNormal = wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
   m_fontFixed = wxFont(14, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   m_fontFixedBold = wxFont(14, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
#endif
   
#endif // _MSC_VER
   
   SetBackgroundColour(*wxBLACK);
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}

/*------------------------------------------------------------------*/

DOScreen::~DOScreen()
{
}

/*------------------------------------------------------------------*/

void DOScreen::OnPaint(wxPaintEvent& event)
{
   wxCoord w, h;

   // get size form paint DC (buffered paint DC won't work)
   wxPaintDC pdc(this);
   pdc.GetSize(&w, &h);
   m_clientWidth = w;
   m_clientHeight = h;

   // obtain buffered paint DC to avoid flickering
   wxBufferedPaintDC dc(this);

   if (m_paintMode == kPMWaveform) {
      DrawScope(dc, w, h, false);
      DrawMath(dc, w, h, false);
      DrawHisto(dc, w, h, false);
   } else
      DrawTcalib(dc, w, h, false);
   
   // Change "Save" button
   if (!m_frame->GetWFfd() && !m_frame->GetWFFile())
      m_frame->SetSaveBtn(wxT("Save"), wxT("Save waveforms"));
}

/*------------------------------------------------------------------*/

void DOScreen::DrawDot(wxDC& dc, wxCoord x, wxCoord y, bool printing)
{
   if (printing)
      dc.DrawRectangle(x, y, 1, 1);
   else
      dc.DrawPoint(x, y);
}

/*------------------------------------------------------------------*/

bool DOScreen::FindClosestWafeformPoint(int& idx_min, int& x_min, int& y_min)
{
   int dist, dist_min, x, y;

   // find point on waveform closest to mouse position
   idx_min = -1;
   dist_min = 1000000;

   int b = m_frame->GetCurrentBoard();
   wxPoint mpos((int)(m_x1[b]+m_mouseX*(m_x2[b]-m_x1[b])), (int)(m_y1[b]+m_mouseY*(m_y2[b]-m_y1[b])));

   for (int idx=0 ; idx<4 ; idx++) {
      if (m_chnon[b][idx]) {
         if (m_osci->GetWaveformDepth(idx)) {
            float *wf = m_frame->GetWaveform(b, idx);
            float *time = m_frame->GetTime(b, idx);

            for (int i=0 ; i<m_osci->GetWaveformDepth(idx) ; i++) {

               x = timeToX(time[i]);
               y = voltToY(idx, wf[i]);

               dist = (x-mpos.x)*(x-mpos.x) +
                      (y-mpos.y)*(y-mpos.y);
               if (dist < dist_min) {
                  dist_min = dist;
                  idx_min = idx;
                  x_min = x;
                  y_min = y;
               }
            }
         }
      }
   }

   return dist_min < 2000;
}

/*------------------------------------------------------------------*/

int DOScreen::timeToX(float t)
{
   return (int) ((double)(t-m_screenOffset[m_board])/m_screenSize[m_board]*(m_x2[m_board]-m_x1[m_board])+m_x1[m_board] + 0.5);
}

double DOScreen::XToTime(int x)
{
   return (double) m_screenSize[m_board]*(x - m_x1[m_board])/(m_x2[m_board]-m_x1[m_board])+m_screenOffset[m_board];
}

double DOScreen::GetT1()
{
   return (double) m_screenOffset[m_board];
}

double DOScreen::GetT2()
{
   return (double) m_screenSize[m_board]+m_screenOffset[m_board];
}

int DOScreen::voltToY(float v)
{
   return voltToY(m_chn, v);
}

int DOScreen::voltToY(int chn, float v)
{
   v = v - m_offset[m_board][chn]*1000;
   return (int) ((m_y1[m_board]+m_y2[m_board])/2-v/10.0/m_scaleTable[m_scale[m_board][chn]]*(m_y2[m_board]-m_y1[m_board]) + 0.5);
}

double DOScreen::YToVolt(int y)
{
   return YToVolt(m_chn, y);
}

double DOScreen::YToVolt(int chn, int y)
{
   double v;

   v = (((double) m_y1[m_board]+m_y2[m_board])/2 - y)/(m_y2[m_board]-m_y1[m_board])*m_scaleTable[m_scale[m_board][chn]]*10;
   v = v + m_offset[m_board][chn]*1000;

   return v;
}

/*------------------------------------------------------------------*/

void DOScreen::SetMathDisplay(int id, bool flag)
{
   int m, chn;

   m = (id - ID_PJ1) / 4;
   chn = (id - ID_PJ1) % 4;

   m_mathFlag[m][chn] = flag;
}

/*------------------------------------------------------------------*/

void DOScreen::DrawScopeBottom(wxDC& dc, int board, int x1, int y1, int width, bool printing)
{
   wxString wxst, wxst2;
   int w, h, idx;
   wxPoint p[6];
   
   // show voltage per division
   for (idx=0 ; idx<4 ; idx++) {
      
      dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
      dc.SetBrush(m_frame->GetColor(idx, printing));

      if (m_chnon[board][idx] && !m_frame->IsFirst()) {
         if (m_scaleTable[m_scale[board][idx]] >= 1000)
            wxst.Printf(wxT("%d V/div"), m_scaleTable[m_scale[board][idx]]/1000);
         else
            wxst.Printf(wxT("%d mV/div"), m_scaleTable[m_scale[board][idx]]);
         if (m_chnon[board][idx] == 2)
            wxst+wxT(" CLK");
         dc.GetTextExtent(wxst, &w, &h);
         dc.DrawRoundedRectangle(x1 + 10 + idx*80, y1+3, w+10, 15, 2);
         dc.DrawText(wxst, x1 + 15 + idx*80, y1+3);
      }
   }
   
   // show horizontal resolution
   if (m_hscaleTable[m_hscale[board]] >= 1000)
      wxst.Printf(wxT("%d us/div  "), m_hscaleTable[m_hscale[board]]/1000);
   else
      wxst.Printf(wxT("%d ns/div  "), m_hscaleTable[m_hscale[board]]);
   if (m_frame->GetActSamplingSpeed() < 1)
      wxst2.Printf(wxT("%1.0lf MS/s Calib"), m_frame->GetActSamplingSpeed()*1000);
   else
      wxst2.Printf(wxT("%1.4lg GS/s Calib"), m_frame->GetActSamplingSpeed());
   
   wxst = wxst+wxst2;
   dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
   dc.SetBrush(*wxGREEN);
   dc.SetTextForeground(*wxBLACK);
   dc.GetTextExtent(wxst, &w, &h);
   dc.DrawRoundedRectangle(width-w, y1+3, w+10, 15, 2);
   dc.DrawText(wxst, width+5-w, y1+3);
   
   // cross out calibration if not valid
   if (m_frame->GetOsci()->GetNumberOfBoards() > 0) {
      if (!m_frame->GetOsci()->IsTCalibrated() || !m_frame->GetOsci()->IsTCalOn()) {
         dc.SetPen(wxPen(*wxRED, 1, wxSOLID));
         dc.DrawLine(width-22, y1+10, width+6, y1+10);
      }
   }
   
   // show trigger settings
   int x_start = width - w;
   int tc = m_frame->GetTriggerConfig();
   if (tc > 0) {
      wxString wxst1, wxst2;
      wxst1.Append((char)wxT('('));
      for (int i=0 ; i<5 ; i++)
         if (tc & (1<<i)) {
            if (i < 4)
               wxst1.Append((char) (wxT('1'+i)));
            else
               wxst1.Append((char) wxT('E'));
            wxst1.Append((char) wxT('|'));
         }
      for (int i=0 ; i<5 ; i++)
         if (tc & (1<<(i+8))) {
            if (i < 4)
               wxst1.Append((char) (wxT('1'+i)));
            else
               wxst1.Append((char) wxT('E'));
            wxst1.Append((char) wxT('&'));
         }
      if (wxst1.Length() > 0)
         wxst1 = wxst1.Left(wxst1.Length()-1);
      wxst1.Append((char) wxT(')'));
      wxst2.Printf(wxT(" %1.0lf ns"), m_frame->GetTrgDelay());
      wxst = wxst1+wxst2;
      dc.GetTextExtent(wxst, &w, &h);
      dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
      dc.SetBrush(m_frame->GetColor(4, printing));
      dc.DrawRoundedRectangle(x_start-20-w-20, y1+3, w+10+20, 15, 2);
      dc.DrawText(wxst, x_start-15-w, y1+3);
   } else {
      if (m_frame->GetTrgSource(board) == 4)
         wxst.Printf(wxT("EXT %1.0lf ns"), m_frame->GetTrgDelay());
      else
         wxst.Printf(wxT("%1.3lf V %1.0lf ns"), m_frame->GetTrgLevel(m_frame->GetTrgSource(board)), m_frame->GetTrgDelay());
      dc.GetTextExtent(wxst, &w, &h);
      dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
      dc.SetBrush(m_frame->GetColor(m_frame->GetTrgSource(board), printing));
      dc.DrawRoundedRectangle(x_start-20-w-20, y1+3, w+10+20, 15, 2);
      dc.DrawText(wxst, x_start-15-w, y1+3);
   }
   
   if (m_frame->GetTrgPolarity() == 0) {
      // positive edge
      p[0] = wxPoint(0, 11);
      p[1] = wxPoint(5, 11);
      p[2] = wxPoint(10, 3);
      p[3] = wxPoint(15, 3);
   } else {
      // negative edge
      p[0] = wxPoint(0,  3);
      p[1] = wxPoint(5,  3);
      p[2] = wxPoint(10,11);
      p[3] = wxPoint(15,11);
   }
   dc.SetPen(wxPen(*wxBLACK, 1, wxSOLID));
   dc.DrawLines(4, p, x_start-20-w-15, y1+3);
   
   dc.SetBrush(*wxGREEN);
   dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
   x_start = x_start - 20 - w - 15;
   if (m_osci->IsRunning()) {
      wxst.Printf(wxT("%d Acq/s"), m_frame->GetAcqPerSecond());
      dc.GetTextExtent(wxst, &w, &h);
      dc.DrawRoundedRectangle(x_start-20-w, y1+3, w+10, 15, 2);
      dc.DrawText(wxst, x_start-15-w, y1+3);
   }
   x_start = x_start - 15 - w;
   if (m_frame->GetNSaved()) {
      wxst.Printf(wxT("%d saved"), m_frame->GetNSaved());
      dc.GetTextExtent(wxst, &w, &h);
      dc.DrawRoundedRectangle(x_start-20-w, y1+3, w+10, 15, 2);
      dc.DrawText(wxst, x_start-15-w, y1+3);
   }
}

/*------------------------------------------------------------------*/

void DOScreen::DrawWaveforms(wxDC& dc, int wfIndex, bool printing)
{
   int x, y, x_old = 0, y_old = 0, w, h;
   wxString wxst;
   wxPoint p[6];
   
   dc.DestroyClippingRegion();
   dc.SetPen(*wxGREY_PEN);
   dc.SetBrush(*wxBLACK_BRUSH);
   dc.DrawRectangle(m_x1[m_board], m_y1[m_board], m_x2[m_board]-m_x1[m_board], m_y2[m_board]-m_y1[m_board]);

   if (m_splitMode && m_board == m_frame->GetCurrentBoard()) {
      dc.SetPen(*wxGREEN_PEN);
      dc.DrawRectangle(m_x1[m_board], m_y1[m_board], m_x2[m_board]-m_x1[m_board], m_y2[m_board]-m_y1[m_board]);
      dc.DrawRectangle(m_x1[m_board]+1, m_y1[m_board]+1, m_x2[m_board]-m_x1[m_board]-2, m_y2[m_board]-m_y1[m_board]-2);
   }

   // draw grid
   if (m_displayShowGrid) {
      dc.SetPen(*wxGREY_PEN);
      for (int i=1 ; i<10 ; i++) {
         if (i == 5) {
            for (int j=m_x1[m_board]+1 ; j<m_x2[m_board] ; j+=2)
               DrawDot(dc, j, i*(m_y2[m_board]-m_y1[m_board])/10+m_y1[m_board], printing);
         } else {
            for (int j=1 ; j<50 ; j++)
               DrawDot(dc, m_x1[m_board]+j*(m_x2[m_board]-m_x1[m_board])/50, i*(m_y2[m_board]-m_y1[m_board])/10+m_y1[m_board], printing);
         }
      }
      
      for (int i=1 ; i<50 ; i++) {
         for (int j=-4 ; j<5 ; j+=2)
            DrawDot(dc, m_x1[m_board]+i*(m_x2[m_board]-m_x1[m_board])/50, (m_y2[m_board]-m_y1[m_board])/2+m_y1[m_board]+j, printing);
      }
      
      for (int i=1 ; i<10 ; i++) {
         if (i == 5) {
            for (int j=m_y1[m_board]+1 ; j<m_y2[m_board] ; j+=2)
               DrawDot(dc, i*(m_x2[m_board]-m_x1[m_board])/10+m_x1[m_board], j, printing);
         } else {
            for (int j=1 ; j<50 ; j++)
               DrawDot(dc, i*(m_x2[m_board]-m_x1[m_board])/10+m_x1[m_board], m_y1[m_board]+j*(m_y2[m_board]-m_y1[m_board])/50, printing);
         }
      }
      
      for (int i=1 ; i<50 ; i++) {
         for (int j=-4 ; j<5 ; j+=2)
            DrawDot(dc, (m_x2[m_board]-m_x1[m_board])/2+m_x1[m_board]+j, m_y1[m_board]+i*(m_y2[m_board]-m_y1[m_board])/50, printing);
      }
   }
   
   for (int idx=3 ; idx>=0 ; idx--) {
      if (m_chnon[m_board][idx] && !m_frame->IsFirst()) {
         
         // draw marker
         if (m_board == m_frame->GetCurrentBoard()) {
            dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
            dc.SetBrush(m_frame->GetColor(idx, printing));
            y = (int) ((m_y1[m_board]+m_y2[m_board])/2+m_offset[m_board][idx]*1000/10.0/m_scaleTable[m_scale[m_board][idx]]*(m_y2[m_board]-m_y1[m_board]) + 0.5);
            
            if (y < m_y1[m_board]) {
               wxPoint p[3];
               p[0] = wxPoint(10, 0);
               p[1] = wxPoint(17, 7);
               p[2] = wxPoint( 3, 7);
               dc.DrawPolygon(3, p, 0, m_y1[m_board]);
            } else if (y > m_y2[m_board]) {
               wxPoint p[3];
               p[0] = wxPoint(10, 7);
               p[1] = wxPoint(17, 0);
               p[2] = wxPoint( 3, 0);
               dc.DrawPolygon(3, p, 0, m_y2[m_board]-8);
            } else {
               wxPoint p[7];
               p[0] = wxPoint(0,  6);
               p[1] = wxPoint(1,  7);
               p[2] = wxPoint(12, 7);
               p[3] = wxPoint(19, 0);
               p[4] = wxPoint(12,-7);
               p[5] = wxPoint(1, -7);
               p[6] = wxPoint(0, -6);
               dc.DrawPolygon(7, p, 0, y);
               
               wxst.Printf(wxT("%d"), idx+1);
               dc.GetTextExtent(wxst, &w, &h);
               dc.SetTextForeground(*wxBLACK);
               dc.SetTextBackground(m_frame->GetColor(idx, printing));
               dc.DrawText(wxst, 3, y-h/2);
            }
         }
         
         // draw waveform
         dc.SetClippingRegion(m_x1[m_board]+1, m_y1[m_board]+1, m_x2[m_board]-m_x1[m_board]-2, m_y2[m_board]-m_y1[m_board]-2);
         dc.SetPen(wxPen(m_frame->GetColor(idx, printing), 1, wxSOLID));
         
         if (m_osci->GetWaveformDepth(idx)) {
            float *wf = m_frame->GetWaveform(wfIndex, idx);
            float *time = m_frame->GetTime(wfIndex, idx);
            m_chn = idx;
            int spacing = timeToX(1/m_osci->GetSamplingSpeed()) - timeToX(0);
            
            for (int i=0 ; i<m_osci->GetWaveformDepth(idx) ; i++) {
               x = timeToX(time[i]);
               y = voltToY(wf[i]);
               if (x >= m_x1[m_board] && x <= m_x2[m_board]) {
                  if (m_displayLines) {
                     if (i > 0)
                        dc.DrawLine(x_old, y_old, x, y);
                     if (i > 0 && spacing > 5) {
                        // draw points if separate by more than 5 pixels
                        dc.DrawRectangle(x-1, y-1, 3, 3);
                        dc.DrawPoint(x-2, y);
                        dc.DrawPoint(x+2, y);
                        dc.DrawPoint(x, y-2);
                        dc.DrawPoint(x, y+2);
                     }
                  } else
                     DrawDot(dc, x, y, printing);
               }
               x_old = x;
               y_old = y;
            }
         }

         dc.DestroyClippingRegion();
      }
   }
   
   // draw trigger level
   for (int channel=0 ; channel<4 ; channel++) {
      if (!m_frame->GetTriggerConfig() && m_frame->GetTrgSource(m_board) < 4 && channel != m_frame->GetTrgSource(m_board))
         continue;
      if (!m_frame->GetTriggerConfig() && m_frame->GetTrgSource(m_board) == 4)
         continue;
      
      double v = (m_frame->GetTrgLevel(channel) - m_offset[m_board][channel])*1000;
      y = (int) ((m_y1[m_board]+m_y2[m_board])/2-(v/10.0/m_scaleTable[m_scale[m_board][channel]]*(m_y2[m_board]-m_y1[m_board]) + 0.5));
      
      p[0] = wxPoint(-8,  0);
      p[1] = wxPoint( 0, -5);
      p[2] = wxPoint( 0,  5);
      if (m_frame->GetTriggerConfig() &&
          (m_frame->GetTrgLevel(channel) == m_frame->GetTrgLevel((channel+1) % 4) ||
           m_frame->GetTrgLevel(channel) == m_frame->GetTrgLevel((channel+2) % 4) ||
           m_frame->GetTrgLevel(channel) == m_frame->GetTrgLevel((channel+3) % 4))) {
             dc.SetBrush(m_frame->GetColor(4, printing)); // gray if two levels overlap
             dc.SetPen(m_frame->GetColor(4, printing));
          } else {
             dc.SetBrush(m_frame->GetColor(channel, printing));
             dc.SetPen(m_frame->GetColor(channel, printing));
          }
      dc.DrawPolygon(3, p, m_x2[m_board]-2, y);
      if (time(NULL) - m_frame->GetLastTriggerUpdate() < 2 && !m_frame->IsFirst()) {
         dc.DrawLine(m_x1[m_board], y, m_x2[m_board]-2, y);
      }
   }
   
   // draw trigger horizontal position marker
   p[0] = wxPoint(-5, 0);
   p[1] = wxPoint( 4, 0);
   p[2] = wxPoint( 4, 7);
   p[3] = wxPoint( 0, 11);
   p[4] = wxPoint( -1, 11);
   p[5] = wxPoint(-5, 7);
   dc.SetBrush(m_frame->GetColor(m_frame->GetTrgSource(m_board), printing));
   dc.SetPen(m_frame->GetColor(m_frame->GetTrgSource(m_board), printing));
   dc.DrawPolygon(6, p, (wxCoord)((m_frame->GetTrgPosition(m_board)-GetScreenOffset(m_board)) / GetScreenSize(m_board) * (m_x2[m_board]-m_x1[m_board]) + m_x1[m_board]),
                  (wxCoord)m_y1[m_board]);
   
   wxst = wxT("T");
   dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
   dc.SetBrush(m_frame->GetColor(m_frame->GetTrgSource(m_board), printing));
   dc.SetTextForeground(*wxBLACK);
   dc.GetTextExtent(wxst, &w, &h);
   dc.DrawText(wxst, (wxCoord)((m_frame->GetTrgPosition(m_board)-GetScreenOffset(m_board)) / GetScreenSize(m_board) * (m_x2[m_board]-m_x1[m_board]) + m_x1[m_board] - w/2 - 1),
               (wxCoord)(m_y1[m_board]));
   
}

/*------------------------------------------------------------------*/

void DOScreen::DrawScope(wxDC& dc, wxCoord width, wxCoord height, bool printing)
{
   int x, y, w, h, y_top, n;
   int idx_min, x_min, y_min;
   wxString wxst, wxst2;

   if (m_frame->GetOsci()->SkipDisplay())
      return;

   // tell DOFrame to pull new waveforms from EPThread
   m_frame->UpdateWaveforms();

   m_dc = &dc;
   
   // scope area
   m_sx1 = 20;
   m_sy1 = 1;
   m_sx2 = width-1;
   m_sy2 = height-20;
   
   // if histo display is on, only show waveform in upper half
   if (m_frame->IsHist())
      m_sy2 = height/2-20;

   // waveform area
   m_x1[m_board] = m_sx1;
   m_y1[m_board] = m_sy1;
   m_x2[m_board] = m_sx2;
   m_y2[m_board] = m_sy2;
   
   y_top = 1;
   
   dc.SetFont(m_fontNormal);

   // draw overall frame
   if (printing) {
      dc.SetBackground(*wxWHITE_BRUSH);
      dc.SetPen(*wxWHITE_PEN);
      dc.SetBrush(*wxWHITE_BRUSH);
   } else {
      dc.SetBackground(*wxBLACK_BRUSH);
      dc.SetPen(*wxBLACK_PEN);
      dc.SetBrush(*wxBLACK_BRUSH);
   }

   //static int n_event = 0;
   //if (++n_event % 30 == 0) {
   if (m_frame->IsHist())
      dc.DrawRectangle(0, 0, width, height/2);
   else
      dc.DrawRectangle(0, 0, width, height);
   //}

   DrawScopeBottom(dc, m_frame->GetCurrentBoard(), 20, m_sy2, width-20, printing);
   
   if (m_splitMode) {
      n = m_osci->GetNumberOfBoards();
      w = (m_sx2-m_sx1)/2;
      h = (m_sy2-m_sy1) / (((n-1) / 2)+1);
      
      for (int i=0 ; i<n ; i++) {
         m_x1[i] = m_sx1+(i%2)*w;
         m_y1[i] = m_sy1+(i/2)*h;
         m_x2[i] = m_x1[i]+w-((i+1)%2);
         m_y2[i] = m_y1[i]+h;
         m_board = i;
         DrawWaveforms(dc, i, printing);
      }
   } else {
      m_board = m_frame->GetCurrentBoard();
      m_x1[m_board] = m_sx1;
      m_y1[m_board] = m_sy1;
      m_x2[m_board] = m_sx2;
      m_y2[m_board] = m_sy2;
      
      int wfIndex;
      if (m_osci->IsMultiBoard())
         wfIndex = m_osci->GetCurrentBoardIndex();
      else
         wfIndex = 0;
      
      DrawWaveforms(dc, wfIndex, printing);
   }
   
   // display optional debug messages
   if (*m_frame->GetOsci()->GetDebugMsg()) {
      wxst = wxString((const wxChar *)m_frame->GetOsci()->GetDebugMsg());
      dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
      dc.SetBrush(*wxGREEN);
      dc.SetTextForeground(*wxBLACK);
      dc.GetTextExtent(wxst, &w, &h);
      dc.DrawRoundedRectangle(m_x1[m_board]+2, m_sy1+2, w+5, 15, 2);
      dc.DrawText(wxst, m_sx1+4, m_sy1+2);
   }
   if (m_debugMsg[0]) {
      wxst = wxString((const wxChar *)m_debugMsg);
      dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
      dc.SetBrush(*wxGREEN);
      dc.SetTextForeground(*wxBLACK);
      dc.GetTextExtent(wxst, &w, &h);
      dc.DrawRoundedRectangle(m_sx1+2, m_sy1+2, w+5, 15, 2);
      dc.DrawText(wxst, m_sx1+4, m_sy1+2);
   }
   
   // print "TRIG?" or "AUTO" if no recent trigger
   if (m_osci->GetNumberOfBoards() && m_osci->IsIdle()) {
      dc.SetTextForeground(*wxGREEN);
      if (m_osci->GetTriggerMode() == TM_AUTO)
         wxst = wxT("AUTO");
      else
         wxst = wxT("TRIG?");
      dc.GetTextExtent(wxst, &w, &h);
      if (m_osci->GetNumberOfBoards() > 1)
         dc.DrawText(wxst, m_x2[m_board] - w - 5, m_y1[m_board] + 22);
      else
         dc.DrawText(wxst, m_x2[m_board] - w - 5, m_y1[m_board] + 1);
   }

   // display date/time
   if (m_displayDateTime) {
      dc.SetTextForeground(*wxGREEN);
      wxst.Printf(wxT("%s"),wxDateTime::Now().Format(wxT("%c")).c_str()); 
      dc.GetTextExtent(wxst, &w, &h);
      dc.DrawText(wxst, m_x2[m_board] - w - 2, m_y2[m_board] - h - 1);
   }

   // display board selector buttons
   n = m_osci->GetNumberOfBoards();
   if (n > 1) {
      if (m_osci->IsMultiBoard())
         n++;
      for (int i=0 ; i<n ; i++) {
         m_BSX1[i] = m_sx2-20*(n-i);
         m_BSY1[i] = m_sy1+4;
         m_BSX2[i] = m_BSX1[i]+16;
         m_BSY2[i] = m_BSY1[i]+16;
         
         if (m_osci->IsMultiBoard() && i == n-1) {
            // display cross icon
            dc.SetPen(wxPen(*wxGREEN, 1, wxSOLID));
            if (m_splitMode) {
               dc.SetBrush(*wxGREEN);
               dc.DrawRoundedRectangle(m_BSX1[i], m_BSY1[i], 16, 16, 2);
               dc.SetPen(wxPen(*wxBLACK, 1, wxSOLID));
            } else {
               dc.SetBrush(*wxBLACK);
               dc.DrawRoundedRectangle(m_BSX1[i], m_BSY1[i], 16, 16, 2);
               dc.SetPen(wxPen(*wxGREEN, 1, wxSOLID));
            }
            
            dc.DrawLine(m_BSX1[i]+8, m_BSY1[i], m_BSX1[i]+8, m_BSY2[i]);
            dc.DrawLine(m_BSX1[i], m_BSY1[i]+8, m_BSX2[i], m_BSY1[i]+8);
         } else {
            // display board number
            wxst.Printf(wxT("%d"), i+1);
            if (i == m_osci->GetCurrentBoardIndex() && !m_splitMode) {
               dc.SetPen(wxPen(*wxGREEN, 1, wxSOLID));
               dc.SetBrush(*wxGREEN);
               dc.SetTextForeground(*wxBLACK);
            } else {
               dc.SetPen(wxPen(*wxGREEN, 1, wxSOLID));
               dc.SetBrush(*wxBLACK);
               dc.SetTextForeground(*wxGREEN);
            }
            dc.DrawRoundedRectangle(m_BSX1[i], m_BSY1[i], 16, 16, 2);
            dc.DrawText(wxst, m_BSX1[i]+4, m_BSY1[i]+2);
         }
      }
   }

   // display cursors and measurements only for current board
   m_board = m_frame->GetCurrentBoard();
   
   // display cursor
   if (width > 0 && height > 0) {
      int b = m_board;
      wxPoint mpos((int)(m_x1[b]+m_mouseX*(m_x2[b]-m_x1[b])), (int)(m_y1[b]+m_mouseY*(m_y2[b]-m_y1[b])));
      if (m_frame->IsCursorA() || m_frame->IsCursorB()) {

         // find point on waveform closest to mouse position
         FindClosestWafeformPoint(idx_min, x_min, y_min);
         if (m_frame->IsSnap()) {
            x = x_min;
            y = y_min;
         } else {
            x = mpos.x;
            y = mpos.y;
         }

         if (m_frame->ActiveCursor() == 1) {
            m_idxA = idx_min;
            m_xCursorA = (double)(x - m_x1[b])/(m_x2[b]-m_x1[b]);
            m_yCursorA = (double)(y - m_y1[b])/(m_y2[b]-m_y1[b]);
            m_uCursorA = YToVolt(y);
            m_tCursorA = XToTime(x);
         } else if (m_frame->ActiveCursor() == 2) {
            m_idxB = idx_min;
            m_xCursorB = (double)(x - m_x1[b])/(m_x2[b]-m_x1[b]);
            m_yCursorB = (double)(y - m_y1[b])/(m_y2[b]-m_y1[b]);
            m_uCursorB = YToVolt(y);
            m_tCursorB = XToTime(x);
         }

         // draw cursor A
         if (m_frame->IsCursorA() && (m_frame->ActiveCursor() != 1 || (m_mouseX > 0 && m_mouseX < 1 && m_mouseY > 0 && m_mouseY < 1))) {
            dc.SetPen(wxPen(m_frame->GetColor(m_idxA, printing), 1, wxSHORT_DASH));
            int x = (int)(m_x1[b]+m_xCursorA*(m_x2[b]-m_x1[b]));
            int y = (int)(m_y1[b]+m_yCursorA*(m_y2[b]-m_y1[b]));
            dc.DrawLine(x, m_y1[b], x, m_y2[b]);
            dc.DrawLine(m_x1[b], y, m_x2[b], y);
            dc.DrawLine(x-3, y, x, y+3);
            dc.DrawLine(x, y+3, x+3, y);
            dc.DrawLine(x+3, y, x, y-3);
            dc.DrawLine(x, y-3, x-3, y);

            dc.SetTextForeground(*wxGREEN);
            dc.GetTextExtent(wxT("Cursor A:"), &w, &h);
            dc.DrawText(wxT("Cursor A:"), m_x1[b] + 3, y_top);
            wxString str;
            str.Printf(wxT("%1.1lf ns / %1.1lf mV"), m_tCursorA, m_uCursorA); 
            dc.SetTextForeground(m_frame->GetColor(m_idxA, printing));
            dc.DrawText(str, m_x1[b] + w + 10, y_top);
            y_top += h;
         }

         // draw cursor B
         if (m_frame->IsCursorB() && (m_frame->ActiveCursor() != 2 || (m_mouseX > 0 && m_mouseX < 1 && m_mouseY > 0 && m_mouseY < 1))) {
            dc.SetPen(wxPen(m_frame->GetColor(m_idxB, printing), 1, wxDOT));
            int x = (int)(m_x1[b]+m_xCursorB*(m_x2[b]-m_x1[b]));
            int y = (int)(m_y1[b]+m_yCursorB*(m_y2[b]-m_y1[b]));
            dc.DrawLine(x, m_y1[b], x, m_y2[b]);
            dc.DrawLine(m_x1[b], y, m_x2[b], y);
            dc.DrawLine(x-3, y, x, y+3);
            dc.DrawLine(x, y+3, x+3, y);
            dc.DrawLine(x+3, y, x, y-3);
            dc.DrawLine(x, y-3, x-3, y);
            
            dc.SetTextForeground(*wxGREEN);
            dc.GetTextExtent(wxT("Cursor B:"), &w, &h);
            dc.DrawText(wxT("Cursor B:"), m_x1[b] + 3, y_top);
            wxString str;
            str.Printf(wxT("%1.1lf ns / %1.1lf mV"), m_tCursorB, m_uCursorB); 
            dc.SetTextForeground(m_frame->GetColor(m_idxB, printing));
            dc.DrawText(str, m_x1[b] + w + 10, y_top);
            y_top += h;
         }

         // cursor difference
         if (m_frame->IsCursorA() && m_frame->IsCursorB()) {
            dc.SetTextForeground(*wxGREEN);
            dc.GetTextExtent(wxT("Diff:"), &w, &h);
            dc.DrawText(wxT("Diff:"), m_x1[b] + 3, y_top);
            wxString str;
            str.Printf(wxT("%1.3lf ns / %1.1lf mV"), m_tCursorB - m_tCursorA, m_uCursorB - m_uCursorA);
            dc.DrawText(str, m_x1[b] + w + 10, y_top);
            y_top += h;
         }

         y_top += h; // leave some space
      }
   }

   // count measurements
   for (int idx = n = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
      for (int chn=0 ; chn<4 ; chn++) {
         if (m_frame->GetMeasurement(idx, chn) != NULL)
            n++;
      }
   }

   // display measurements
   dc.SetFont(m_fontFixed);
   dc.GetTextExtent(wxT("Chn delay [CH1]: "), &w, &h);
   int nameWidth = w;
   dc.GetTextExtent(wxT("-00.000 MHz"), &w, &h);
   int valueWidth = w;

   if (m_frame->IsStat() && n > 0) {
      dc.SetTextForeground(*wxGREEN);
      dc.DrawText(wxT("     Min      Max     Mean      Std      N"), m_x1[m_board]+nameWidth+valueWidth, y_top);
      dc.GetTextExtent(wxT("     Min      Max     Mean      Std      N"), &w, &h);
      m_MeasX1 = m_x1[m_board] + nameWidth + valueWidth;
      m_MeasX2 = m_MeasX1 + w;
      y_top += h;
   }

   m_MeasY1 = y_top;
   for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
      for (int chn=0 ; chn<4 ; chn++) {
         Measurement *m;
         m = m_frame->GetMeasurement(idx, chn);
         m_chn = chn;
         
         if (m != NULL) {
            // set parameters from cursor positions
            m->SetParam(0, m_tCursorA);
            m->SetParam(1, m_uCursorA);
            m->SetParam(2, m_tCursorB);
            m->SetParam(3, m_uCursorB);

            dc.SetTextForeground(m_frame->GetColor(chn, printing));
            wxString str;
            str.Printf(wxT("%s [CH%d]:"), m_frame->GetMeasurementName(idx).c_str(), chn+1);
            dc.DrawText(str, m_x1[m_board] + 3, y_top);

            double x1[2048], y1[2048], x2[2048], y2[2048];

            int wfIndex;
            if (m_osci->IsMultiBoard())
               wfIndex = m_osci->GetCurrentBoardIndex();
            else
               wfIndex = 0;

            if (m_osci->GetWaveformDepth(chn)) {
               float *wf1 = m_frame->GetWaveform(wfIndex, chn);
               float *wf2 = m_frame->GetWaveform(wfIndex, (chn+1)%4);
               float *time1 = m_frame->GetTime(wfIndex, chn);
               float *time2 = m_frame->GetTime(wfIndex, (chn+1)%4);
               int   n_inside = 0;

               for (int i=0 ; i<m_osci->GetWaveformDepth(chn) ; i++) {
                  if (timeToX(time1[i]) >= m_x1[m_board]) {
                     x1[n_inside] = time1[i];
                     y1[n_inside] = wf1[i];
                     x2[n_inside] = time2[i];
                     y2[n_inside] = wf2[i];
                     n_inside++;
                  }
                  if (timeToX(time1[i]) > m_x2[m_board])
                     break;
               }

               dc.SetPen(wxPen(m_frame->GetColor(chn, printing), 1, wxSOLID));

               m->Measure(x1, y1, x2, y2, n_inside, false, m_frame->IsIndicator() && m_chnon[m_board][chn] ? this : NULL);
               dc.DrawText(m->GetString(), m_x1[m_board]+nameWidth, y_top+1);

               if (m_frame->IsStat()) {
                  dc.DrawText(m->GetStat(), m_x1[m_board]+nameWidth+valueWidth, y_top+1);
               }
            }

            y_top += h;
         }
      }
   }

   m_MeasY2 = y_top;
   
   // display scalers
   if (m_displayScalers) {
      y_top += 5;
      for (int chn=0 ; chn<5 ; chn++) {
         if (chn == 4)
            dc.SetTextForeground(*wxLIGHT_GREY);
         else
            dc.SetTextForeground(m_frame->GetColor(chn, printing));
         unsigned int s = m_osci->GetScaler(chn);
         if (chn == 4) {
            if (s < 1000)
               wxst.Printf(wxT("FreqT: %d Hz"), s);
            else if (s < 1000000)
               wxst.Printf(wxT("FreqT: %1.3lf kHz"), s/1000.0);
            else
               wxst.Printf(wxT("FreqT: %1.3lf MHz"), s/1000000.0);
         } else {
            if (s < 1000)
               wxst.Printf(wxT("Freq%d: %d Hz"), chn+1, s);
            else if (s < 1000000)
               wxst.Printf(wxT("Freq%d: %1.3lf kHz"), chn+1, s/1000.0);
            else
               wxst.Printf(wxT("Freq%d: %1.3lf MHz"), chn+1, s/1000000.0);
         }
         dc.GetTextExtent(wxst, &w, &h);
         dc.DrawText(wxst, m_x1[m_board] + 3, y_top);
         y_top += h;
      }
   }
   
   dc.DestroyClippingRegion();
}

/*------------------------------------------------------------------*/

void DOScreen::DrawHisto(wxDC& dc, wxCoord width, wxCoord height, bool printing)
{
   int i, x, y, x_old, y_old, w, h, y_top;
   wxString wxst, wxst2;
   int histo[1000], uflow, oflow;
   
   if (m_frame->GetOsci()->SkipDisplay())
      return;
   
   if (!m_frame->IsHist())
      return;

   m_dc = &dc;
   
   // histo area
   m_hx1 = 20;
   m_hy1 = height/2+1;
   m_hx2 = width-1;
   m_hy2 = height-20;
   
   x_old = y_old = 0;
   y_top = 1;
   
   /* if all waveform channels are off, use full screen */
   if (!m_chnon[m_board][0] && !m_chnon[m_board][1] && !m_chnon[m_board][2] && !m_chnon[m_board][3])
      m_sy1 = 1;
   
   // set font for all text output
   dc.SetFont(m_fontFixedBold);
   
   // draw overall frame
   if (printing) {
      dc.SetBackground(*wxWHITE_BRUSH);
      dc.SetPen(*wxWHITE_PEN);
      dc.SetBrush(*wxWHITE_BRUSH);
   } else {
      dc.SetBackground(*wxBLACK_BRUSH);
      dc.SetPen(*wxBLACK_PEN);
      dc.SetBrush(*wxBLACK_BRUSH);
   }
   
   dc.DrawRectangle(0, height/2, width, height/2);
   dc.SetPen(*wxGREY_PEN);
   dc.DrawRectangle(m_hx1, m_hy1, m_hx2-m_hx1, m_hy2-m_hy1);
   
   if (m_histAxisAuto) {
      bool first = true;
      for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
         for (int chn=0 ; chn<4 ; chn++) {
            Measurement *m = m_frame->GetMeasurement(idx, chn);
            if (m == NULL)
               continue;
            
            /* find min and max of array */
            double *a = m->GetArray();
            int n = m->GetNMeasured();
            
            if (first) {
               m_histAxisMin = a[0];
               m_histAxisMax = a[0];
               first = false;
            }
            for (i=1 ; i<n ; i++) {
               if (a[i] < m_histAxisMin)
                  m_histAxisMin = a[i];
               if (a[i] > m_histAxisMax)
                  m_histAxisMax = a[i];
            }
         }
      }
   }
   
   for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
      for (int chn=0 ; chn<4 ; chn++) {
         Measurement *m = m_frame->GetMeasurement(idx, chn);
         if (m == NULL)
            continue;
         
         /* build histogram */
         double *a = m->GetArray();
         int n = m->GetNMeasured();
         
         int nBins = (int) (1.5*sqrt((double)n));
         if (nBins > 1000)
            nBins = 1000;
         
         for (i=0 ; i<nBins ; i++)
            histo[i] = 0;
         uflow = oflow = 0;

         /* fill data into histo */
         for (i=0 ; i<n ; i++) {
            int bin = (int)((a[i]-m_histAxisMin)/(m_histAxisMax-m_histAxisMin)*(nBins-1));
            if (bin >= nBins)
               oflow++;
            else if (bin < 0)
               uflow++;
            else
               histo[bin]++;
         }

         int hmax = histo[0];
         for (i=1 ; i<nBins ; i++) {
            if (histo[i] > hmax)
               hmax = histo[i];
         }
         
         /* draw histo */
         dc.SetPen(wxPen(m_frame->GetColor(chn, printing), 1, wxSOLID));
         for (i=0 ; i<nBins ; i++) {
            x = m_hx1+(int)((double)i/nBins*(m_hx2-m_hx1));
            y = m_hy2-(int)((double)histo[i]/hmax*(m_hy2-m_hy1-30));
            if (i>0) {
               dc.DrawLine(x_old, y_old, x, y_old);
               dc.DrawLine(x, y_old, x, y);
            }
            x_old = x;
            y_old = y;
         }
         dc.DrawLine(x_old, y_old, m_hx2, y_old);
         
         // underflow and overflow bins
         dc.SetBrush(wxBrush(m_frame->GetColor(chn, printing)));

         if (uflow) {
            y = (int)((double)uflow/hmax*(m_hy2-m_hy1-1));
            if (y > m_hy2-m_hy1-1)
               y = m_hy2-m_hy1-1;
            dc.DrawRectangle(m_hx1, m_hy2-y, 3, y);
         }
         if (oflow) {
            y = (int)((double)oflow/hmax*(m_hy2-m_hy1-1));
            if (y > m_hy2-m_hy1-1)
               y = m_hy2-m_hy1-1;
            dc.DrawRectangle(m_hx2-3, m_hy2-y, 3, y);
         }
         dc.SetBrush(*wxBLACK_BRUSH);
      }
   }

   DrawHAxis(dc, m_hx1, m_hy2, m_hx2-m_hx1, 3, 4, 5, 7, 0, m_histAxisMin, m_histAxisMax);
   
   // display statistics
   dc.SetFont(m_fontFixed);
   y_top = m_hy1 + 1;
   if (m_frame->IsStat()) {
      dc.SetTextForeground(*wxGREEN);
      dc.DrawText(wxT("     Min      Max     Mean      Std      N"), m_hx1, y_top);
      dc.GetTextExtent(wxT("N"), &w, &h);
      y_top += h;
      
      for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
         for (int chn=0 ; chn<4 ; chn++) {
            Measurement *m = m_frame->GetMeasurement(idx, chn);
            if (m == NULL)
               continue;
            
            double *a = m->GetArray();
            int nValues = m->GetNMeasured();
            
            double sa = 0;
            double sa2 = 0;
            int n = 0;
            double mean = 0;
            double std = 0;
            for (i=0 ; i<nValues ; i++) {
               if (a[i] >= m_histAxisMin && a[i] <= m_histAxisMax) {
                  sa += a[i];
                  sa2 += a[i]*a[i];
                  n++;
               }
            }
            
            if (n) {
               mean = sa / n;
               std = sqrt(sa2/n - sa*sa/n/n);
            }

            wxString str;
            if (n == 0)
               str.Printf(wxT("     N/A      N/A      N/A      N/A %6d"), 0);
            else
               str.Printf(wxT("%8.3lf %8.3lf %8.3lf %8.4lf %6d"), m_histAxisMin,
                          m_histAxisMax, mean, std, n);
            dc.SetTextForeground(m_frame->GetColor(chn, printing));
            dc.DrawText(str, m_hx1, y_top);
            y_top += h;
         }
      }
   }
   
   dc.SetPen(*wxGREY_PEN);

   if (m_dragMin) {
      x = (int) ((m_minCursor - m_histAxisMin) / (m_histAxisMax - m_histAxisMin) * (m_hx2-m_hx1) + m_hx1);
      dc.DrawLine(x, m_hy1, x, m_hy2);
   } else
      x = m_hx1;
   
   // dragging handles
   y = (m_hy1+m_hy2)/2;
   dc.DrawLine(x, y-10, x+10, y-10);
   dc.DrawEllipticArc(x+5, y-10, 10, 10, 0, 90);
   dc.DrawLine(x+15, y-5, x+15, y+5);
   dc.DrawEllipticArc(x+5, y, 10, 10, 270, 360);
   dc.DrawLine(x, y+10, x+10, y+10);
   
   dc.DrawLine(x+5, y-6, x+5, y+6);
   dc.DrawLine(x+10, y-6, x+10, y+6);

   if (m_dragMax) {
      x = (int) ((m_maxCursor - m_histAxisMin) / (m_histAxisMax - m_histAxisMin) * (m_hx2-m_hx1) + m_hx1);
      dc.DrawLine(x, m_hy1, x, m_hy2);
   } else
      x = m_hx2;

   y = (m_hy1+m_hy2)/2;
   dc.DrawLine(x, y-10, x-10, y-10);
   dc.DrawEllipticArc(x-15, y-10, 10, 10, 90, 180);
   dc.DrawLine(x-15, y-5, x-15, y+5);
   dc.DrawEllipticArc(x-15, y, 10, 10, 180, 270);
   dc.DrawLine(x, y+10, x-10, y+10);
   
   dc.DrawLine(x-5, y-6, x-5, y+6);
   dc.DrawLine(x-10, y-6, x-10, y+6);

   // zoom out box
   if (!m_histAxisAuto) {
      dc.DrawRoundedRectangle((m_hx1+m_hx2)/2-15, m_hy2-10, 30, 14, 5);
      dc.DrawLine((m_hx1+m_hx2)/2-10, m_hy2-3, (m_hx1+m_hx2)/2+10, m_hy2-3);
      dc.DrawLine((m_hx1+m_hx2)/2-10, m_hy2-3, (m_hx1+m_hx2)/2-6, m_hy2-7);
      dc.DrawLine((m_hx1+m_hx2)/2-10, m_hy2-3, (m_hx1+m_hx2)/2-6, m_hy2+1);
      dc.DrawLine((m_hx1+m_hx2)/2+10, m_hy2-3, (m_hx1+m_hx2)/2+6, m_hy2-7);
      dc.DrawLine((m_hx1+m_hx2)/2+10, m_hy2-3, (m_hx1+m_hx2)/2+6, m_hy2+1);
   }
   
   // save button
   wxString wxs("Save");
   dc.GetTextExtent(wxs, &w, &h);
   m_savex1 = m_hx2-w-8;
   m_savey1 = m_hy1+4;
   m_savex2 = m_hx2-4;
   m_savey2 = m_savey1+h+2;
   dc.SetPen(wxPen(*wxGREEN, 1, wxSOLID));
   dc.SetBrush(*wxBLACK);
   dc.SetTextForeground(*wxGREEN);
   dc.DrawRoundedRectangle(m_savex1, m_savey1, m_savex2-m_savex1, m_savey2-m_savey1, 2);
   dc.DrawText(wxs, m_savex1+2, m_savey1+2);

}

/*------------------------------------------------------------------*/

void DOScreen::SaveHisto(int fh)
{
   int i;
   wxString wxst, wxst2;
   int histo[1000], uflow, oflow;
   double mmin, mmax;
   
   if (m_histAxisAuto) {
      bool first = true;
      for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
         for (int chn=0 ; chn<4 ; chn++) {
            Measurement *m = m_frame->GetMeasurement(idx, chn);
            if (m == NULL)
               continue;
            
            /* find min and max of array */
            double *a = m->GetArray();
            int n = m->GetNMeasured();
            
            if (first) {
               m_histAxisMin = a[0];
               m_histAxisMax = a[0];
               first = false;
            }
            for (i=1 ; i<n ; i++) {
               if (a[i] < m_histAxisMin)
                  m_histAxisMin = a[i];
               if (a[i] > m_histAxisMax)
                  m_histAxisMax = a[i];
            }
         }
      }
   }
   
   for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
      for (int chn=0 ; chn<4 ; chn++) {
         Measurement *m = m_frame->GetMeasurement(idx, chn);
         if (m == NULL)
            continue;
         
         wxst.Printf(wxT("%s [CH%d]\r\n\r\n"), m_frame->GetMeasurementName(idx).c_str(), chn+1);
         write(fh, wxst.c_str(), wxst.length());

         /* build histogram */
         double *a = m->GetArray();
         int n = m->GetNMeasured();
         
         int nBins = (int) (1.5*sqrt((double)n));
         if (nBins > 1000)
            nBins = 1000;
         
         for (i=0 ; i<nBins ; i++)
            histo[i] = 0;
         uflow = oflow = 0;
         
         /* fill data into histo */
         mmin = mmax = a[0];
         for (i=0 ; i<n ; i++) {
            int bin = (int)((a[i]-m_histAxisMin)/(m_histAxisMax-m_histAxisMin)*(nBins-1));
            if (bin >= nBins)
               oflow++;
            else if (bin < 0)
               uflow++;
            else
               histo[bin]++;
            if (a[i] > mmax)
               mmax = a[i];
            if (a[i] < mmax)
               mmin = a[i];
         }
         
         int hmax = histo[0];
         for (i=1 ; i<nBins ; i++) {
            if (histo[i] > hmax)
               hmax = histo[i];
         }
         
         if (uflow) {
            wxst.Printf(wxT("%8.3lf\t%8.3lf\t%d\r\n"), mmin, m_histAxisMin, uflow);
            write(fh, wxst.c_str(), wxst.length());
         }

         /* save histo */
         for (i=0 ; i<nBins ; i++) {
            double x1 = (double)i/(nBins-1)*(m_histAxisMax-m_histAxisMin)+m_histAxisMin;
            double x2 = (double)(i+1)/(nBins-1)*(m_histAxisMax-m_histAxisMin)+m_histAxisMin;

            wxst.Printf(wxT("%8.3lf\t%8.3lf\t%d\r\n"), x1, x2, histo[i]);
            write(fh, wxst.c_str(), wxst.length());
         }
         
         if (oflow) {
            wxst.Printf(wxT("%8.3lf\t%8.3lf\t%d\r\n"), m_histAxisMax, mmax, oflow);
            write(fh, wxst.c_str(), wxst.length());
         }
         wxst.Printf(wxT("\r\n\r\n"));
         write(fh, wxst.c_str(), wxst.length());
      }
   }
}

/*------------------------------------------------------------------*/

#define LN10 2.302585094
#define LOG2 0.301029996
#define LOG5 0.698970005

/* min/max/abs macros */
#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

void DOScreen::DrawHAxis(wxDC &dc, int x1, int y1, int width,
           int minor, int major, int text, int label, int grid, double xmin, double xmax)
{
   double dx, int_dx, frac_dx, x_act, label_dx, major_dx, x_screen, maxwidth;
   int tick_base, major_base, label_base, n_sig1, n_sig2, xs, w, h;
   wxString str;
   double base[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 };
   
   if (xmax <= xmin || width <= 0)
      return;
   
   /* use 5 as min tick distance */
   dx = (xmax - xmin) / (double) (width / 5);
   
   frac_dx = modf(log(dx) / LN10, &int_dx);
   if (frac_dx < 0) {
      frac_dx += 1;
      int_dx -= 1;
   }
   
   tick_base = frac_dx < LOG2 ? 1 : frac_dx < LOG5 ? 2 : 3;
   major_base = label_base = tick_base + 1;
   
   /* rounding up of dx, label_dx */
   dx = pow(10, int_dx) * base[tick_base];
   major_dx = pow(10, int_dx) * base[major_base];
   label_dx = major_dx;
   
   /* number of significant digits */
   if (xmin == 0)
      n_sig1 = 0;
   else
      n_sig1 = (int) floor(log(fabs(xmin)) / LN10) - (int) floor(log(fabs(label_dx)) / LN10) + 1;
   
   if (xmax == 0)
      n_sig2 = 0;
   else
      n_sig2 =
      (int) floor(log(fabs(xmax)) / LN10) - (int) floor(log(fabs(label_dx)) / LN10) + 1;
   
   n_sig1 = MAX(n_sig1, n_sig2);
   n_sig1 = MAX(n_sig1, 4);
   
   /* determination of maximal width of labels */
   str.Printf(wxT("%1.*lG"), n_sig1, floor(xmin / dx) * dx);
   dc.GetTextExtent(str, &w, &h);
   maxwidth = h / 2 * str.Len();
   str.Printf(wxT("%1.*lG"), n_sig1, floor(xmax / dx) * dx);
   maxwidth = MAX(maxwidth, h / 2 * str.Len());
   str.Printf(wxT("%1.*lG"), n_sig1, floor(xmax / dx) * dx + label_dx);
   maxwidth = MAX(maxwidth, h / 2 * str.Len());
   
   /* increasing label_dx, if labels would overlap */
   while (maxwidth > 0.7 * label_dx / (xmax - xmin) * width) {
      label_base++;
      label_dx = pow(10, int_dx) * base[label_base];
      if (label_base % 3 == 2 && major_base % 3 == 1) {
         major_base++;
         major_dx = pow(10, int_dx) * base[major_base];
      }
   }
   
   x_act = floor(xmin / dx) * dx;
   
   dc.DrawLine(x1, y1, x1 + width, y1);
   
   do {
      x_screen = (x_act - xmin) / (xmax - xmin) * width + x1;
      xs = (int) (x_screen + 0.5);
      
      if (x_screen > x1 + width + 0.001)
         break;
      
      if (x_screen >= x1) {
         if (fabs(floor(x_act / major_dx + 0.5) - x_act / major_dx) <
             dx / major_dx / 10.0) {
            
            if (fabs(floor(x_act / label_dx + 0.5) - x_act / label_dx) <
                dx / label_dx / 10.0) {
               /* label tick mark */
               dc.DrawLine(xs, y1, xs, y1 + text);
               
               /* grid line */
               if (grid != 0 && xs > x1 && xs < x1 + width)
                  dc.DrawLine(xs, y1, xs, y1 + grid);
               
               /* label */
               if (label != 0) {
                  str.Printf(wxT("%1.*lG"), n_sig1, x_act);
                  dc.GetTextExtent(str, &w, &h);
                  dc.DrawText(str, (int) xs - w / 2, y1 + label);
               }
            } else {
               /* major tick mark */
               dc.DrawLine(xs, y1, xs, y1 + major);
               
               /* grid line */
               if (grid != 0 && xs > x1 && xs < x1 + width)
                  dc.DrawLine(xs, y1 - 1, xs, y1 + grid);
            }
            
         } else
            /* minor tick mark */
            dc.DrawLine(xs, y1, xs, y1 + minor);
         
      }
      
      x_act += dx;
      
      /* supress 1.23E-17 ... */
      if (fabs(x_act) < dx / 100)
         x_act = 0;
      
   } while (1);
}

/*------------------------------------------------------------------*/

void DOScreen::DrawTcalib(wxDC& dc, wxCoord width, wxCoord height, bool printing)
{
   int x, y, x_old, y_old;
   double tscale;
   wxString str;

   m_dc = &dc;
   m_x1[m_board] = 20;
   m_y1[m_board] = 1;
   m_x2[m_board] = width-1;
   m_y2[m_board] = height-20;
   x_old = y_old = 0;
   tscale = 20/m_osci->GetCurrentBoard()->GetNominalFrequency();

   // draw overall frame
   if (printing) {
      dc.SetBackground(*wxWHITE_BRUSH);
      dc.SetPen(*wxWHITE_PEN);
      dc.SetBrush(*wxWHITE_BRUSH);
   } else {
      dc.SetBackground(*wxBLACK_BRUSH);
      dc.SetPen(*wxBLACK_PEN);
      dc.SetBrush(*wxBLACK_BRUSH);
   }

   dc.DrawRectangle(0, 0, width, height);
   dc.SetPen(*wxGREY_PEN);
   dc.DrawRectangle(m_x1[m_board], m_y1[m_board], m_x2[m_board]-m_x1[m_board], m_y2[m_board]-m_y1[m_board]);

   // draw grid
   if (m_displayShowGrid) {
      dc.SetPen(*wxGREY_PEN);
      for (int i=1 ; i<10 ; i++) {
         if (i == 5) {
            for (int j=m_x1[m_board]+1 ; j<m_x2[m_board] ; j+=2)
               DrawDot(dc, j, i*(m_y2[m_board]-m_y1[m_board])/10+m_y1[m_board], printing);
         } else {
            for (int j=1 ; j<50 ; j++)
               DrawDot(dc, m_x1[m_board]+j*(m_x2[m_board]-m_x1[m_board])/50, i*(m_y2[m_board]-m_y1[m_board])/10+m_y1[m_board], printing);
         }
      }

      for (int i=1 ; i<50 ; i++) {
         for (int j=-4 ; j<5 ; j+=2)
            DrawDot(dc, m_x1[m_board]+i*(m_x2[m_board]-m_x1[m_board])/50, (m_y2[m_board]-m_y1[m_board])/2+m_y1[m_board]+j, printing);
      }

      for (int i=1 ; i<10 ; i++) {
         if (i == 5) {
            for (int j=m_y1[m_board]+1 ; j<m_y2[m_board] ; j+=2)
               DrawDot(dc, i*(m_x2[m_board]-m_x1[m_board])/10+m_x1[m_board], j, printing);
         } else {
            for (int j=1 ; j<50 ; j++)
               DrawDot(dc, i*(m_x2[m_board]-m_x1[m_board])/10+m_x1[m_board], m_y1[m_board]+j*(m_y2[m_board]-m_y1[m_board])/50, printing);
         }
      }

      for (int i=1 ; i<50 ; i++) {
         for (int j=-4 ; j<5 ; j+=2)
            DrawDot(dc, (m_x2[m_board]-m_x1[m_board])/2+m_x1[m_board]+j, m_y1[m_board]+i*(m_y2[m_board]-m_y1[m_board])/50, printing);
      }
      
      dc.SetFont(m_fontFixedBold);
      dc.SetTextForeground(*wxWHITE);
      dc.DrawText(wxT("ns"), m_x1[m_board] - 15, m_y1[m_board] + 10);
      for (int i=(int)(-tscale*5) ; i<=tscale*5 ; i++) {
         int w, h;
         str.Printf(wxT("%1.1lf"), -i*tscale/10.0);
         dc.GetTextExtent(str, &w, &h);
         dc.DrawText(str, m_x1[m_board] - 2 - w, m_y1[m_board]+(5+i)*(m_y2[m_board]-m_y1[m_board])/10 - h/2);
      }

   }
   
   // draw progress bar
   dc.SetBrush(*wxGREEN);
   dc.SetPen(*wxWHITE_PEN);
   dc.DrawRoundedRectangle(m_x1[m_board], m_y2[m_board]+3, (int)((m_x2[m_board]-m_x1[m_board])*m_frame->GetProgress()/100.0), 6, 2);

   // draw waveform in background
   dc.SetClippingRegion(m_x1[m_board]+1, m_y1[m_board]+1, m_x2[m_board]-m_x1[m_board]-2, m_y2[m_board]-m_y1[m_board]-2);
   dc.SetPen(*wxMEDIUM_GREY_PEN);
   dc.SetTextForeground(*wxLIGHT_GREY);

   if (m_frame->GetProgress() < 100) {
      if (m_osci->GetCurrentBoard()->GetBoardType() == 6)
         str.Printf(wxT("Calibration waveform chip %d"), m_osci->GetChip()+1);
      else
         str.Printf(wxT("Calibration waveform"));
      dc.DrawText(str, m_x1[m_board] + 10, m_y1[m_board] + 10);
      
      if (m_osci->GetWaveformDepth(3)) {
         float wf[2048];
         int tc = m_osci->GetCurrentBoard()->GetStopCell(m_osci->GetChip());
         
         if (m_osci->GetCurrentBoard()->GetBoardType() == 9)
            m_osci->GetCurrentBoard()->GetWave(m_osci->GetChip(), 0, wf, true, tc, 0, false);
         else
            m_osci->GetCurrentBoard()->GetWave(m_osci->GetChip(), 8, wf, true, tc, 0, false);
   
         float *time = m_osci->GetTime(0, 3);

         for (int i=0 ; i<m_osci->GetWaveformDepth(3) ; i++) {
            x = timeToX(time[i]);
            y = voltToY(wf[i]);
            if (i > 0)
               dc.DrawLine(x_old, y_old, x, y);
            x_old = x;
            y_old = y;
         }
      }
   }

   // draw timing calibration array
   dc.SetClippingRegion(m_x1[m_board]+1, m_y1[m_board]+1, m_x2[m_board]-m_x1[m_board]-2, m_y2[m_board]-m_y1[m_board]-2);
   
   if (m_osci->GetWaveformDepth(3)) {
      float time[2048];

      if (m_osci->GetTimeCalibration(0, 0, 0, time, true)) {

         /* differential nonlinearity */
         dc.SetPen(*wxGREEN_PEN);
         dc.SetTextForeground(*wxGREEN);
         dc.DrawText(wxT("Effective bin width"), m_x1[m_board] + 10, m_y1[m_board] + 30);
         for (int i=1 ; i<m_osci->GetWaveformDepth(3) ; i++) {
            x = (int)((double)i/m_osci->GetWaveformDepth(3)*(m_x2[m_board]-m_x1[m_board])+m_x1[m_board] + 0.5);

            y = (int) ((m_y1[m_board]+m_y2[m_board])/2-time[i]/tscale*(m_y2[m_board]-m_y1[m_board]) + 0.5);

            if (i > 1)
               dc.DrawLine(x_old, y_old, x, y);
            x_old = x;

            y_old = y;
         }

         /* integral nonlinearity */
         dc.SetPen(*wxRED_PEN);
         dc.SetTextForeground(*wxRED);
         dc.DrawText(wxT("Integral nonlinearity"), m_x1[m_board] + 10, m_y1[m_board] + 50);
         m_osci->GetTimeCalibration(0, 0, 1, time, true);
         for (int i=0 ; i<m_osci->GetWaveformDepth(3) ; i++) {
            x = (int)((double)i/m_osci->GetWaveformDepth(3)*(m_x2[m_board]-m_x1[m_board])+m_x1[m_board] + 0.5);

            y = (int) ((m_y1[m_board]+m_y2[m_board])/2-time[i]/tscale*(m_y2[m_board]-m_y1[m_board]) + 0.5);

            if (i > 0)
               dc.DrawLine(x_old, y_old, x, y);
            x_old = x;
            y_old = y;
         }
      }
   }
   
   dc.DestroyClippingRegion();
}

/*------------------------------------------------------------------*/

void DOScreen::DrawMath(wxDC& dc, wxCoord width, wxCoord height, bool printing)
{
   for (int math=0 ; math < 2 ; math++)
      for (int idx=3 ; idx>=0 ; idx--)
         if (m_mathFlag[math][idx]) {
            if (math == 0)
               DrawPeriodJitter(dc, idx, printing);
         }
}

/*------------------------------------------------------------------*/

void DOScreen::DrawPeriodJitter(wxDC& dc, int idx, bool printing)
{
   int i, j, w, h, n_pos, n_neg, xs, ys, x_old, y_old;
   double miny, maxy, mean;
   double t_pos[1000], t_neg[1000], t_average[1000], t_delta[1000];
   wxString str;

   dc.SetFont(m_fontFixed);
   dc.SetClippingRegion(m_x1[m_board]+1, m_y1[m_board]+1, m_x2[m_board]-m_x1[m_board]-2, m_y2[m_board]-m_y1[m_board]-2);
   dc.SetPen(wxPen(m_frame->GetColor(idx, printing), 1, wxSOLID));

   float *y = m_frame->GetWaveform(0, idx);
   float *x = m_frame->GetTime(0, idx);
   int n = m_osci->GetWaveformDepth(idx);

   miny = maxy = y[0];
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
   }
   mean = (miny + maxy)/2;

   if (maxy - miny < 50)
      return;

   /* search zero crossings */
   n_pos = n_neg = 0;
   for (i=1 ; i<n ; i++) {
      if (y[i] > mean && y[i-1] <= mean) {
         t_pos[n_pos++] = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]); 
      }
      if (y[i] < mean && y[i-1] >= mean && n_pos > 0) {
         t_neg[n_neg++] = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]); 
      }
   }

   if (n_pos == 0 || n_neg == 0)
      return;

   for (n=i=j=0 ;  ; i++,j++) {
      if (i+1 >= n_pos)
         break;
      t_average[n] = (t_pos[i+1] + t_pos[i]) / 2;
      t_delta[n++] = t_pos[i+1] - t_pos[i];
      if (j+1 >= n_neg)
         break;
      t_average[n] = (t_neg[j+1] + t_neg[j]) / 2;
      t_delta[n++] = t_neg[j+1] - t_neg[j];
   }

   for (i=0,mean=0 ; i<n ; i++)
      mean += t_delta[i];
   mean /= n;

   x_old = y_old = 0;
   for (i=0 ; i<n ; i++) {
      xs = timeToX(t_average[i]);
      ys = (int) ((m_y1[m_board]+m_y2[m_board])/2-100*(t_delta[i] - mean)/10.0*(m_y2[m_board]-m_y1[m_board]) + 0.5);

      if (i > 0)
         dc.DrawLine(x_old, y_old, xs, ys);
      x_old = xs;
      y_old = ys;
   }

   str.Printf(wxT("%1.3lf ns"), mean);
   dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxSOLID));
   dc.SetTextForeground(*wxGREEN);
   dc.SetBrush(*wxBLACK);
   dc.GetTextExtent(str, &w, &h);
   dc.DrawRoundedRectangle(m_x1[m_board]+1, m_y1[m_board]+(m_y2[m_board]-m_y1[m_board])/2 - h/2, w+5, h, 2);
   dc.DrawText(str, m_x1[m_board] + 3, m_y1[m_board]+(m_y2[m_board]-m_y1[m_board])/2 - h/2);

   str.Printf(wxT("+10 ps"));
   dc.GetTextExtent(str, &w, &h);
   dc.DrawRoundedRectangle(m_x1[m_board]+1, m_y1[m_board]+4*(m_y2[m_board]-m_y1[m_board])/10 - h/2, w+5, h, 2);
   dc.DrawText(str, m_x1[m_board]+3, m_y1[m_board]+4*(m_y2[m_board]-m_y1[m_board])/10 - h/2);

   str.Printf(wxT("-10 ps"));
   dc.GetTextExtent(str, &w, &h);
   dc.DrawRoundedRectangle(m_x1[m_board]+1, m_y1[m_board]+6*(m_y2[m_board]-m_y1[m_board])/10 - h/2, w+5, h, 2);
   dc.DrawText(str, m_x1[m_board]+3, m_y1[m_board]+6*(m_y2[m_board]-m_y1[m_board])/10 - h/2);
}

/*------------------------------------------------------------------*/

void DOScreen::OnSize(wxSizeEvent& event)
{
   wxWindow::Refresh();
}

/*------------------------------------------------------------------*/

void DOScreen::SetScale(int b, int idx, int scale)
{
   m_scale[b][idx] = scale;
}

/*------------------------------------------------------------------*/

void DOScreen::SetHScale(int b, int hscale)
{ 
   m_hscale[b] = hscale;
   m_screenSize[b] = m_hscaleTable[hscale] * 10;
}

/*------------------------------------------------------------------*/

void DOScreen::SetHScaleInc(int b, int increment)
{
   int s = m_hscale[b] += increment;
   if (s < 0)
      s = 0;

   /* limit to min 500 ns/div */
   if (s > 8)
      s = 8;

   SetHScale(b, s);
}

/*------------------------------------------------------------------*/

void DOScreen::OnMouse(wxMouseEvent& event)
{ 
   wxSize s;
   int x, y, i, b;

   if (m_clientHeight > 21 && m_clientWidth > 21) {
      m_mouseX = (double) (event.GetPosition().x - m_x1[m_frame->GetCurrentBoard()]) /
                          (m_x2[m_frame->GetCurrentBoard()] - m_x1[m_frame->GetCurrentBoard()]);
      m_mouseY = (double) (event.GetPosition().y - m_y1[m_frame->GetCurrentBoard()]) /
                          (m_y2[m_frame->GetCurrentBoard()] - m_y1[m_frame->GetCurrentBoard()]);
   }

   bool hand = false;
   bool sizewe = false;
   
   x = event.GetPosition().x;
   y = event.GetPosition().y;
   if (x > m_MeasX1 && x < m_MeasX2 &&
       y > m_MeasY1 && y < m_MeasY2) {
      hand = true;
      if (event.LeftDown())
         m_frame->StatReset();
   }
   
   for (int i=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
      if (x > m_BSX1[i] && x < m_BSX2[i] &&
          y > m_BSY1[i] && y < m_BSY2[i]) {
         hand = true;
         if (event.LeftDown()) {
            m_splitMode = false;
            m_frame->SetSplitMode(false);
            m_frame->SelectBoard(i); // select different board
            m_frame->SetCursorA(false);
            m_frame->SetCursorB(false);
         }
         break;
      }
   }

   if (m_osci->IsMultiBoard()) {
      int i = m_osci->GetNumberOfBoards();
      if (x > m_BSX1[i] && x < m_BSX2[i] &&
          y > m_BSY1[i] && y < m_BSY2[i]) {
         hand = true;
         if (event.LeftDown()) {
            m_splitMode = true;
            m_frame->SetSplitMode(true);
            m_frame->SetCursorA(false);
            m_frame->SetCursorB(false);
         }
      }
   }

   int tip = 0;
   if (m_frame->IsHist()) {
      // save button
      if (x > m_savex1 && x < m_savex2 && y > m_savey1 && y < m_savey2) {
         tip = 4;
         if (event.LeftDown())
            m_frame->SaveHisto();
      }
      // left handle
      if (x > m_hx1 && x < m_hx1+15 && y > (m_hy1+m_hy2)/2-10 && y < (m_hy1+m_hy2)/2+10) {
         hand = true;
         tip = 1;
         if (event.LeftDown()) {
            m_minCursor = m_histAxisMin;
            m_histAxisAuto = false;
            m_dragMin = true;
         }
      }
      // right handle
      if (x > m_hx2-15 && x < m_hx2 && y > (m_hy1+m_hy2)/2-10 && y < (m_hy1+m_hy2)/2+10) {
         hand = true;
         tip = 2;
         if (event.LeftDown()) {
            m_maxCursor = m_histAxisMax;
            m_histAxisAuto = false;
            m_dragMax = true;
         }
      }
      // auto scale button
      if (!m_histAxisAuto && x > (m_hx1+m_hx2)/2-15 && x < (m_hx1+m_hx2)/2+15 &&
         y > m_hy2-10 && y < m_hy2+4) {
         hand = true;
         tip = 3;
         if (event.LeftDown()) {
            m_histAxisAuto = true;
         }
      }
      
      if (event.Dragging() && m_dragMin) {
         m_minCursor = m_histAxisMin + (m_histAxisMax - m_histAxisMin) * (x - m_hx1)/(m_hx2 - m_hx1);
         sizewe = true;
      }
      if (event.LeftUp() && m_dragMin) {
         m_histAxisMin = m_minCursor;
         m_dragMin = false;
      }
      if (event.Dragging() && m_dragMax) {
         m_maxCursor = m_histAxisMin + (m_histAxisMax - m_histAxisMin) * (x - m_hx1)/(m_hx2 - m_hx1);
         sizewe = true;
      }
      if (event.LeftUp() && m_dragMax) {
         m_histAxisMax = m_maxCursor;
         m_dragMax = false;
      }
   }
   
   if (tip == 1)
      this->SetToolTip(wxT("Drag right to set minimum"));
   else if (tip == 2)
      this->SetToolTip(wxT("Drag left to set maximum"));
   else if (tip == 3)
      this->SetToolTip(wxT("Auto zoom to show all values"));
   else if (tip == 4)
      this->SetToolTip(wxT("Save histogram"));
   else
      this->SetToolTip(NULL);
   
   if (sizewe)
      SetCursor(wxCURSOR_SIZEWE);
   else if (hand)
      SetCursor(wxCURSOR_HAND);
   else
      SetCursor(wxNullCursor);

   if (event.LeftDown()) {
      this->SetFocus();
      
      if (m_splitMode) {
         for (i=b=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
            if (x > m_x1[i] && x < m_x2[i] &&
                y > m_y1[i] && y < m_y2[i]) {
               b = i;
               break;
            }
         }
         if (b != m_frame->GetCurrentBoard()) {
            m_frame->SelectBoard(b);
         }
      }
      
      if (x > m_x1[m_frame->GetCurrentBoard()] && x < m_x2[m_frame->GetCurrentBoard()] &&
          y > m_y1[m_frame->GetCurrentBoard()] && y < m_y2[m_frame->GetCurrentBoard()]) {
         if (m_frame->ActiveCursor() == 1)
            m_frame->SetActiveCursor(0);
         else if (m_frame->ActiveCursor() == 2)
            m_frame->SetActiveCursor(0);
         else {
            if (m_frame->IsCursorA() && !m_frame->IsCursorB())
               m_frame->SetActiveCursor(1);
            else if (!m_frame->IsCursorA() && m_frame->IsCursorB())
               m_frame->SetActiveCursor(2);
            else if (m_frame->IsCursorA() && m_frame->IsCursorB()){
               // activate cursor close to mouse pointer
               double d1 = (m_xCursorA-m_mouseX)*(m_xCursorA-m_mouseX) +
               (m_yCursorA-m_mouseY)*(m_yCursorA-m_mouseY);
               double d2 = (m_xCursorB-m_mouseX)*(m_xCursorB-m_mouseX) +
               (m_yCursorB-m_mouseY)*(m_yCursorB-m_mouseY);
               m_frame->SetActiveCursor(d1 < d2 ? 1 : 2);
            }
         }
      }
   }

   if (event.RightDown()) {
      m_frame->ToggleControls();
   }

   wxWindow::Refresh();
}

