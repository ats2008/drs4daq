/*
 * Measurement.cpp
 * Measuremnet class implementation
 * $Id: Measurement.cpp 21615 2015-02-25 09:31:08Z ritt $
 */

#include "DRSOscInc.h"

void linfit(double *x, double *y, int n, double &a, double &b);

Measurement::Measurement(DOFrame *frame, int index)
{
   m_frame = frame;
   m_index = index;
   memset(m_param, 0, sizeof(m_param));
   m_statIndex = 0;
   m_nMeasured = 0;
   m_nStat = 1000;
   m_statArray = new double[m_nStat];
   m_vsum = m_vvsum = 0;
   m_min = m_max = 0;
   ResetStat();
}

Measurement::~Measurement()
{
   delete m_statArray;
}

void Measurement::ResetStat()
{
   m_nMeasured = 0;
   m_statIndex = 0;
}

void Measurement::SetNStat(int n)
{
   if (n > 1000000)
      n = 1000000;
   if (n < 1)
      n = 1;
   m_nStat = n;
   delete m_statArray;
   m_statArray = new double[n];
   ResetStat();
}

wxString Measurement::GetName()
{
   switch (m_index) {
   case 0: return wxT("Level"); break;
   case 1: return wxT("Pk-Pk"); break;
   case 2: return wxT("RMS"); break;
   case 3: return wxT("VSlice"); break;
   case 4: return wxT("Charge"); break;
   case 5: return wxT("Freq"); break;
   case 6: return wxT("Period"); break;
   case 7: return wxT("Rise"); break;
   case 8: return wxT("Fall"); break;
   case 9: return wxT("Pos Width"); break;
   case 10: return wxT("Neg Width"); break;
   case 11: return wxT("Chn delay"); break;
   case 12: return wxT("HSlice"); break;
   default: return wxT("<undefined>"); break;
   }
}

void Measurement::Measure(double *x1, double *y1, double *x2, double *y2, int n)
{
   Measure(x1, y1, x2, y2, n, true, NULL);
}

double Measurement::Measure(double *x1, double *y1, double *x2, double *y2, int n, bool update, DOScreen *s)
{
   double v;
   int i, na;

   switch (m_index) {
      case 0: v = MLevel(x1, y1, n, s); break;
      case 1: v = MPkPk(x1, y1, n, s); break;
      case 2: v = MRMS(x1, y1, n, s); break;
      case 3: v = MVSlice(x1, y1, n, s); break;
      case 4: v = MCharge(x1, y1, n, s); break;
      case 5: v = MFreq(x1, y1, n, s); break;
      case 6: v = MPeriod(x1, y1, n, s); break;
      case 7: v = MRise(x1, y1, n, s); break;
      case 8: v = MFall(x1, y1, n, s); break;
      case 9: v = MPosWidth(x1, y1, n, s); break;
      case 10: v = MNegWidth(x1, y1, n, s); break;
      case 11: v = MChnDelay(x1, y1, x2, y2, n, s); break;
      case 12: v = MHSlice(x1, y1, n, s); break;
   default: v = 0; break;
   }

   m_value = v;

   /* update statistics */
   if (update && !ss_isnan(v)) {
      m_statArray[m_statIndex] = v;
      m_statIndex = (m_statIndex + 1) % m_nStat;

      if (m_nMeasured < m_nStat) {
         m_nMeasured++;
         na = m_nMeasured;
      } else {
         na = m_nStat;
      }

      m_vsum = m_vvsum = 0;
      m_min = m_max = v;

      for (i=0 ; i<na ; i++) {
         m_vsum += m_statArray[i];
         m_vvsum += (m_statArray[i] * m_statArray[i]);
         if (m_statArray[i] < m_min)
            m_min = m_statArray[i];
         if (m_statArray[i] > m_max)
            m_max = m_statArray[i];
      }
   }

   return v;
}

