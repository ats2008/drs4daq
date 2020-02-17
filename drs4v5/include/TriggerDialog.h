#ifndef __TriggerDialog__
#define __TriggerDialog__

/*
$Id: TriggerDialog.h 22292 2016-04-28 10:31:04Z ritt $
*/

class DOFrame;

#include "DRSOsc.h"

/** Implementing TriggerDialog_fb */
class TriggerDialog : public TriggerDialog_fb
{
protected:
   // Handlers for TriggerDialog_fb events.
   void OnClose( wxCommandEvent& event );
   void OnButton( wxCommandEvent& event );
   void OnTriggerLevel( wxCommandEvent& event );
   
public:
   /** Constructor */
   TriggerDialog( wxWindow* parent );
   
   void SetTriggerLevel(double level);
   void SelectBoard(int board);

private:
   DOFrame *m_frame;
   int     m_board;
   void    UpdateControls();
};

#endif // __TriggerDialog__
