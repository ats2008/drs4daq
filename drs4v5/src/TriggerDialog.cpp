/*
 * TriggerDialog.cpp
 * Modal Trigger Configuration Dialog class
 * $Id: TriggerDialog.cpp 22292 2016-04-28 10:31:04Z ritt $
 */

#include "DRSOscInc.h"

TriggerDialog::TriggerDialog( wxWindow* parent )
:
TriggerDialog_fb( parent )
{
   m_frame = (DOFrame *)parent;
   m_board = 0;

   UpdateControls();
}

void TriggerDialog::UpdateControls()
{
   if (!m_frame->IsTrgConfigEnabled()) {
      m_cbOR1->Disable();
      m_cbOR2->Disable();
      m_cbOR3->Disable();
      m_cbOR4->Disable();
      m_cbOREXT->Disable();
      
      m_cbAND1->Disable();
      m_cbAND2->Disable();
      m_cbAND3->Disable();
      m_cbAND4->Disable();
      m_cbANDEXT->Disable();
      
      m_cbTrans->Disable();
      
      m_tbLevel1->Disable();
      m_tbLevel2->Disable();
      m_tbLevel3->Disable();
      m_tbLevel4->Disable();
   } else {
      m_cbOR1->Enable();
      m_cbOR2->Enable();
      m_cbOR3->Enable();
      m_cbOR4->Enable();
      m_cbOREXT->Enable();
      
      m_cbAND1->Enable();
      m_cbAND2->Enable();
      m_cbAND3->Enable();
      m_cbAND4->Enable();
      m_cbANDEXT->Enable();
      
      m_cbTrans->Enable();
      
      m_tbLevel1->Enable();
      m_tbLevel2->Enable();
      m_tbLevel3->Enable();
      m_tbLevel4->Enable();

      int tc = m_frame->GetTriggerConfig();
      m_cbOR1->SetValue((tc & (1<<0))>0);
      m_cbOR2->SetValue((tc & (1<<1))>0);
      m_cbOR3->SetValue((tc & (1<<2))>0);
      m_cbOR4->SetValue((tc & (1<<3))>0);
      m_cbOREXT->SetValue((tc & (1<<4))>0);
      
      m_cbAND1->SetValue((tc & (1<<8))>0);
      m_cbAND2->SetValue((tc & (1<<9))>0);
      m_cbAND3->SetValue((tc & (1<<10))>0);
      m_cbAND4->SetValue((tc & (1<<11))>0);
      m_cbANDEXT->SetValue((tc & (1<<12))>0);
      
      m_cbTrans->SetValue((tc & (1<<15))>0);
      
      wxString s;
      s.Printf(wxT("%1.3lf"), m_frame->GetTrgLevel(0));
      m_tbLevel1->SetValue(s);
      s.Printf(wxT("%1.3lf"), m_frame->GetTrgLevel(1));
      m_tbLevel2->SetValue(s);
      s.Printf(wxT("%1.3lf"), m_frame->GetTrgLevel(2));
      m_tbLevel3->SetValue(s);
      s.Printf(wxT("%1.3lf"), m_frame->GetTrgLevel(3));
      m_tbLevel4->SetValue(s);
   }
}

void TriggerDialog::OnClose( wxCommandEvent& event )
{
   this->Hide();
}

void TriggerDialog::OnButton( wxCommandEvent& event )
{
   if (event.GetId() == ID_TRANS) {
      DRSBoard *b = m_frame->GetOsci()->GetCurrentBoard();
      if (b->GetFirmwareVersion() < 21699) {
         wxMessageBox(wxT("For this operation a boards with firmware\nrevision >= 21699 is required"),
                      wxT("DRS Oscilloscope"), wxOK | wxICON_STOP, this);
         m_cbTrans->SetValue(false);
         return;
      }
      if (event.IsChecked()) {
         m_cbOREXT->SetValue(false);
         m_cbOREXT->Disable();
         m_cbANDEXT->SetValue(false);
         m_cbANDEXT->Disable();
      } else {
         m_cbOREXT->Enable();
         m_cbANDEXT->Enable();
      }
   }
   m_frame->SetTriggerConfig(event.GetId(), event.IsChecked()); 
}

void TriggerDialog::OnTriggerLevel( wxCommandEvent& event )
{
   if (event.GetId() == ID_LEVEL1)
      m_frame->SetTrgLevel(0, atof(m_tbLevel1->GetValue().mb_str()));
   if (event.GetId() == ID_LEVEL2)
      m_frame->SetTrgLevel(1, atof(m_tbLevel2->GetValue().mb_str()));
   if (event.GetId() == ID_LEVEL3)
      m_frame->SetTrgLevel(2, atof(m_tbLevel3->GetValue().mb_str()));
   if (event.GetId() == ID_LEVEL4)
      m_frame->SetTrgLevel(3, atof(m_tbLevel4->GetValue().mb_str()));
}

void TriggerDialog::SetTriggerLevel(double level)
{
   wxString s;
   s.Printf(wxT("%1.3lf"), level);
   m_tbLevel1->SetValue(s);
   m_tbLevel2->SetValue(s);
   m_tbLevel3->SetValue(s);
   m_tbLevel4->SetValue(s);
}

void TriggerDialog::SelectBoard(int board)
{
   m_board = board;
   UpdateControls();
}