wxString Measurement::GetString()
{
   wxString str;

   if (ss_isnan(m_value))
      str.Printf(wxT("    N/A"));
   else {
      switch (m_index) {
         case 0:
         case 1:
         case 2:
         case 3: str.Printf(wxT("%6.1lf mV"),  m_value); break;
         case 4: str.Printf(wxT("%6.1lf pC"), m_value); break;
         case 5: str.Printf(wxT("%6.1lf MHz"), m_value); break;
         case 6:
         case 7:
         case 8:
         case 9:
         case 10:
         case 11:
         case 12: str.Printf(wxT("%6.3lf ns"),  m_value); break;
      }
   }

   return str;
}

wxString Measurement::GetStat()
{
   double mean, std;

   if (m_nMeasured == 0) {
      mean = 0;
      std = 0;
   } else {
      mean  = m_vsum / m_nMeasured;
      std = sqrt(m_vvsum/m_nMeasured - m_vsum*m_vsum/m_nMeasured/m_nMeasured);
   }

   wxString str;
   if (ss_isnan(m_min) || ss_isnan(m_max))
      str.Printf(wxT("     N/A      N/A      N/A      N/A %6d"), m_nMeasured);
   else
      str.Printf(wxT("%8.3lf %8.3lf %8.3lf %8.4lf %6d"), m_min, m_max, mean, std, m_nMeasured);
   return str;
}

double Measurement::MLevel(double *x, double *y, int n, DOScreen *s)
{
   double l = 0;
   for (int i=0 ; i<n ; i++)
      l += y[i];
   
   if (n > 0)
      l /= n;

   if (s) {
      s->GetDC()->DrawLine(s->timeToX(x[0]), s->voltToY(l),
                           s->timeToX(x[n-1]), s->voltToY(l));
   }
   return l;
}

double Measurement::MPkPk(double *x, double *y, int n, DOScreen *s)
{
   double min_x, min_y, max_x, max_y;

   min_x = max_x = x[0];
   min_y = max_y = y[0];
   for (int i=0 ; i<n ; i++) {
      if (y[i] < min_y) {
         min_x = x[i];
         min_y = y[i];
      }
      if (y[i] > max_y) {
         max_x = x[i];
         max_y = y[i];
      }
   }
   
   if (s) {
      int x_min = s->timeToX(min_x);
      int x_max = s->timeToX(max_x);
      int y_min = s->voltToY(min_y);
      int y_max = s->voltToY(max_y);

      int x_center = (x_min + x_max) / 2;

      if (x_max > x_min) {
         s->GetDC()->DrawLine(x_min-20, y_min, x_center+20, y_min);
         s->GetDC()->DrawLine(x_center-20, y_max, x_max+20, y_max);
      } else {
         s->GetDC()->DrawLine(x_max-20, y_max, x_center+20, y_max);
         s->GetDC()->DrawLine(x_center-20, y_min, x_min+20, y_min);
      }

      s->GetDC()->DrawLine(x_center, y_max, x_center, y_min);
      s->GetDC()->DrawLine(x_center, y_max, x_center+2, y_max+6);
      s->GetDC()->DrawLine(x_center, y_max, x_center-2, y_max+6);
      s->GetDC()->DrawLine(x_center, y_min, x_center+2, y_min-6);
      s->GetDC()->DrawLine(x_center, y_min, x_center-2, y_min-6);

   }

   return max_y-min_y;
}

double Measurement::MRMS(double *x, double *y, int n, DOScreen *s)
{
   double mean = 0;
   double rms  = 0;

   if (n <= 0)
     return 0;

   for (int i=0 ; i<n ; i++)
      mean += y[i];
   mean /= n;
   for (int i=0 ; i<n ; i++)
      rms += (y[i]-mean)*(y[i]-mean);
   rms = sqrt(rms/n);

   if (s) {
      int ym = s->voltToY(mean);
      for (int i=0 ; i<n ; i++)
         s->GetDC()->DrawLine(s->timeToX(x[i]), ym, s->timeToX(x[i]), s->voltToY(y[i]));
   }

   return rms;
}

