#ifndef __MeasureDialog__
#define __MeasureDialog__

/*
$Id: MeasureDialog.h 18557 2011-10-28 13:21:44Z ritt $
*/

class DOFrame;

#include "DRSOsc.h"

/** Implementing MeasureDialog_fb */
class MeasureDialog : public MeasureDialog_fb
{
protected:
   // Handlers for MeasureDialog_fb events.
   void OnClose( wxCommandEvent& event );
   void OnButton( wxCommandEvent& event );
   void OnStat( wxCommandEvent& event );
   void OnHist( wxCommandEvent& event );
   void OnStatNAverage( wxCommandEvent& event );
   void OnIndicator( wxCommandEvent& event );
   void OnStatReset( wxCommandEvent& event );
   
public:
   /** Constructor */
   MeasureDialog( wxWindow* parent );

private:
   DOFrame *m_frame;
};

#endif // __MeasureDialog__
