/*
 * AboutDialog.cpp
 * About Dialog class
 * $Id: AboutDialog.cpp 21911 2015-11-23 07:31:04Z ritt $
 */

#include "DRSOscInc.h"

extern char svn_revision[];
extern char drsosc_version[];

AboutDialog::AboutDialog(wxWindow* parent)
:
AboutDialog_fb( parent )
{
   wxString str;
   char d[80];

   str.Printf(wxT("Version %s"), (const wxChar*)wxString::FromAscii(drsosc_version));
   m_stVersion->SetLabel(str);

   strcpy(d, svn_revision+23);
   d[10] = 0;
   str.Printf(wxT("Build %d, %s"), atoi(svn_revision+17), (const wxChar*)wxString::FromAscii(d));
   m_stBuild->SetLabel(str);
}