double Measurement::MVSlice(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double u  = 0;
   
   if (n <= 0)
      return 0;
   
   for (i=0 ; i<n-1 ; i++)
      if (x[i] <= m_param[0] && x[i+1] > m_param[0])
         break;
   
   if (i == n-1)
      return ss_nan();
   
   if (x[i+1] - x[i] == 0)
      return ss_nan();
   
   u = y[i] + (y[i+1]-y[i]) * (m_param[0] - x[i]) / (x[i+1] - x[i]);
   
   if (s) {
      s->GetDC()->DrawLine(s->timeToX(m_param[0]), s->GetY1(), s->timeToX(m_param[0]), s->GetY2());
   }
   
   return u;
}

double Measurement::MCharge(double *x, double *y, int n, DOScreen *s)
{
   double q = 0;
   double x1, x2, y1, y2;
   
   if (n <= 0)
      return 0;
   
   for (int i=0 ; i<n ; i++) {
      if (x[i+1] >= m_param[0] && x[i] <= m_param[2]) {
         if (x[i] < m_param[0]) {
            x1 = m_param[0];
            y1 = y[i] + (y[i+1]-y[i]) * (m_param[0] - x[i]) / (x[i+1] - x[i]);
         } else {
            x1 = x[i];
            y1 = y[i];
         }
         if (x[i+1] > m_param[2]) {
            x2 = m_param[2];
            y2 = y[i] + (y[i+1]-y[i]) * (m_param[2] - x[i]) / (x[i+1] - x[i]);
         } else {
            x2 = x[i+1];
            y2 = y[i+1];
         }
         
         q += 0.5 * (y1 + y2) * (x2 - x1);
         
         if (s) {
            wxPoint p[4];
            p[0] = wxPoint(s->timeToX(x1), s->voltToY(0));
            p[1] = wxPoint(s->timeToX(x1), s->voltToY(y1));
            p[2] = wxPoint(s->timeToX(x2), s->voltToY(y2));
            p[3] = wxPoint(s->timeToX(x2), s->voltToY(0));
            
            s->GetDC()->DrawPolygon(4, p, 0, 0);
         }
      }
   }
   
   return q / 50; // signal into 50 Ohm
}

/*-------------------------------------------------------------------------*/

double Measurement::MFreq(double *x, double *y, int n, DOScreen *s)
{
   double p = MPeriod(x, y, n, s);

   if (ss_isnan(p) || p == 0)
      return ss_nan();

   return 1000/p;
}

double Measurement::MPeriod(double *x, double *y, int n, DOScreen *s)
{
   int i, pos_edge, n_pos, n_neg;
   double miny, maxy, mean, t1, t2;

   if (n <= 0)
     return 0;

   miny = maxy = y[0];
   mean = 0;
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
      mean += y[i];
   }
   if (n < 5 || maxy - miny < 10)
      return ss_nan();
   
   mean = mean/n;

   /* count zero crossings */
   n_pos = n_neg = 0;
   for (i=1 ; i<n ; i++) {
      if (y[i] > mean && y[i-1] <= mean)
         n_pos++;
      if (y[i] < mean && y[i-1] >= mean)
         n_neg++;
   }

   /* search for zero crossing */
   for (i=n/2+2 ; i>1 ; i--) {
      if (n_pos > 1 && y[i] > mean && y[i-1] <= mean)
         break;
      if (n_neg > 1 && y[i] < mean && y[i-1] >= mean)
         break;
   }
   if (i == 1)
      for (i=n/2 ; i<n ; i++) {
         if (n_pos > 1 && y[i] > mean && y[i-1] <= mean)
            break;
         if (n_neg > 1 && y[i] < mean && y[i-1] >= mean)
            break;
      }
   if (i == n)
      return ss_nan();

   pos_edge = y[i] > mean;

   t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   /* search next zero crossing */
   for (i++ ; i<n ; i++) {
      if (pos_edge && y[i] > mean && y[i-1] <= mean)
         break;
      if (!pos_edge && y[i] < mean && y[i-1] >= mean)
         break;
   }

   if (i == n)
      return ss_nan();

   t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   if (s) {
      int ym = s->voltToY(mean);
      int x1 = s->timeToX(t1);
      int x2 = s->timeToX(t2);
      s->GetDC()->DrawLine(x1, ym-10, x1, ym+10);
      s->GetDC()->DrawLine(x2, ym-10, x2, ym+10);
      s->GetDC()->DrawLine(x1, ym, x2, ym);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym-2);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym+2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym-2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym+2);
   }

   return t2 - t1;
}

