/*
$Id: Measurement.h 21271 2014-02-17 15:20:55Z ritt $
*/

class DOScreen;
class DOFrame;

class Measurement
{
protected:
   DOFrame *m_frame;
   wxString m_name;
   int      m_index;
   double   m_value;
   double   m_param[4];
   double   *m_statArray;
   int      m_statIndex;
   int      m_nMeasured;
   int      m_nStat;
   double   m_vsum;
   double   m_vvsum;
   double   m_min;
   double   m_max;
   
public:
   /** Constructor & Desctructor */
   Measurement(DOFrame *frame, int index);
   ~Measurement();

   wxString GetName();
   wxString GetUnit();
   double Measure(double *x1, double *y1, double *x2, double *y2, int n, bool update, DOScreen *s);
   void Measure(double *x1, double *y1, double *x2, double *y2, int n);
   wxString GetString();
   wxString GetStat();
   void SetNStat(int n);
   int GetNStat() { return m_nStat; }
   int GetNMeasured() { return m_nMeasured; }
   void ResetStat();
   double *GetArray() { return m_statArray; }
   void SetParam(int i, double p) { if (i<4) m_param[i] = p; }
   double GetParam(int i) { return m_param[i]; }

   static const int N_MEASUREMENTS = 13;

private:
   double MLevel(double *x1, double *y1, int n, DOScreen *s);
   double MPkPk(double *x1, double *y1, int n, DOScreen *s);
   double MRMS(double *x1, double *y1, int n, DOScreen *s);
   double MVSlice(double *x1, double *y1, int n, DOScreen *s);
   double MCharge(double *x1, double *y1, int n, DOScreen *s);
   double MFreq(double *x1, double *y1, int n, DOScreen *s);
   double MPeriod(double *x1, double *y1, int n, DOScreen *s);
   double MRise(double *x1, double *y1, int n, DOScreen *s);
   double MFall(double *x1, double *y1, int n, DOScreen *s);
   double MPosWidth(double *x1, double *y1, int n, DOScreen *s);
   double MNegWidth(double *x1, double *y1, int n, DOScreen *s);
   double MChnDelay(double *x1, double *y1, double *x2, double *y2, int n, DOScreen *s);
   double MHSlice(double *x1, double *y1, int n, DOScreen *s);
};
