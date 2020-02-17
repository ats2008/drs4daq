/*
 * Osci.cpp
 * DRS oscilloscope main class
 * $Id: DOFrame.cpp 22327 2016-10-11 13:18:26Z ritt $
 */

#define MULTI_THREAD_READOUT

#include "DRSOscInc.h"
#include "strlcpy.h"

#include "pos.xpm"
#include "neg.xpm"

#ifndef O_TEXT
#define O_TEXT 0
#define O_BINARY 0
#endif

char svn_revision[] = "$Id: DOFrame.cpp 22327 2016-10-11 13:18:26Z ritt $";

char drsosc_version[] = "5.0.6";

// critical section between main and event processing thread
wxCriticalSection *g_epcs;

/*------------------------------------------------------------------*/

class MyPrintout: public wxPrintout
{
public:
   MyPrintout(DOScreen *screen, const wxChar *title):wxPrintout(title) { m_screen = screen; }
   bool OnPrintPage(int page);
private:
   DOScreen *m_screen;
};

/*------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(DOFrame, wxFrame)
   EVT_TIMER        (wxID_ANY, DOFrame::OnTimer )
   EVT_MENU         (wxID_EXIT, DOFrame::OnExit )
END_EVENT_TABLE()

DOFrame::DOFrame( wxWindow* parent )
:
DOFrame_fb( parent )
{
   // colors for four channels
   m_color[0] = wxColor(255, 255,   0); // yellow
   m_color[1] = wxColor(170, 170, 255); // light blue
   m_color[2] = wxColor(255, 150, 150); // light red
   m_color[3] = wxColor(150, 255, 150); // light green
   m_color[4] = wxColor(170, 170, 170); // grey for ext trigger

   // colors for printing
   m_pcolor[0] = wxColor(128, 128,   0); // dark yellow
   m_pcolor[1] = wxColor(  0,   0, 255); // blue
   m_pcolor[2] = wxColor(255,   0,   0); // red
   m_pcolor[3] = wxColor(  0, 255,   0); // green
   m_pcolor[4] = wxColor(128, 128, 128); // grey for ext trigger

   // initialize variables
   m_acqPerSecond = 0;
   m_lastTriggerUpdate = time(NULL);
   m_reqSamplingSpeed = 1;
   m_WFFile = NULL;
   m_WFfd = 0;
   m_nSaved = 0;
   m_actCursor = 0;
   m_cursorA = m_cursorB = false;
   m_snap = false;
   m_hideControls = false;
   m_stat = true;
   m_hist = false;
   m_indicator = true;
   m_first = true;
   m_freqLocked = false;
   m_nStat = 1000;
   m_progress = 0;
   m_oldIdle = false;

   m_configDialog = NULL;
   m_measureDialog = NULL;
   m_triggerDialog = NULL;
   m_displayDialog = NULL;
   m_epthread = NULL;
   g_epcs = NULL;

   // initialize source
   m_board = 0;
   m_firstChannel = 0;
   m_chnSection = 0;
   m_multiBoard = false;
   m_splitMode = false;

   // initialize settings
   m_trgCorr = true;
   
   for (int b=0 ; b<MAX_N_BOARDS ; b++) {
      m_refClk[b] = false; // internal Refclk
      m_HScale[b] = 4;  // 20 ns / div.
      m_HOffset[b] = 0;

      m_trgMode[b] = TM_AUTO;
      m_trgSource[b] = 0; // CH1
      m_trgLevel[b][0] = m_trgLevel[b][1] = m_trgLevel[b][2] = m_trgLevel[b][3] = 0.25;
      m_trgDelay[b] = 0;
      m_trgDelayNs[b] = 0;
      m_trgNegative[b] = false;
      m_trgConfig[b] = 0;
      m_trgConfigEnabled[b] = true;
      
      for (int i=0 ; i<4 ; i++) {
         m_chnOn[b][i] = false;
         m_chnOffset[b][i] = 0;
         m_chnScale[b][i] = 6; // 100 mV / div
      }
      
      m_chnOn[b][0] = true;
      m_range[b] = 0;
   }
   
   m_clkOn = false;
   m_spikeRemoval = true;
   m_displayScalers = false;

   // overwrite settings from configuration file
   LoadConfig(m_xmlError, sizeof(m_xmlError));

   // create Osci object
#ifdef MULTI_THREAD_READOUT
   m_osci = new Osci(m_reqSamplingSpeed, true);
#else
   m_osci = new Osci(m_reqSamplingSpeed, false);
#endif

   // update sampling speed from calibration of first board if not in configuration
   if (m_reqSamplingSpeed == 0)
      m_reqSamplingSpeed = ((int)(m_osci->GetTrueSamplingSpeed()*100+0.5))/100.0;
   else
      m_osci->SetSamplingSpeed(m_reqSamplingSpeed);

   // turn off multi board mode if only one board present
   if (m_osci->GetNumberOfBoards() < 2) {
      m_multiBoard = false;
      m_splitMode = false;
   }
   
   // turn off transparent mode for boards with old firmware
   for (int i=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
      if (m_osci->GetBoard(i)->GetFirmwareVersion() < 21699)
         m_trgConfig[i] &= ~(1<<15);
   }

   // set section (needed before ConfigDialog) to display 2048-bin mode
   if (m_osci->GetNumberOfBoards() > 0 && m_osci->GetBoard(0)->Is2048ModeCapable())
      m_chnSection = 2;
   else
      m_chnSection = 0;
   m_osci->SelectChannel(m_firstChannel, m_chnSection);

   // create Measurement objects
   for (int i=0 ; i<Measurement::N_MEASUREMENTS ; i++)
      for (int chn=0 ; chn<4 ; chn++) {
         m_measurement[i][chn] = new Measurement(this, i);
         m_measFlag[i][chn] = false;
      }

   // create screen object
   m_screen = new DOScreen(m_pnScreen, m_osci, this);
   wxBoxSizer *vbox1 = new wxBoxSizer(wxVERTICAL);
   vbox1->Add(m_screen, 1, wxEXPAND);
   m_pnScreen->SetSizer(vbox1);

#ifdef _MSC_VER
   /* adjust font size under Windows */
   m_stScale1->SetFont(wxFont(8, wxFONTFAMILY_DEFAULT,
                              wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
   m_stScale2->SetFont(wxFont(8, wxFONTFAMILY_DEFAULT,
                              wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
   m_stScale3->SetFont(wxFont(8, wxFONTFAMILY_DEFAULT,
                              wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
   m_stScale4->SetFont(wxFont(8, wxFONTFAMILY_DEFAULT,
                              wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));   
#endif
   
   // create timer
   m_timer = new wxTimer(this);

   // create modeless dialog boxes
   m_configDialog = new ConfigDialog(this);
   m_measureDialog = new MeasureDialog(this);
   m_triggerDialog = new TriggerDialog(this);
   m_displayDialog = new DisplayDialog(this);

   // set position of dialog boxes conveniently
   int w, h, cw, ch, mw, mh, dw, dh, x, y;
   wxDisplaySize(&dw, &dh);
   m_configDialog->GetSize(&cw, &ch);
   m_measureDialog->GetSize(&mw, &mh);
   this->GetSize(&w, &h);
   this->GetPosition(&x, &y);
   
   x = x + w + 1;
   if (x + cw > dw)
      x = dw - cw - 1;
   m_configDialog->Move(x, y);
   
   x = x + cw + 1;
   if (x + mw > dw)
      x = dw - mw - 1;
   m_measureDialog->Move(x, y);
   
   // initialize status bar
   CreateStatusBar();
   UpdateStatusBar();

   // start event processing thread
#ifdef MULTI_THREAD_READOUT
   g_epcs = new wxCriticalSection();
   m_epthread = new EPThread(this);
#endif

   // Configure individual boards
   
   for (int i=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
      m_board = i;
      m_osci->SelectBoard(i);
      
      int ofs = m_HOffset[i];
      SetRefclk(m_board, m_refClk[i]); // update sampling speed according to Refclk
      m_HOffset[i] = ofs;
      
      m_osci->SetClkOn(m_clkOn);
      m_osci->SetSpikeRemoval(m_spikeRemoval);
      
      m_osci->SetTriggerDelay(m_trgDelay[i]);
      m_trgDelayNs[i] = m_osci->GetTriggerDelayNs();
      m_osci->SetInputRange(m_range[i]);
      m_osci->SetTriggerMode(m_trgMode[i]);
      if (m_trgConfig[i])
         m_osci->SetTriggerConfig(m_trgConfig[i]);
      else
         m_osci->SetTriggerSource(m_trgSource[i]);
      m_osci->SetTriggerLevel(m_trgLevel[0][i]);
      m_osci->SetTriggerPolarity(m_trgNegative[i]);
      m_osci->SetTriggerDelay(m_trgDelay[i]);
      m_trgDelayNs[i] = m_osci->GetTriggerDelayNs();
      for (int j=0 ; j<4 ; j++)
         m_osci->SetChnOn(i, j, m_chnOn[i][j]);
   }
   
   // select first board
   m_board = 0;
   m_osci->SelectBoard(m_board);

   // turn on multi board mode
   if (m_multiBoard) {
      m_osci->SetMultiBoard(true);
      for (int i=1 ; i<m_osci->GetNumberOfBoards() ; i++) {
         SetTriggerSource(i, 4);       // select external trigger
         SetTriggerPolarity(i, false); // positive trigger
      }
      m_osci->SelectBoard(m_board);
   }
   
   // set screen controls from values
   UpdateControls();
   
   // update screen settings from values
   for (int i=0 ; i<m_osci->GetNumberOfBoards() ; i++) {
      for (int j=0 ; j<4 ; j++) {
         m_screen->SetChnOn(i, j, m_chnOn[i][j]);
         m_screen->SetPos(i, j, m_chnOffset[i][j]/1000.0);
         m_screen->SetScale(i, j, m_chnScale[i][j]);
      }
      m_screen->SetHScale(i, m_HScale[i]);
      m_screen->SetScreenOffset(i, m_HOffset[i]);
   }
   m_screen->SetSplitMode(m_splitMode);
   m_screen->SetDisplayScalers(m_displayScalers);

   // Start acquisition
   m_single = false;
   m_running = true;
   m_rearm = false;
   m_osci->Enable(true);
   m_osci->SetRunning(m_running);
   m_timer->Start(100, true);
   m_acquisitions = 0;
   m_stopWatch.Start();
   m_stopWatch1.Start();
}

/*------------------------------------------------------------------*/

DOFrame::~DOFrame()
{
   m_running = false;
   if (m_timer->IsRunning())
      m_timer->Stop();
   delete m_timer;

   SaveConfig();

   if (m_epthread) {
      m_osci->StopThread();
      m_epthread->StopThread();
   }

   /* close file if open */
  if (m_WFFile || m_WFfd)
      CloseWFFile(false);

   if (g_epcs)
      delete g_epcs;

   for (int i=0 ; i<Measurement::N_MEASUREMENTS ; i++)
      for (int chn=0 ; chn<4 ; chn++)
         delete m_measurement[i][chn];

   delete m_osci;
   delete m_screen;
}

/*------------------------------------------------------------------*/

void DOFrame::UpdateWaveforms()
{
   if (m_epthread) {
      int n = m_osci->IsMultiBoard() ? m_osci->GetNumberOfBoards() : 1;
      
      g_epcs->Enter();
      for (int i=0 ; i<n ; i++) {
         for (int j=0 ; j<4 ; j++) {
            memcpy(m_time[i][j], m_epthread->GetTime(i, j), m_osci->GetWaveformDepth(i)*sizeof(float));
            memcpy(m_waveform[i][j], m_epthread->GetWaveform(i, j), m_osci->GetWaveformDepth(j)*sizeof(float));
         }
      }
      g_epcs->Leave();
   }
}

/*------------------------------------------------------------------*/

void DOFrame::ClearWaveforms()
{
   // used when switching between boards
   if (m_epthread) {
      if (!m_osci->IsMultiBoard())
         m_epthread->ClearWaveforms();
   }
}

/*------------------------------------------------------------------*/

void DOFrame::UpdateControls()
{
   if (m_osci->IsMultiBoard() && m_board > 0) {
      m_slTrgLevel->Enable(false);
      m_rbAuto->Enable(false);
      m_rbNormal->Enable(false);
      m_bpPolarity->Enable(false);
      m_rbSource->Enable(false);
   } else {
      m_slTrgLevel->Enable(true);
      m_rbAuto->Enable(true);
      m_rbNormal->Enable(true);
      m_bpPolarity->Enable(true);
      m_rbSource->Enable(true);
      m_btTrgCfg->Enable();
   }

   m_slTrgLevel->SetValue((int)(-m_trgLevel[m_board][0]*1000));
   if (m_trgMode[m_board] == TM_AUTO)
      m_rbAuto->SetValue(1);
   else
      m_rbNormal->SetValue(1);
   if (m_trgNegative[m_board])
      m_bpPolarity->SetBitmapLabel(wxBitmap(neg_xpm));
   else
      m_bpPolarity->SetBitmapLabel(wxBitmap(pos_xpm));
   m_rbSource->SetSelection(m_trgSource[m_board]);
   m_slTrgDelay->SetValue(100-m_trgDelay[m_board]);
   if (m_trgConfig[m_board])
      m_rbSource->Enable(false);
   
   m_btCh1->SetValue(m_chnOn[m_board][0]);
   m_btCh2->SetValue(m_chnOn[m_board][1]);
   m_btCh3->SetValue(m_chnOn[m_board][2]);
   m_btCh4->SetValue(m_chnOn[m_board][3]);
   
   m_slPos1->SetValue(m_chnOffset[m_board][0]/2);
   m_slPos2->SetValue(m_chnOffset[m_board][1]/2);
   m_slPos3->SetValue(m_chnOffset[m_board][2]/2);
   m_slPos4->SetValue(m_chnOffset[m_board][3]/2);
   
   for (int i=0 ; i<4 ; i++) {
      m_screen->SetChnOn(m_board, i, m_chnOn[m_board][i]);
      m_osci->SetChnOn(m_board, i, m_chnOn[m_board][i]);
      m_screen->SetPos(m_board, i, m_chnOffset[m_board][i]/1000.0);
      m_screen->SetScale(m_board, i, m_chnScale[m_board][i]);
      
      wxString wxst;
      if (DOScreen::m_scaleTable[m_chnScale[m_board][i]] >= 1000)
         wxst.Printf(wxT("%d V"), DOScreen::m_scaleTable[m_chnScale[m_board][i]]/1000);
      else
         wxst.Printf(wxT("%dmV"), DOScreen::m_scaleTable[m_chnScale[m_board][i]]);
      
      if (i == 0) m_stScale1->SetLabel(wxst);
      if (i == 1) m_stScale2->SetLabel(wxst);
      if (i == 2) m_stScale3->SetLabel(wxst);
      if (i == 3) m_stScale4->SetLabel(wxst);
   }

   // set initial horizontal scale and offset
   m_screen->SetHScale(m_board, m_HScale[m_board]);
   
   // calculate trigger position from screen offset
   double trgFrac = (GetTrgPosition(m_board) - m_HOffset[m_board]) / m_screen->GetScreenSize(m_board);
   RecalculateHOffset(trgFrac);
   
   wxString wxstr;
   if (DOScreen::m_hscaleTable[m_screen->GetHScale(m_board)] >= 1000) {
      wxstr.Printf(wxT("%d us/div"), DOScreen::m_hscaleTable[m_HScale[m_board]]/1000);
   } else {
      wxstr.Printf(wxT("%d ns/div"), DOScreen::m_hscaleTable[m_HScale[m_board]]);
   }
   m_stHScale->SetLabel(wxstr);

   // enable trigger config button for evaluation boards V4.0 & above
   if (m_osci->GetNumberOfBoards() > 0) {
      if (m_osci->GetCurrentBoard()->GetBoardType() < 8) {
         EnableTriggerConfig(false);
      } else {
         if (m_osci->IsMultiBoard() && m_board > 0)
            EnableTriggerConfig(false);
         else
            EnableTriggerConfig(true);
      }
   }
}

/*------------------------------------------------------------------*/

float *DOFrame::GetTime(int b, int c)
{
   if (m_epthread)
      return m_time[b][c];
   else
      return m_osci->GetTime(b, c);
}

/*------------------------------------------------------------------*/

float *DOFrame::GetWaveform(int b, int c)
{
   if (m_epthread)
      return m_waveform[b][c];
   else
      return m_osci->GetWaveform(b, c);
}

/*------------------------------------------------------------------*/

double DOFrame::GetActSamplingSpeed()
{
   return m_osci->GetTrueSamplingSpeed();
}

/*------------------------------------------------------------------*/

void DOFrame::UpdateStatusBar()
{
   if (m_osci->GetNumberOfBoards() == 0) {
      SetStatusText(wxT("No DRS boards found!"));
   } else {
      DRSBoard *b = m_osci->GetCurrentBoard();
      wxString wxstr;

#ifdef HAVE_VME
      if (b->GetTransport() == 1)
         wxstr.Printf(wxT("Connected to VME board slot %2d %s, serial #%d, firmware revision %d, T=%1.1lf C"), 
            (b->GetSlotNumber() >> 1)+2, ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower", 
            b->GetBoardSerialNumber(), b->GetFirmwareVersion(), b->GetTemperature());
      else
#endif
         wxstr.Printf(wxT("Connected to USB board #%d, serial #%d, firmware revision %d, T=%1.1lf C"), 
            b->GetSlotNumber(), b->GetBoardSerialNumber(), b->GetFirmwareVersion(), b->GetTemperature());

      SetStatusText(wxstr);
   }
}

/*------------------------------------------------------------------*/

void DOFrame::SaveConfig()
{
   char str[256], name[256], filename[1024];

   strcpy(filename, "drsosc.cfg");

#ifdef _MSC_VER
   if (getenv("APPDATA")) {
      strlcpy(filename, getenv("APPDATA"), sizeof(filename));
      strlcat(filename, "\\DRSOsc", sizeof(filename));
      mkdir(filename);
      strlcat(filename, "\\drsosc.cfg", sizeof(filename));
   } else
      strcpy(filename, "drsosc.cfg");
#endif

#ifdef OS_DARWIN
   strcpy(filename, "DRSOsc.app/drsosc.cfg");
#endif

   MXML_WRITER *xml = mxml_open_file(filename);
   if (xml == NULL) {
      wxMessageBox(wxT("Error writing to configuration file"), wxT("DRSOsc"), wxOK | wxICON_ERROR);
      return;
   }

   mxml_start_element(xml, "DRSOsc");

   sprintf(str, "%d", (int)m_multiBoard);
   mxml_write_element(xml, "MultiBoard", str);
   
   sprintf(str, "%d", (int)m_splitMode);
   mxml_write_element(xml, "SplitMode", str);

   for (int b=0 ; b<m_osci->GetNumberOfBoards() ; b++) {
      sprintf(str, "Board-%d", m_osci->GetBoard(b)->GetBoardSerialNumber());
      mxml_start_element(xml, str);
      
      sprintf(str, "%1.3lf", m_trgLevel[b][0]);
      mxml_write_element(xml, "TrgLevel1", str);
      sprintf(str, "%1.3lf", m_trgLevel[b][1]);
      mxml_write_element(xml, "TrgLevel2", str);
      sprintf(str, "%1.3lf", m_trgLevel[b][2]);
      mxml_write_element(xml, "TrgLevel3", str);
      sprintf(str, "%1.3lf", m_trgLevel[b][3]);
      mxml_write_element(xml, "TrgLevel4", str);
      sprintf(str, "%d", m_trgMode[b]);
      mxml_write_element(xml, "TrgMode", str);
      sprintf(str, "%d", m_trgNegative[b]);
      mxml_write_element(xml, "TrgNegative", str);
      sprintf(str, "%d", m_trgSource[b]);
      mxml_write_element(xml, "TrgSource", str);
      sprintf(str, "%d", m_trgDelay[b]);
      mxml_write_element(xml, "TrgDelay", str);
      sprintf(str, "%d", m_trgConfig[b]);
      mxml_write_element(xml, "TrgConfig", str);
      
      sprintf(str, "%d", m_HScale[b]);
      mxml_write_element(xml, "HScale", str);
      sprintf(str, "%1.3lg", m_reqSamplingSpeed);
      mxml_write_element(xml, "SamplingSpeed", str);
      sprintf(str, "%d", m_freqLocked);
      mxml_write_element(xml, "FreqLocked", str);
      sprintf(str, "%d", m_HOffset[b]);
      mxml_write_element(xml, "HOffset", str);
      
      for (int i=0 ; i<4 ; i++) {
         sprintf(name, "ChnOn%d", i);
         sprintf(str, "%d", m_chnOn[b][i]);
         mxml_write_element(xml, name, str);
         sprintf(name, "ChnOffset%d", i);
         sprintf(str, "%d", m_chnOffset[b][i]);
         mxml_write_element(xml, name, str);
         sprintf(name, "ChnScale%d", i);
         sprintf(str, "%d", m_chnScale[b][i]);
         mxml_write_element(xml, name, str);
      }
      
      sprintf(str, "%d", m_chnSection);
      mxml_write_element(xml, "ChnSection", str);
      
      sprintf(str, "%d", m_clkOn);
      mxml_write_element(xml, "ClkOn", str);
      
      sprintf(str, "%1.2lf", m_range[b]);
      mxml_write_element(xml, "Range", str);
      
      sprintf(str, "%d", m_spikeRemoval);
      mxml_write_element(xml, "SpikeRemoval", str);
      
      sprintf(str, "%d", (int)m_refClk[b]);
      mxml_write_element(xml, "ExtRefclk", str);
      
      sprintf(str, "%d", (int)m_displayScalers);
      mxml_write_element(xml, "DisplayScalers", str);

      mxml_end_element(xml);
   }

   mxml_end_element(xml);
   mxml_close_file(xml);
}

/*------------------------------------------------------------------*/

void DOFrame::LoadConfig(char *error, int size)
{
   int i, j, idx, fh;
   char filename[1024];

   strcpy(filename, "drsosc.cfg");

#ifdef _MSC_VER
   if (getenv("APPDATA")) {
      strlcpy(filename, getenv("APPDATA"), sizeof(filename));
      strlcat(filename, "\\DRSOsc", sizeof(filename));
      mkdir(filename);
      strlcat(filename, "\\drsosc.cfg", sizeof(filename));
   } else
      strcpy(filename, "drsosc.cfg");
#endif

#ifdef OS_DARWIN
   strcpy(filename, "DRSOsc.app/drsosc.cfg");
#endif

   /* try to create configuration file to see if we have write access */
   fh = open(filename, O_RDWR | O_CREAT | O_APPEND, 0644);
   if (fh < 0) {
      wxString str = wxT("Error creating configuration file ");
      str += (const wxString &)filename;
      wxMessageBox((const wxChar*)wxString::FromAscii((const char*)str), wxT("DRSOsc"), wxOK | wxICON_ERROR);
      return;
   }
   close(fh);

   PMXML_NODE root = mxml_parse_file(filename, error, size, NULL);
   if (root == NULL) {
      wxMessageBox(wxT("Error reading from configuration file"), wxT("DRSOsc"), wxOK | wxICON_ERROR);
      return;
   }

   PMXML_NODE osc = mxml_find_node(root, "DRSOsc");
   if (osc == NULL) {
      mxml_free_tree(root);
      return;
   }
   
   PMXML_NODE node = mxml_find_node(osc, "MultiBoard");
   if (node) m_multiBoard = atoi(mxml_get_value(node)) > 0;
   
   node = mxml_find_node(osc, "SplitMode");
   if (node) m_splitMode = atoi(mxml_get_value(node)) > 0;

   for (i=idx=0 ; ; i++) {
      PMXML_NODE b = mxml_subnode(osc, i);
      if (!b)
         break;
      
      node = mxml_find_node(b, "TrgLevel1");
      if (!node)
         continue; // node is not a board
      
      if (node) m_trgLevel[idx][0] = atof(mxml_get_value(node));
      node = mxml_find_node(b, "TrgLevel2");
      if (node) m_trgLevel[idx][1] = atof(mxml_get_value(node));
      node = mxml_find_node(b, "TrgLevel3");
      if (node) m_trgLevel[idx][2] = atof(mxml_get_value(node));
      node = mxml_find_node(b, "TrgLevel4");
      if (node) m_trgLevel[idx][3] = atof(mxml_get_value(node));
      node = mxml_find_node(b, "TrgMode");
      if (node) m_trgMode[idx] = atoi(mxml_get_value(node));
      node = mxml_find_node(b, "TrgNegative");
      if (node) m_trgNegative[idx] = atoi(mxml_get_value(node)) == 1;
      node = mxml_find_node(b, "TrgSource");
      if (node) m_trgSource[idx] = atoi(mxml_get_value(node));
      node = mxml_find_node(b, "TrgDelay");
      if (node) m_trgDelay[idx] = atoi(mxml_get_value(node));
      node = mxml_find_node(b, "TrgConfig");
      if (node) m_trgConfig[idx] = atoi(mxml_get_value(node));
      node = mxml_find_node(b, "HScale");
      if (node) m_HScale[idx] = atoi(mxml_get_value(node));
      node = mxml_find_node(b, "SamplingSpeed");
      if (node) m_reqSamplingSpeed = atof(mxml_get_value(node));
      node = mxml_find_node(b, "FreqLocked");
      if (node) m_freqLocked = atoi(mxml_get_value(node)) == 1;
      node = mxml_find_node(b, "HOffset");
      if (node) m_HOffset[idx] = atoi(mxml_get_value(node));
      
      for (j=0 ; j<4 ; j++) {
         char str[256];
         sprintf(str, "ChnOn%d", j);
         node = mxml_find_node(b, str);
         if (node) m_chnOn[idx][j] = atoi(mxml_get_value(node)) == 1;
         sprintf(str, "ChnOffset%d", j);
         node = mxml_find_node(b, str);
         if (node) m_chnOffset[idx][j] = atoi(mxml_get_value(node));
         sprintf(str, "ChnScale%d", j);
         node = mxml_find_node(b, str);
         if (node) m_chnScale[idx][j] = atoi(mxml_get_value(node));
      }
      
      node = mxml_find_node(b, "ChnSection");
      if (node) m_chnSection = atoi(mxml_get_value(node));
      
      node = mxml_find_node(b, "ClkOn");
      if (node) m_clkOn = atoi(mxml_get_value(node)) == 1;
      
      node = mxml_find_node(b, "Range");
      if (node) m_range[idx] = atof(mxml_get_value(node));
      
      node = mxml_find_node(b, "SpikeRemoval");
      if (node) m_spikeRemoval = atoi(mxml_get_value(node)) == 1;
      
      node = mxml_find_node(b, "ExtRefclk");
      if (node) m_refClk[idx] = atoi(mxml_get_value(node)) == 1;
      
      node = mxml_find_node(b, "DisplayScalers");
      if (node) m_displayScalers = atoi(mxml_get_value(node)) == 1;

      idx++;
   }
   
   mxml_free_tree(root);
}

/*------------------------------------------------------------------*/

void DOFrame::OnSave(wxCommandEvent& WXUNUSED(event))
{
   wxString filename;

   if (!m_WFFile && !m_WFfd) {
      filename = wxFileSelector(wxT("Choose a file to write to"), wxEmptyString, 
         wxEmptyString, wxT(".xml"), wxT("XML file (*.xml)|*.xml|Binary file (*.dat)|*.dat"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

      if (!filename.empty()) {
         
         m_nSaveMax = wxGetNumberFromUser(wxT("Number of events to be saved:"), wxT(""), wxT("DRSOsc"), 1000, 1, 1000000000);
         if (m_nSaveMax > 0) {
            SetSaveBtn(wxT("Close"), wxT("Stop saving waveforms"));
            if (filename.Find(wxT(".xml")) != wxNOT_FOUND) {
               m_WFfd = 0;
               m_WFFile = mxml_open_file(filename.char_str());
               if (m_WFFile)
                  mxml_start_element(m_WFFile, "DRSOSC");
            } else {
               m_WFFile = NULL;
               m_WFfd = open(filename.char_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
               assert(m_WFfd > 0);
            }
            m_osci->SetEventSerial(1);
            m_nSaved = 0;
         }
      }
   } else {
      CloseWFFile(false);
   }
}

/*------------------------------------------------------------------*/

void DOFrame::SaveHisto()
{
   wxString filename;
   
   m_osci->Enable(false);

   filename = wxFileSelector(wxT("Choose a file to write to"), wxEmptyString,
                             wxEmptyString, wxT(".txt"), wxT("Text file (*.txt)|*.txt"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
   
   if (!filename.empty()) {
      
      int fd = open(filename.char_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
      assert(fd > 0);
      
      m_screen->SaveHisto(fd);
      
      close(fd);
   }
   
   m_osci->Enable(true);
}

/*------------------------------------------------------------------*/

wxPrintDialogData g_printDialogData;

void DOFrame::OnPrint(wxCommandEvent& WXUNUSED(event))
{
   wxPrinter p(&g_printDialogData);
   MyPrintout printout(m_screen, wxT("My printout"));
   if (!p.Print(this, &printout, true)) {
      if (wxPrinter::GetLastError() == wxPRINTER_ERROR)
         wxMessageBox(wxT("There was a problem printing.\nPerhaps your current printer is not set correctly?"), wxT("Printing"), wxOK);
      else
         wxMessageBox(wxT("You cancelled printing"),
                      wxT("Printing"), wxOK);
   } else
      g_printDialogData = p.GetPrintDialogData();
}

/*------------------------------------------------------------------*/

bool MyPrintout::OnPrintPage(int page)
{
   wxCoord w, h;

   wxDC *dc = wxPrintout::GetDC();
   if (dc == NULL)
      return false;

   dc->GetSize(&w, &h);
   FitThisSizeToPage(wxSize(800, 600));
   m_screen->DrawScope(*dc, 800, 600, true);

   return true;
}

/*------------------------------------------------------------------*/

void DOFrame::OnConfig( wxCommandEvent& event )
{
   m_configDialog->Show(true);
}

/*------------------------------------------------------------------*/

void DOFrame::OnMeasure( wxCommandEvent& event )
{
   m_measureDialog->Show(true);
}

/*------------------------------------------------------------------*/

void DOFrame::OnDisplay( wxCommandEvent& event )
{
   m_displayDialog->Show(true);
}

/*------------------------------------------------------------------*/

void DOFrame::OnExit( wxCommandEvent& event )
{
   Close(true);
}

/*------------------------------------------------------------------*/

void DOFrame::OnAbout( wxCommandEvent& event )
{
   AboutDialog ad(this);
   ad.ShowModal();
}

/*------------------------------------------------------------------*/

void DOFrame::OnTrgLevelChange(wxScrollEvent& event)
{
   double f = (-m_slTrgLevel->GetValue()) / 1000.0; // -0.5 ... 0.5
   
   /* convert to voltage according to screen settings */
   int chn = m_trgSource[m_board];
   m_trgLevel[m_board][0] = m_screen->GetScale(m_board, chn)*10*f/1000 + m_screen->GetOffset(m_board, chn);
   m_trgLevel[m_board][1] = m_trgLevel[m_board][0];
   m_trgLevel[m_board][2] = m_trgLevel[m_board][0];
   m_trgLevel[m_board][3] = m_trgLevel[m_board][0];
   m_lastTriggerUpdate = time(NULL);
   m_osci->SetTriggerLevel(m_trgLevel[m_board][0]);
   m_triggerDialog->SetTriggerLevel(m_trgLevel[m_board][chn]);
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::SetTrgLevel(int i, double value)
{
   if (i<4) {
      m_trgLevel[m_board][i] = value;
      m_osci->SetIndividualTriggerLevel(i, value);
   }
   m_lastTriggerUpdate = time(NULL);
}

/*------------------------------------------------------------------*/

void DOFrame::OnTrgDelayChange(wxScrollEvent& event)
{
   m_trgDelay[m_board] = 100-m_slTrgDelay->GetValue();
   m_osci->SetTriggerDelay(m_trgDelay[m_board]);
   m_trgDelayNs[m_board] = m_osci->GetTriggerDelayNs();
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

double DOFrame::GetTrgPosition(int board)
{
   // return current trigger position in ns relative to left edge of waveform
   return m_osci->GetWaveformLength() - m_trgDelayNs[board];
}

/*------------------------------------------------------------------*/

void DOFrame::OnTrigger(wxCommandEvent& event)
{
   if (event.GetId() == ID_RUN) {
      if (m_running) {
         m_running = false;
         m_single = false;
         m_osci->SetRunning(false);
         m_osci->SetSingle(false);
         m_btRun->SetLabel(wxT("Run"));
         m_acqPerSecond = 0;
         m_acquisitions = 0;
         m_oldIdle = false;
      } else {
         m_running = true;
         m_single = false;
         m_osci->SetSingle(false);
         m_osci->SetRunning(true);
         m_btRun->SetLabel(wxT("Stop"));
         m_acquisitions = 0;
         m_oldIdle = false;
         m_stopWatch.Start();
         m_stopWatch1.Start();
      }
   } else if (event.GetId() == ID_SINGLE) {
      if (m_running)
         m_osci->SetRunning(false);
      m_running = false;
      m_osci->SetSingle(true);
      m_single = true;
      m_btRun->SetLabel(wxT("Run"));

      if (m_osci->IsArmed()) {
         /* no trigger happened, so issue a software trigger */
         m_osci->SingleTrigger();
         m_rearm = true;
      } else
         m_osci->Start();

      m_acqPerSecond = 0;
      m_acquisitions = 0;
   } else if (event.GetId() == ID_TRGCFG) {
      m_triggerDialog->Show();
   }

   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnTrgButton(wxCommandEvent& event)
{
   if (event.GetId() == ID_TR_NORMAL) {
      m_trgMode[m_board] = TM_NORMAL;
   } else if (event.GetId() == ID_TR_AUTO) {
      m_trgMode[m_board] = TM_AUTO;
   } else if (event.GetId() == ID_TR_POLARITY) {
      m_trgNegative[m_board] = !m_trgNegative[m_board];
      if (m_trgNegative[m_board])
         m_bpPolarity->SetBitmapLabel(wxBitmap(neg_xpm));
      else
         m_bpPolarity->SetBitmapLabel(wxBitmap(pos_xpm));
   } else if (event.GetId() == ID_TR_SOURCE) {
      m_trgSource[m_board] = m_rbSource->GetSelection();
   }

   m_osci->SetTriggerMode(m_trgMode[m_board]);
   if (m_trgConfig[m_board])
      m_osci->SetTriggerConfig(m_trgConfig[m_board]);
   else
      m_osci->SetTriggerSource(m_trgSource[m_board]);
   m_osci->SetTriggerPolarity(m_trgNegative[m_board]);
   m_osci->SetTriggerDelay(m_trgDelay[m_board]);
   m_lastTriggerUpdate = time(NULL);
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::SetTriggerSource(int b, int source)
{
   m_trgSource[b] = source;
   m_osci->SelectBoard(b);
   m_osci->SetTriggerSource(source);
}

/*------------------------------------------------------------------*/

void DOFrame::SetTriggerPolarity(int b, bool negative)
{
   m_trgNegative[b] = negative;
   m_osci->SelectBoard(b);
   m_osci->SetTriggerPolarity(negative);
}

/*------------------------------------------------------------------*/

void DOFrame::OnChnOn(wxCommandEvent& event)
{
   int i, id;

   id = event.GetId();

   if (id == ID_CHON1) i = 0;
   if (id == ID_CHON2) i = 1;
   if (id == ID_CHON3) i = 2;
   if (id == ID_CHON4) i = 3;

   m_chnOn[m_board][i] = event.IsChecked();
   m_screen->SetChnOn(m_board, i, event.IsChecked());
   m_osci->SetChnOn(0, i, event.IsChecked());
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnPosChange(wxScrollEvent& event)
{
   // 2mV per tick -0.5 ... +0.5 V
   m_chnOffset[m_board][0] = m_slPos1->GetValue()*2;
   m_chnOffset[m_board][1] = m_slPos2->GetValue()*2;
   m_chnOffset[m_board][2] = m_slPos3->GetValue()*2;
   m_chnOffset[m_board][3] = m_slPos4->GetValue()*2;
   for (int i=0 ; i<4 ; i++)
      m_screen->SetPos(m_board, i, m_chnOffset[m_board][i]/1000.0);

   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnScaleChange(wxCommandEvent& event)
{
   int id, i, inc;

   id = event.GetId();

   if (id == ID_SCALEUP1) { i=0; inc = -1; }
   if (id == ID_SCALEUP2) { i=1; inc = -1; }
   if (id == ID_SCALEUP3) { i=2; inc = -1; }
   if (id == ID_SCALEUP4) { i=3; inc = -1; }

   if (id == ID_SCALEDN1) { i=0; inc = +1; }
   if (id == ID_SCALEDN2) { i=1; inc = +1; }
   if (id == ID_SCALEDN3) { i=2; inc = +1; }
   if (id == ID_SCALEDN4) { i=3; inc = +1; }

   m_chnScale[m_board][i] += inc;
   if (m_chnScale[m_board][i] > 9)
      m_chnScale[m_board][i] = 9;
   if (m_chnScale[m_board][i] < 0)
      m_chnScale[m_board][i] = 0;

   m_screen->SetScale(m_board, i, m_chnScale[m_board][i]);

   wxString wxst;
   if (DOScreen::m_scaleTable[m_chnScale[m_board][i]] >= 1000)
     wxst.Printf(wxT("%d V"), DOScreen::m_scaleTable[m_chnScale[m_board][i]]/1000);
   else
     wxst.Printf(wxT("%dmV"), DOScreen::m_scaleTable[m_chnScale[m_board][i]]);

   if (i == 0) m_stScale1->SetLabel(wxst);
   if (i == 1) m_stScale2->SetLabel(wxst);
   if (i == 2) m_stScale3->SetLabel(wxst);
   if (i == 3) m_stScale4->SetLabel(wxst);

   m_screen->Refresh();
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::ChangeHScale(int delta)
{
   // remember current trigger position inside screen
   double trgFrac = (GetTrgPosition(m_board)-m_screen->GetScreenOffset(m_board)) / m_screen->GetScreenSize(m_board);

   // change screen scale
   if (delta)
      m_screen->SetHScaleInc(m_board, delta < 0 ? -1 : +1);
   m_HScale[m_board] = m_screen->GetHScale(m_board);

   // adjust sampling speed if necessary
   if (!m_freqLocked) {
      m_reqSamplingSpeed = 100.0/DOScreen::m_hscaleTable[m_HScale[m_board]];
      if (m_reqSamplingSpeed > m_osci->GetMaxSamplingSpeed())
         m_reqSamplingSpeed = m_osci->GetMaxSamplingSpeed();
      if (m_reqSamplingSpeed < m_osci->GetMinSamplingSpeed())
         m_reqSamplingSpeed = m_osci->GetMinSamplingSpeed();
      m_osci->SetSamplingSpeed(m_reqSamplingSpeed);
   }

   // update config dialog
   if (m_configDialog != NULL)
      m_configDialog->FreqChange();

   RecalculateHOffset(trgFrac);
   StatReset();
}

void DOFrame::RecalculateHOffset(double trgFrac)
{
   wxString wxstr;

   // force recalculation of trigger delay
   m_osci->SetTriggerDelay(m_trgDelay[m_board]);
   m_trgDelayNs[m_board] = m_osci->GetTriggerDelayNs();

   // calculate new screen offset to keep trigger point at same position
   m_HOffset[m_board] = (int)(GetTrgPosition(m_board) - trgFrac * (double)m_screen->GetScreenSize(m_board) + 0.5);

   // adjust horizontal slider accordingly
   double v = (m_HOffset[m_board] + m_screen->GetScreenSize(m_board)/2 - m_osci->GetWaveformLength()/2) / m_osci->GetWaveformLength();
   if (v < -0.5) // limit range
      v = -0.5;
   if (v > 0.5)
      v = 0.5;
   m_slHOffset->SetValue((int)(-v*2000 + 0.5));

   // convert back slider setting to m_HOffset with valid range
   m_HOffset[m_board] = (int)(m_osci->GetWaveformLength()*v - m_screen->GetScreenSize(m_board)/2 + m_osci->GetWaveformLength()/2 + 0.5);

   m_screen->SetScreenOffset(m_board, m_HOffset[m_board]);

   // update label
   if (DOScreen::m_hscaleTable[m_HScale[m_board]] >= 1000) {
      wxstr.Printf(wxT("%d us/div"), DOScreen::m_hscaleTable[m_HScale[m_board]]/1000);
   } else {
      wxstr.Printf(wxT("%d ns/div"), DOScreen::m_hscaleTable[m_HScale[m_board]]);
   }
   m_stHScale->SetLabel(wxstr);

   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnHScaleChange(wxCommandEvent& event)
{
   if (event.GetId() == ID_HSCALEUP)
      ChangeHScale(-1);
   else
      ChangeHScale(+1);
   StatReset();
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnHOffsetChange(wxScrollEvent& event)
{
   double v = -m_slHOffset->GetValue()/2000.0; // -0.5 ... +0.5

   // scale from (size/2-length) to (size/2)
   m_HOffset[m_board] = (int)(m_osci->GetWaveformLength()*v - m_screen->GetScreenSize(m_board)/2 + m_osci->GetWaveformLength()/2 + 0.5);
   m_screen->SetScreenOffset(m_board, m_HOffset[m_board]);
   m_screen->Refresh();
}

/*------------------------------------------------------------------*/

void DOFrame::OnZero(wxMouseEvent& event)
{
   if (event.GetId() == ID_HOR_POS) {
      m_slHOffset->SetValue(0);
      OnHOffsetChange((wxScrollEvent&)event);
   }
   if (event.GetId() == ID_TR_LEVEL) {
      m_slTrgLevel->SetValue(0);
      OnTrgLevelChange((wxScrollEvent&)event);
   }
   if (event.GetId() == ID_POS1) {
      m_slPos1->SetValue(0);
      OnPosChange((wxScrollEvent&)event);
   }
   if (event.GetId() == ID_POS2) {
      m_slPos2->SetValue(0);
      OnPosChange((wxScrollEvent&)event);
   }
   if (event.GetId() == ID_POS3) {
      m_slPos3->SetValue(0);
      OnPosChange((wxScrollEvent&)event);
   }
   if (event.GetId() == ID_POS4) {
      m_slPos4->SetValue(0);
      OnPosChange((wxScrollEvent&)event);
   }
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayDateTime(bool flag)
{
   m_screen->SetDisplayDateTime(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayShowGrid(bool flag)
{
   m_screen->SetDisplayShowGrid(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayLines(bool flag)
{
   m_screen->SetDisplayLines(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayMode(int mode, int n)
{
   m_screen->SetDisplayMode(mode, n);
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayScalers(bool flag)
{
   m_displayScalers = flag;
   m_screen->SetDisplayScalers(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayCalibrated(bool flag)
{
   m_osci->SetCalibrated(flag);
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayCalibrated2(bool flag)
{
   m_osci->SetCalibrated2(flag);
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayTCalOn(bool flag)
{
   m_osci->SetTCalOn(flag);
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayTrgCorr(bool flag)
{
   m_trgCorr = flag;
}

/*------------------------------------------------------------------*/

void DOFrame::SetRefclk(int b, bool flag)
{
   m_refClk[b] = flag;
   m_osci->SelectBoard(b);
   m_osci->SetRefclk(b, flag);
   SetSamplingSpeed(m_reqSamplingSpeed);
}

/*------------------------------------------------------------------*/

void DOFrame::SelectBoard(int board)
{
   m_board = board;
   m_configDialog->SelectBoard(board);
   m_triggerDialog->SelectBoard(board);
   ClearWaveforms();
   UpdateControls();
   UpdateStatusBar();
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::SetMultiBoard(bool flag)
{
   m_multiBoard = flag;
}

/*------------------------------------------------------------------*/

void DOFrame::SetSource(int board, int firstChannel, int chnSection)
{
   m_board = board;
   m_firstChannel = firstChannel;
   m_chnSection = chnSection;
}

/*------------------------------------------------------------------*/

void DOFrame::SetDisplayRotated(bool flag)
{
   m_osci->SetRotated(flag);
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::ToggleControls()
{
   if (m_hideControls) {
      m_pnControls->Show();
      m_hideControls = false;
   } else {
      m_pnControls->Hide();
      m_hideControls = true;
   }
   this->Layout();
}

/*------------------------------------------------------------------*/

void DOFrame::OnTimer(wxTimerEvent& event)
{
   char str[256];

   if (m_first) {
      if (m_osci->GetError(str, sizeof(str)))
         wxMessageBox((const wxChar*)wxString::FromAscii(str),
                         wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, this);

      if (m_osci->GetNumberOfBoards() == 0)
            wxMessageBox(wxT("No DRS board found\r\nRunning in demo mode"),
                            wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, this);

      if (m_osci->GetNumberOfBoards() && 
          fabs(GetRange() - GetOsci()->GetCalibratedInputRange()) > 0.001) {
         wxString str;

         str.Printf(wxT("This board was calibrated at %1.2lg V ... %1.2lg V\nYou must recalibrate the board if you use a different input range"), 
            GetOsci()->GetCalibratedInputRange()-0.5, GetOsci()->GetCalibratedInputRange()+0.5);
         wxMessageBox((const wxChar*)wxString::FromAscii((const char*)str), wxT("DRS Oscilloscope Warning"), wxOK | wxICON_EXCLAMATION, this);
      }
      
      // issue warning if timing calibration not valid
      m_osci->CheckTimingCalibration();
   }

   m_first = false;

   /* calculate number of acquistions once per second */
   if (m_osci->GetNumberOfBoards() > 0 && m_stopWatch.Time() > 1000) {
      if (g_epcs)
         g_epcs->Enter();
      m_acqPerSecond = (int) (1000.0 * m_acquisitions / m_stopWatch.Time() + 0.5);
      m_acquisitions = 0;
      if (g_epcs)
         g_epcs->Leave();

      m_stopWatch.Start();

      // update temperature and number of acquisitions
      UpdateStatusBar();
   }

   if (m_epthread == NULL) {
      // handle event processing in the main thread
      if (m_running) {
         if (m_osci->HasNewEvent()) {
            ProcessEvents();
            m_oldIdle = false;
            m_stopWatch1.Start();
         } else {
            if (m_osci->GetTriggerMode() == TM_AUTO && m_stopWatch1.Time() > 1000) {
               m_osci->SingleTrigger();
               while (!m_osci->HasNewEvent());
               m_osci->ReadWaveforms();
               ProcessEvents();
               m_acquisitions++;
            }
         }
      }

      if (m_single) {
         if (m_osci->HasNewEvent()) {
            ProcessEvents();
            m_oldIdle = false;
         }
      }

      if (m_osci->IsIdle() && !m_oldIdle) {
         m_oldIdle = true;
         m_screen->Refresh(); // need one refresh to display "TRIG?"
      }

      m_timer->Start(10, true);
   } else {
      m_screen->Refresh();
      m_timer->Start(40, true);
   }
}

/*------------------------------------------------------------------*/

void DOFrame::ProcessEvents(void)
{
   int status;
   wxStopWatch sw;

   while (m_osci->HasNewEvent() && sw.Time() < 100) {
      m_osci->ReadWaveforms();
      if (m_rearm) {
         m_osci->Start();
         m_rearm = false;
      }

      if (m_trgCorr)
         m_osci->CorrectTriggerPoint(GetTrgPosition(0));
      if (m_WFFile || m_WFfd) {
         status = m_osci->SaveWaveforms(m_WFFile, m_WFfd);
         if (status < 0)
            CloseWFFile(true);
         IncrementSaved();
         if (m_nSaved >= m_nSaveMax)
            CloseWFFile(false);
      }

      EvaluateMeasurements();
      IncrementAcquisitions();

      if (m_osci->GetNumberOfBoards() == 0)
         break;
   }

   m_screen->Refresh();
}

void DOFrame::IncrementAcquisitions()
{
   if (g_epcs)
      g_epcs->Enter();
   m_acquisitions++;
   if (g_epcs)
      g_epcs->Leave();
}

void DOFrame::IncrementSaved()
{
   m_nSaved++;
}

/*------------------------------------------------------------------*/

void DOFrame::CloseWFFile(bool errorFlag)
{
   if (errorFlag)
      wxMessageBox(wxT("Error writing waveforms to file\nWriting will be stopped"),
                     wxT("DRS Oscilloscope Error"), wxOK | wxICON_STOP, this);

   if (g_epcs)
      g_epcs->Enter();
   if (m_WFFile)
      mxml_close_file(m_WFFile);
   if (m_WFfd)
      close(m_WFfd);
   m_WFFile = NULL;
   m_WFfd   = 0;
   if (g_epcs)
      g_epcs->Leave();
}

/*------------------------------------------------------------------*/

void DOFrame::SetSaveBtn(wxString l, wxString t)
{
   m_btSave->SetLabel(l);
   m_btSave->SetToolTip(t);
}

/*------------------------------------------------------------------*/

void DOFrame::EvaluateMeasurements()
{
   double x1[2048], y1[2048], x2[2048], y2[2048];

   double t1 = m_screen->GetT1();
   double t2 = m_screen->GetT2();

   for (int idx = 0 ; idx<Measurement::N_MEASUREMENTS ; idx++) {
      for (int chn=0 ; chn<4 ; chn++) {
         Measurement *m;
         m = GetMeasurement(idx, chn);

         int wfIndex;
         if (m_osci->IsMultiBoard())
            wfIndex = m_osci->GetCurrentBoardIndex();
         else
            wfIndex = 0;

         if (m && m_osci->GetWaveformDepth(chn)) {
            float *wf1 = m_osci->GetWaveform(wfIndex, chn);
            float *wf2 = m_osci->GetWaveform(wfIndex, (chn+1)%4);
            float *time1 = m_osci->GetTime(wfIndex, chn);
            float *time2 = m_osci->GetTime(wfIndex, (chn+1)%4);
            int   n_inside = 0;

            for (int i=0 ; i<m_osci->GetWaveformDepth(chn) ; i++) {
               if (time1[i] >= t1) {
                  x1[n_inside] = time1[i];
                  y1[n_inside] = wf1[i];
                  x2[n_inside] = time2[i];
                  y2[n_inside] = wf2[i];
                  n_inside++;
               }
               if (time1[i] > t2)
                  break;
            }

            m->Measure(x1, y1, x2, y2, n_inside);
         }
      }
   }
}

/*------------------------------------------------------------------*/

void DOFrame::OnCursor(wxCommandEvent& event)
{
   if (event.GetId() == ID_CURSORA) {
      if (m_toggleCursorA->GetValue() == 1) {
         m_cursorA = true;
         m_actCursor = 1;
      } else {
         m_cursorA = false;
         if (m_actCursor == 1)
            m_actCursor = 0;
      }
   } else {
      if (m_toggleCursorB->GetValue() == 1) {
         m_cursorB = true;
         m_actCursor = 2;
      } else {
         m_cursorB = false;
         if (m_actCursor == 2)
            m_actCursor = 0;
      }
   }

   m_screen->Refresh();
}

void DOFrame::SetCursorA(bool flag)
{
   m_cursorA = flag;
   m_toggleCursorA->SetValue(flag);
}

void DOFrame::SetCursorB(bool flag)
{
   m_cursorB = flag;
   m_toggleCursorA->SetValue(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::OnSnap(wxCommandEvent& event)
{
   m_snap = event.IsChecked();
}

/*------------------------------------------------------------------*/

void DOFrame::SetMeasurement(int id, bool flag)
{
   int m, chn;

   m = (id - ID_LEVEL1) / 4;
   chn = (id - ID_LEVEL1) % 4;

   if (m < Measurement::N_MEASUREMENTS) {
      m_measFlag[m][chn] = flag;
      m_measurement[m][chn]->ResetStat();
   }
}

/*------------------------------------------------------------------*/

void DOFrame::SetMathDisplay(int id, bool flag)
{
   m_screen->SetMathDisplay(id, flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetStat(bool flag) 
{ 
   m_stat = flag;
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::SetHist(bool flag) 
{ 
   m_hist = flag;
}

/*------------------------------------------------------------------*/

void DOFrame::SetStatNStat(int n) 
{ 
   for (int i=0 ; i<Measurement::N_MEASUREMENTS ; i++)
      for (int j=0 ; j<4 ; j++)
         m_measurement[i][j]->SetNStat(n);
}


/*------------------------------------------------------------------*/

void DOFrame::SetIndicator(bool flag) 
{ 
   m_indicator = flag;
}

/*------------------------------------------------------------------*/

Measurement* DOFrame::GetMeasurement(int m, int chn)
{
   if (m>=0 && m<Measurement::N_MEASUREMENTS && chn>=0 && chn<4)
      if (m_measFlag[m][chn])
         return m_measurement[m][chn];

   return NULL;
}

/*------------------------------------------------------------------*/

void DOFrame::StatReset()
{
   for (int i=0 ; i<Measurement::N_MEASUREMENTS ; i++)
      for (int j=0 ; j<4 ; j++)
         m_measurement[i][j]->ResetStat();
}

/*------------------------------------------------------------------*/

void DOFrame::SetSamplingSpeed(double speed)
{
   if (speed > m_osci->GetMaxSamplingSpeed())
      speed = m_osci->GetMaxSamplingSpeed();
   if (speed < m_osci->GetMinSamplingSpeed())
      speed = m_osci->GetMinSamplingSpeed();

    // remember current trigger position inside screen
   double trgFrac = (GetTrgPosition(m_board)-m_screen->GetScreenOffset(m_board)) / m_screen->GetScreenSize(m_board);

   m_reqSamplingSpeed = speed;
   m_osci->SetSamplingSpeed(speed);

   RecalculateHOffset(trgFrac);
   StatReset();
}

/*------------------------------------------------------------------*/

void DOFrame::EnableTriggerConfig(bool flag)
{
   m_trgConfigEnabled[m_board] = flag;
   m_btTrgCfg->Enable(flag);
   m_btTrgCfg->Show(flag);
}

/*------------------------------------------------------------------*/

void DOFrame::SetTriggerConfig(int id, bool flag)
{
   int chn;

   if (id >= ID_OR1 && id <= ID_OREXT) {
      chn = (id - ID_OR1);
      if (flag)
         m_trgConfig[m_board] |= (1 << chn);
      else
         m_trgConfig[m_board] &= ~(1 << chn);
   }

   if (id >= ID_AND1 && id <= ID_ANDEXT) {
      chn = (id - ID_AND1);
      if (flag)
         m_trgConfig[m_board] |= (1 << (chn+8));
      else
         m_trgConfig[m_board] &= ~(1 << (chn+8));
   }

   if (id == ID_TRANS) {
      if (flag) {
         m_trgConfig[m_board] |= (1<<15);
         m_trgConfig[m_board] &= ~((1<<4) | (1<<12)); // remove possible EXT trigger
      } else
         m_trgConfig[m_board] &= ~(1<<15);
   }
   
   if ((m_trgConfig[m_board] & 0x7FFF) > 0) {
      m_rbSource->Enable(false);
      m_osci->SetTriggerConfig(m_trgConfig[m_board]);
   } else {
      m_rbSource->Enable(true);
      m_osci->SetTriggerSource(m_trgSource[m_board]);
   }

}

/*------------------------------------------------------------------*/

void DOFrame::EnableEPThread(bool flag)
{
   if (m_epthread)
      m_epthread->Enable(flag);
}