double Measurement::MRise(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double miny, maxy, t1, t2, y10, y90;

   if (n <= 0)
     return 0;

   miny = maxy = y[0];
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
   }

   if (maxy - miny < 10)
      return ss_nan();

   y10 = miny+0.1*(maxy-miny);
   y90 = miny+0.9*(maxy-miny);

   /* search for 10% level crossing */
   for (i=n/2+2 ; i>1 ; i--)
      if (y[i] > y10 && y[i-1] <= y10)
         break;
   if (i == 1)
      for (i=n/2 ; i<n ; i++) {
         if (y[i] > y10 && y[i-1] <= y10)
            break;
      }
   if (i == n)
      return ss_nan();

   t1 = (y10*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   /* search for 90% level crossing */
   for (i++ ; i<n ; i++) 
      if (y[i] > y90 && y[i-1] <= y90)
         break;

   if (i == n)
      return ss_nan();

   t2 = (y90*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   if (s) {
      int y1 = s->voltToY(y10);
      int y2 = s->voltToY(y90);
      int x1 = s->timeToX(t1);
      int x2 = s->timeToX(t2);
      int ym = (y1 + y2)/2;

      s->GetDC()->DrawLine(x1, y1+10, x1, ym-10);
      s->GetDC()->DrawLine(x2, y2-10, x2, ym+10);
      s->GetDC()->DrawLine(x1, ym, x2, ym);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym-2);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym+2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym-2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym+2);
   }

   return t2 - t1;
}

double Measurement::MFall(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double miny, maxy, t1, t2, y10, y90;

   if (n <= 0)
     return 0;

   miny = maxy = y[0];
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
   }

   if (maxy - miny < 10)
      return ss_nan();

   y10 = miny+0.1*(maxy-miny);
   y90 = miny+0.9*(maxy-miny);

   /* search for 90% level crossing */
   for (i=n/2+2 ; i>1 ; i--)
      if (y[i] < y90 && y[i-1] >= y90)
         break;
   if (i == 1)
      for (i=n/2 ; i<n ; i++) {
         if (y[i] < y90 && y[i-1] >= y90)
            break;
      }
   if (i == n)
      return ss_nan();

   t1 = (y90*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   /* search for 10% level crossing */
   for (i++ ; i<n ; i++) 
      if (y[i] < y10 && y[i-1] >= y10)
         break;

   if (i == n)
      return ss_nan();

   t2 = (y10*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   if (s) {
      int y1 = s->voltToY(y90);
      int y2 = s->voltToY(y10);
      int x1 = s->timeToX(t1);
      int x2 = s->timeToX(t2);
      int ym = (y1 + y2)/2;

      s->GetDC()->DrawLine(x1, y1-10, x1, ym+10);
      s->GetDC()->DrawLine(x2, y2+10, x2, ym-10);
      s->GetDC()->DrawLine(x1, ym, x2, ym);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym-2);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym+2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym-2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym+2);
   }

   return t2 - t1;
}

double Measurement::MPosWidth(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double miny, maxy, mean, t1, t2;

   if (n <= 0)
     return 0;

   miny = maxy = y[0];
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
   }
   mean = (miny + maxy)/2;

   if (maxy - miny < 10)
      return ss_nan();

   /* search for first pos zero crossing */
   for (i=1 ; i<n ; i++)
      if (y[i] > mean && y[i-1] <= mean)
         break;
   if (i == n)
      return ss_nan();

   t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   /* search next neg zero crossing */
   for (i++ ; i<n ; i++)
      if (y[i] < mean && y[i-1] >= mean)
         break;
   if (i == n)
      return ss_nan();

   t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   if (s) {
      int ym = s->voltToY(mean);
      int x1 = s->timeToX(t1);
      int x2 = s->timeToX(t2);
      s->GetDC()->DrawLine(x1, ym-10, x1, ym+10);
      s->GetDC()->DrawLine(x2, ym-10, x2, ym+10);
      s->GetDC()->DrawLine(x1, ym, x2, ym);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym-2);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym+2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym-2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym+2);
   }

   return t2 - t1;
}

double Measurement::MNegWidth(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double miny, maxy, mean, t1, t2;

   if (n <= 0)
     return 0;

   miny = maxy = y[0];
   for (i=0 ; i<n ; i++) {
      if (y[i] > maxy)
         maxy = y[i];
      if (y[i] < miny)
         miny = y[i];
   }
   mean = (miny + maxy)/2;

   if (maxy - miny < 10)
      return ss_nan();

   /* search for first neg zero crossing */
   for (i=1 ; i<n ; i++)
      if (y[i] < mean && y[i-1] >= mean)
         break;
   if (i == n)
      return ss_nan();

   t1 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   /* search next pos zero crossing */
   for (i++ ; i<n ; i++)
      if (y[i] > mean && y[i-1] <= mean)
         break;
   if (i == n)
      return ss_nan();

   t2 = (mean*(x[i]-x[i-1])+x[i-1]*y[i]-x[i]*y[i-1])/(y[i]-y[i-1]);

   if (s) {
      int ym = s->voltToY(mean);
      int x1 = s->timeToX(t1);
      int x2 = s->timeToX(t2);
      s->GetDC()->DrawLine(x1, ym-10, x1, ym+10);
      s->GetDC()->DrawLine(x2, ym-10, x2, ym+10);
      s->GetDC()->DrawLine(x1, ym, x2, ym);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym-2);
      s->GetDC()->DrawLine(x1, ym, x1+6, ym+2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym-2);
      s->GetDC()->DrawLine(x2, ym, x2-6, ym+2);
   }

   return t2 - t1;
}

#define N_FIT 0

void linfit(double *x, double *y, int n, double &a, double &b)
{
   int i;
   double sx, sxx, sy, syy, sxy;
   
   sx = sxx = sy = syy = sxy = 0;
   
   for (i=0 ; i<n ; i++) {
      sx += x[i];
      sy += y[i];
      sxy += x[i]*y[i];
      sxx += x[i]*x[i];
      syy += y[i]*y[i];
   }
   
   b = (sxy - sx*sy/n) / (sxx - sx*sx/n);
   a = sy/n-b*sx/n;
      
   return;
}

double Measurement::MChnDelay(double *x1, double *y1, double *x2, double *y2, int n, DOScreen *s)
{
   int i, pol, i1l, i1r, i2l, i2r;
   double t1, t2, thr, a, b;

   if (n <= 0)
     return 0;

   thr = m_frame->GetTrgLevel(0) * 1000;
   pol = m_frame->GetTrgPolarity();
   for (i=1 ; i<n ; i++) {
      if (pol == 1 && y1[i] < thr && y1[i-1] >= thr)
         break;
      if (pol == 0 && y1[i] > thr && y1[i-1] <= thr)
         break;
   }
   if (i == n)
      return ss_nan();

   t1 = (thr*(x1[i]-x1[i-1])+x1[i-1]*y1[i]-x1[i]*y1[i-1])/(y1[i]-y1[i-1]);
   
   if (N_FIT > 0 && i>=N_FIT/2) {
      i1l = i-N_FIT/2;
      i1r = i1l+N_FIT-1;
      linfit(&x1[i-N_FIT/2], &y1[i-N_FIT/2], N_FIT, a, b);
      if (b != 0)
         t1 = (thr-a)/b;
      
      if (s) {
         int xa = s->timeToX(x1[i1l]);
         int ya = s->voltToY(s->GetCurChn(), a+b*x1[i1l]);
         int xb = s->timeToX(x1[i1r]);
         int yb = s->voltToY(s->GetCurChn(), a+b*x1[i1r]);
         
         s->GetDC()->DrawLine(xa, ya, xb, yb);
      }
   }
   
   for (i=1 ; i<n ; i++) {
      if (pol == 1 && y2[i] < thr && y2[i-1] >= thr)
         break;
      if (pol == 0 && y2[i] > thr && y2[i-1] <= thr)
         break;
   }
   if (i == n)
      return ss_nan();
   
   t2 = (thr*(x2[i]-x2[i-1])+x2[i-1]*y2[i]-x2[i]*y2[i-1])/(y2[i]-y2[i-1]);

   if (N_FIT > 0 && i>=N_FIT/2) {
      i2l = i-N_FIT/2;
      i2r = i2l+N_FIT-1;
      linfit(&x2[i-N_FIT/2], &y2[i-N_FIT/2], N_FIT, a, b);
      if (b != 0)
         t2 = (thr-a)/b;

      if (s) {
         int xa = s->timeToX(x2[i2l]);
         int ya = s->voltToY(s->GetCurChn(), a+b*x2[i2l]);
         int xb = s->timeToX(x2[i2r]);
         int yb = s->voltToY(s->GetCurChn(), a+b*x2[i2r]);
         
         s->GetDC()->DrawLine(xa, ya, xb, yb);
      }
}

   if (s) {
      if (s->GetChnOn(0, (s->GetCurChn()+1)%4)) { /// ### TBG: change board index
         int ym1 = s->voltToY(s->GetCurChn(), thr);
         int ym2 = s->voltToY((s->GetCurChn()+1)%4, thr);
         int xa = s->timeToX(t1);
         int xb = s->timeToX(t2);
         int ymm = (ym1+ym2)/2;
         if (ym1 < ym2) {
            s->GetDC()->DrawLine(xa, ym1-10, xa, ymm+10);
            s->GetDC()->DrawLine(xb, ymm-10, xb, ym2+10);
         } else {
            s->GetDC()->DrawLine(xa, ym1+10, xa, ymm-10);
            s->GetDC()->DrawLine(xb, ymm+10, xb, ym2-10);
         }
         s->GetDC()->DrawLine(xa, ymm, xb, ymm);
         s->GetDC()->DrawLine(xa, ymm, xa+6, ymm-2);
         s->GetDC()->DrawLine(xa, ymm, xa+6, ymm+2);
         s->GetDC()->DrawLine(xb, ymm, xb-6, ymm-2);
         s->GetDC()->DrawLine(xb, ymm, xb-6, ymm+2);
      }
   }
   
   return t2 - t1;
}

double Measurement::MHSlice(double *x, double *y, int n, DOScreen *s)
{
   int i;
   double tmin = ss_nan();
   double t, dtmin = 1E6;;
   
   if (n <= 0)
      return 0;
   
   for (i=0 ; i<n-1 ; i++) {
      if ((y[i] <= m_param[1] && y[i+1] > m_param[1]) ||
          (y[i] >= m_param[1] && y[i+1] < m_param[1])) {

         if (y[i+1] - y[i] == 0)
            continue;
         t = x[i] + (x[i+1]-x[i]) * (m_param[1] - y[i]) / (y[i+1] - y[i]);
         if (fabs(t - m_param[0]) < dtmin) {
            dtmin = fabs(t - m_param[0]);
            tmin = t;
         }
      }
   }
   
   if (s) {
      s->GetDC()->DrawLine(s->GetX1(), s->voltToY(m_param[1]), s->GetX2(), s->voltToY(m_param[1]));
   }
   
   return tmin;
}



