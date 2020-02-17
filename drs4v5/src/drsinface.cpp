/********************************************************************\

  Name:         drscl.cpp
  Created by:   Stefan Ritt

  Contents:     Command line interface to DRS chip via USB and VME

  $Id: drscl.cpp 21435 2014-07-30 13:02:31Z ritt $

\********************************************************************/

#include <math.h>

#ifdef _MSC_VER

#include <windows.h>
#include <conio.h>
#include <io.h>
#include <direct.h>

#define DIR_SEPARATOR '\\'

#elif defined(OS_LINUX) || defined(OS_DARWIN)

#define O_BINARY 0

#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DIR_SEPARATOR '/'

#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <assert.h>

#include "strlcpy.h"
#include "DRS.h"
#ifdef HAVE_VME
#include "ace.h"
#endif

const char *drscl_svn_revision = "$Id: drscl.cpp 21435 2014-07-30 13:02:31Z ritt $";

void print_help();
void clear_screen();
int match(const char *str, const char *cmd);
void cmd_loop();

#if defined(OS_LINUX) || defined(OS_DARWIN)
#define getch() getchar()
#define Sleep(x) usleep(x*1000)
#endif

#ifdef _MSC_VER
#include <conio.h>
#define drs_kbhit() kbhit()
#endif

void print_help()
{
   puts("Available commands:\n");
   puts("active <0|1>                Set domino active mode on (1) or off (0)");
   puts("board <i>|<i1> <i2>|all     Address individual board/range/all boards");
   puts("calib [dir]                 Response Calibration. Use dir=\"area\" for MEG");
   puts("chn [n]                     Set number of channels: 8, 4, 2, 1");
   puts("ct                          Chip Test");
   puts("del <0|1>                   Switch delayed start on/off");
   puts("dir                         Show CF directory");
   puts("dmode <0|1>                 Set Domino mode 0=single, 1=cont.");
   puts("et                          EEPROM test");
   puts("exit                        Exit program");
   puts("freq <ghz> [0|1]            Set frequency of board [without/with] regulation");
   puts("info                        Show information about board");
   puts("init                        Initialize board");
   puts("led <0|1>                   Turn LED on (1) or off (0)");
   puts("lock [0|1]                  Display lock status [without/with] restart");
   puts("multi [0|1]                 Turn multi-buffer mode on/off");
   puts("offset <voltage>            Set offset voltage");
   puts("phase <value> [0|1]         Set ADC clock phase and inversion");
   puts("quit                        Exit program");
   puts("ram                         Test speed to FPGA RAM");
   puts("range <center>              Change input range to <center>+=0.5V");
   puts("read <chn> [0|1] [file]     Read waveform to [file], chn=0..19 [with] calibration");
   puts("refclk [0|1]                Use FPGA ref. clk (0) or ext. P2 ref. clk (1)");
   puts("reg                         Register test");
   puts("serial <number>             Set serial number of board");
   puts("scan                        Scan for boards");
   puts("standby <0|1>               Turn standby mode on (1) or off (0)");
   puts("start                       Start domino wave");
   puts("stop                        Issue soft trigger");
   puts("tcout [file] [idx_offset]   Print time calibration of DRS4, or write it onto [file]");
   puts("tcs <0|1>                   Timing calibration signal on (1) or off (0)");
   puts("tcalib [freq]               Timing Calibration");
   puts("tlevel <voltage>            Set trigger level in Volts");
   puts("trans <0|1>                 Set transparent mode on (1) or off (0)");
   puts("trig <0|1>                  Hardware trigger on (1) or off (0)");
   puts("upload <file>               Upload ACE file to CF");
   puts("volt off|<voltage>          Turn calibration voltage on/off");

   puts("");
}

/*------------------------------------------------------------------*/

void clear_screen()
{
#ifdef _MSC_VER

   HANDLE hConsole;
   COORD coordScreen = { 0, 0 };        /* here's where we'll home the cursor */
   BOOL bSuccess;
   DWORD cCharsWritten;
   CONSOLE_SCREEN_BUFFER_INFO csbi;     /* to get buffer info */
   DWORD dwConSize;             /* number of character cells in the current buffer */

   hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

   /* get the number of character cells in the current buffer */
   bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
   dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

   /* fill the entire screen with blanks */
   bSuccess = FillConsoleOutputCharacter(hConsole, (TCHAR) ' ', dwConSize, coordScreen, &cCharsWritten);

   /* put the cursor at (0, 0) */
   bSuccess = SetConsoleCursorPosition(hConsole, coordScreen);
   return;

#else
   printf("\033[2J");
#endif
}

/*------------------------------------------------------------------*/

int match(const char *str, const char *cmd)
{
   int i;

   if (str[0] == '\r' || str[0] == '\n')
      return 0;

   for (i = 0; i < (int) strlen(str); i++) {
      if (toupper(str[i]) != toupper(cmd[i]) && str[i] != '\r' && str[i] != '\n')
         return 0;
   }

   return 1;
}

/*------------------------------------------------------------------*/

class ProgressBar : public DRSCallback
{
public:
   void Progress(int prog);
};

void ProgressBar::Progress(int prog)
{
   if (prog == 0)
      printf("[--------------------------------------------------]\r");
   printf("[");
   for (int i=0 ; i<prog/2 ; i++)
      printf("=");
   printf("\r");
   fflush(stdout);
}

/*------------------------------------------------------------------*/

void cmd_loop()
{
   int i, j, idx, i_start, i_end, nparam, calib, status, debug, cascading,
      ext_refclk;
   char str[256], dir[256], line[256], file_name[256], param[10][100], *pc;
   double freq, triggerfreq, range;
   FILE *f = NULL;
   DRS *drs;
   DRSBoard *b;
#ifdef HAVE_VME
   ACE ace;
#endif
   static char bar[] = {'\\', '|', '/', '-'};

   /* do initial scan */
   drs = new DRS();
   if (drs->GetError(str, sizeof(str)))
      printf("%s", str);

   for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
      b = drs->GetBoard(i);
#ifdef HAVE_VME
      if (b->GetTransport() != 1)
         printf("Found DRS%d board %2d on USB, serial #%04d, firmware revision %5d\n", 
            b->GetDRSType(), i, b->GetBoardSerialNumber(), b->GetFirmwareVersion());
#else
      printf("Found DRS%d board %2d on USB, serial #%04d, firmware revision %5d\n", 
         b->GetDRSType(), i, b->GetBoardSerialNumber(), b->GetFirmwareVersion());
#endif
   }

   if (drs->GetNumberOfBoards()) {
      i_start = 0;
      i_end = 1;
      b = drs->GetBoard(0);
   } 
   else {
      printf("No DRS Boards found\n");
      i_start = i_end = 0;
      b = NULL;
   }

   puts("");

   do {
      /* print prompt */
      if (i_start == i_end-1)
         printf("B%d> ", i_start);
      else if (i_start == 0 && i_end == 0)
         printf("> ");
      else
         printf("B%d-%d> ", i_start, i_end-1);
      memset(line, 0, sizeof(line));
      fgets(line, sizeof(line), stdin);
      /* strip \r\n */
      while (strpbrk(line,"\n\r"))
         *strpbrk(line,"\n\r") = 0;

      /* analyze line */
      nparam = 0;
      pc = line;
      while (*pc == ' ')
         pc++;

      memset(param, 0, sizeof(param));
      do {
         if (*pc == '"') {
            pc++;
            for (i = 0; *pc && *pc != '"'; i++)
               param[nparam][i] = *pc++;
            if (*pc)
               pc++;
         } else if (*pc == '\'') {
            pc++;
            for (i = 0; *pc && *pc != '\''; i++)
               param[nparam][i] = *pc++;
            if (*pc)
               pc++;
         } else if (*pc == '`') {
            pc++;
            for (i = 0; *pc && *pc != '`'; i++)
               param[nparam][i] = *pc++;
            if (*pc)
               pc++;
         } else
            for (i = 0; *pc && *pc != ' '; i++)
               param[nparam][i] = *pc++;
         param[nparam][i] = 0;
         while (*pc == ' ' || *pc == '\r' || *pc == '\n')
            pc++;
         nparam++;
      } while (*pc);

      if (param[0][0] == 0) {
      }

      /* help ---------- */
      else if ((param[0][0] == 'h' && param[0][1] == 'e') || param[0][0] == '?')
         print_help();

      /* scan ---------- */
      else if (match(param[0], "scan")) {
         j = 0;

         do {
            delete drs;
            drs = new DRS();

            for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
               b = drs->GetBoard(i);
#ifdef HAVE_VME
               if (b->GetTransport() != 1)
                  printf("Found DRS%d board %2d on USB, serial #%04d, firmware revision %5d\n", 
                     b->GetDRSType(), i, b->GetBoardSerialNumber(), b->GetFirmwareVersion());
#else
               printf("Found DRS%d board %2d on USB, serial #%04d, firmware revision %5d\n", 
                  b->GetDRSType(), i, b->GetBoardSerialNumber(), b->GetFirmwareVersion());
#endif
            }
            
            if (drs_kbhit())
               break;

            if (param[1][0] == 'r') {
               printf("%c\r", bar[j]);
               fflush(stdout);
               j = (j+1) % 4;
               Sleep(1000);
            }

         } while (param[1][0] == 'r');

         while (drs_kbhit())
            getch();

         if (drs->GetNumberOfBoards()) {
            i_start = 0;
            i_end = 1;
            b = drs->GetBoard(0);
         } else {
            printf("No DRS Boards found\n");
            i_start = i_end = 0;
            b = NULL;
         }
      }

      /* address board ---------- */
      else if (match(param[0], "board")) {
         if (param[1][0] == 'a') {
            i_start = 0;
            i_end = drs->GetNumberOfBoards();
            b = drs->GetBoard(0);
         } else if (param[2][0] && atoi(param[2]) > 0 && atoi(param[2]) < drs->GetNumberOfBoards()) {
            i_start = atoi(param[1]);
            i_end = atoi(param[2]) + 1;
            b = drs->GetBoard(i_start);
         } else if (atoi(param[1]) >= 0 && atoi(param[1]) < drs->GetNumberOfBoards()) {
            i_start = atoi(param[1]);
            i_end = i_start + 1;
            b = drs->GetBoard(i_start);
         } else
            printf("Board #%d does not exist\n", atoi(param[1]));
      }

      /* info ---------- */
      else if (match(param[0], "info")) {
         for (idx=i_start ; idx<i_end ; idx++) {
            b = drs->GetBoard(idx);
            printf("==============================\n");
            printf("Mezz. Board index:    %d\n", idx);
#ifdef HAVE_VME
            if (b->GetTransport() == TR_VME) {
               printf("Slot:                 %d", (b->GetSlotNumber() >> 1)+2);
               if ((b->GetSlotNumber() & 1) == 0)
                  printf(" upper\n");
               else
                  printf(" lower\n");
            }
#endif
            printf("DRS type:             DRS%d\n", b->GetDRSType());
            printf("Board type:           %d\n", b->GetBoardType());
            printf("Serial number:        %04d\n", b->GetBoardSerialNumber());
            printf("Firmware revision:    %d\n", b->GetFirmwareVersion());
            printf("Temperature:          %1.1lf C\n", b->GetTemperature());
            if (b->GetDRSType() == 4) {
               printf("Input range:          %1.2lgV...%1.2lgV\n", 
                      b->GetInputRange()-0.5, b->GetInputRange()+0.5);
               printf("Calibrated range:     %1.2lgV...%1.2lgV\n", b->GetCalibratedInputRange()-0.5,
                  b->GetCalibratedInputRange()+0.5);
               printf("Calibrated frequency: %1.3lf GHz\n", b->GetCalibratedFrequency());

               if (b->GetTransport() == TR_VME) {
                  printf("Multi Buffer WP:      %d\n", b->GetMultiBufferWP());
                  printf("Multi Buffer RP:      %d\n", b->GetMultiBufferRP());
               }
            }
            
            printf("Status reg.:          %08X\n", b->GetStatusReg());
            if (b->GetStatusReg() & BIT_RUNNING)
               puts("  Domino wave running");
            if (b->GetDRSType() == 4) {
               if (b->GetBoardType() == 5) {
                  if (b->GetStatusReg() & BIT_PLL_LOCKED0)
                     puts("  PLL locked");
               } else if (b->GetBoardType() == 6) {
                  i = 0;
                  if (b->GetStatusReg() & BIT_PLL_LOCKED0) i++;
                  if (b->GetStatusReg() & BIT_PLL_LOCKED1) i++;
                  if (b->GetStatusReg() & BIT_PLL_LOCKED2) i++;
                  if (b->GetStatusReg() & BIT_PLL_LOCKED3) i++;
                  if (i == 4)
                     puts("  All PLLs locked");
                  else if (i == 0)
                     puts("  No PLL locked");
                  else
                     printf("  %d PLLs locked\n", i);
                  if (b->GetStatusReg() & BIT_LMK_LOCKED)
                     puts("  LMK PLL locked");
               }
            } else {
               if (b->GetStatusReg() & BIT_NEW_FREQ1)
                  puts("  New Freq1 ready");
               if (b->GetStatusReg() & BIT_NEW_FREQ2)
                  puts("  New Freq2 ready");
            }
            
            printf("Control reg.:         %08X\n", b->GetCtrlReg());
            if (b->GetCtrlReg() & BIT_MULTI_BUFFER)
               puts("  Multi-buffering enabled");
            if (b->GetDRSType() == 4) {
               if (b->GetConfigReg() & BIT_CONFIG_DMODE)
                  puts("  DMODE circular");
               else
                  puts("  DMODE single shot");
            } else {
               if (b->GetCtrlReg() & BIT_DMODE)
                  puts("  DMODE circular");
               else
                  puts("  DMODE single shot");
            }
            if (b->GetCtrlReg() & BIT_LED)
               puts("  LED");
            if (b->GetCtrlReg() & BIT_TCAL_EN)
               puts("  TCAL enabled");
            if (b->GetDRSType() == 4) {
               if (b->GetCtrlReg() & BIT_TRANSP_MODE)
                  puts("  TRANSP_MODE enabled");
            } else {
               if (b->GetCtrlReg() & BIT_FREQ_AUTO_ADJ)
                  puts("  FREQ_AUTO_ADJ enabled");
            }
            if (b->GetCtrlReg() & BIT_ENABLE_TRIGGER1)
               puts("  Hardware trigger enabled");
            if (b->GetDRSType() == 4) {
               if (b->GetCtrlReg() & BIT_READOUT_MODE)
                  puts("  Readout from stop");
               if (b->GetCtrlReg() & BIT_ENABLE_TRIGGER2)
                  puts("  Internal trigger enabled");
            } else {
               if (b->GetCtrlReg() & BIT_LONG_START_PULSE)
                  puts("  LONG_START_PULSE");
            }
            if (b->GetCtrlReg() & BIT_DELAYED_START)
               puts("  DELAYED_START");
            if (b->GetCtrlReg() & BIT_ACAL_EN)
               puts("  ACAL enabled");
            if (b->GetDRSType() < 4)
               if (b->GetCtrlReg() & BIT_TRIGGER_DELAYED)
                  puts("  DELAYED_TRIGGER selected");
            if (b->GetBoardType() != 5)
               printf("Trigger bus:          %08X\n", b->GetTriggerBus());
            if (b->GetDRSType() == 4) {
               if (b->GetRefclk() == 1) {
                  if (b->IsPLLLocked() && b->IsLMKLocked()) {
                     b->ReadFrequency(0, &freq);
                     printf("Frequency:            %1.3lf GHz\n", freq);
                  } else {
                     if (!b->IsPLLLocked())
                        printf("Frequency:            PLL not locked\n");
                     else
                        printf("Frequency:            LMK chip not locked\n");
                  }
               } else {
                  if (b->IsPLLLocked()) {
                     b->ReadFrequency(0, &freq);
                     printf("Frequency:            %1.3lf GHz\n", freq);
                  } else {
                     printf("Frequency:            PLL not locked\n");
                  }
               }
            } else {
               if (b->IsBusy()) {
                  b->ReadFrequency(0, &freq);
                  printf("Frequency0:           %1.4lf GHz\n", freq);
                  b->ReadFrequency(1, &freq);
                  printf("Frequency1:           %1.4lf GHz\n", freq);
               } else
                  puts("Domino wave stopped");
            }
         }
      }

      /* init ---------- */
      else if (match(param[0], "init")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->Init();
         }
      }

      /* set led ---------- */
      else if (match(param[0], "led")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (atoi(param[1]))
               b->SetLED(1);
            else
               b->SetLED(0);
         }
      }

      /* set multi buffer mode ---------- */
      else if (match(param[0], "multi")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (atoi(param[1]))
               b->SetMultiBuffer(1);
            else
               b->SetMultiBuffer(0);
         }
      }

      /* lock status ---------- */
      else if (match(param[0], "lock")) {
         int slot, found, restart;

         restart = atoi(param[1]);

         // select external reference clock
         for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
            b = drs->GetBoard(i);
            b->SetRefclk(1);
            b->SetFrequency(b->GetNominalFrequency(), true);
         }

         // loop until keyboard hit
         do {
            clear_screen();
            printf("                1 1 1 1 1 1 1 1 1 1 2 2\n");
            printf("2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1\n\n");

            // upper slots
            for (slot = 2 ; slot<22 ; slot++) {
               found = 0;
               for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
                  b = drs->GetBoard(i);
                  if ((b->GetSlotNumber() & 1) == 0 && (b->GetSlotNumber() >> 1)+2 == slot) {
                     found = 1;
                     if (b->IsLMKLocked())
                        printf("O ");
                     else
                        printf("- ");
                  }
               }
               if (!found)
                  printf("  ");
            }
            printf("\n");

            // lower slots
            for (slot = 2 ; slot<22 ; slot++) {
               found = 0;
               for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
                  b = drs->GetBoard(i);
                  if ((b->GetSlotNumber() & 1) == 1 && (b->GetSlotNumber() >> 1)+2 == slot) {
                     found = 1;
                     if (b->IsLMKLocked())
                        printf("O ");
                     else
                        printf("- ");
                  }
               }
               if (!found)
                  printf("  ");
            }
            printf("\n");

            if (restart) {
               for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
                  b = drs->GetBoard(i);
                  b->SetFrequency(b->GetNominalFrequency(), true);
               }
            }

            Sleep(300);

         } while (!drs_kbhit());
         puts("");
         while (drs_kbhit())
            getch();
      }

      /* start domino wave ---------- */
      else if (match(param[0], "start")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->StartDomino();
            b->ReadFrequency(0, &freq);
            for (j=0 ; j<10 ; j++)
               if (b->GetDRSType() != 4 || b->IsPLLLocked())
                  break;
            if (j == 10)
               printf("Domino wave started but PLL did not lock!\n");
            else
               printf("Domino wave started at %1.3lf GHz\n", freq);
         }
      }

      /* issue soft trigger ---------- */
      else if (match(param[0], "stop")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->SoftTrigger();
         }
      }

      /* set serial ---------- */
      else if (match(param[0], "serial")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Serial number: ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            if (!b->SetBoardSerialNumber(atoi(str)))
               printf("Board EEPROM is write protected\n");
            else
               printf("Serial number successfully changed\n");
         }
      }

      /* eeprom test ---------- */
      else if (match(param[0], "et")) {
         unsigned short buf[16384];
         unsigned short rbuf[16384];
         int n_error;

         do {
            for (i=0 ; i<16384 ; i++)
               buf[i] = rand();
            b->WriteEEPROM(1, buf, sizeof(buf));
            memset(rbuf, 0, sizeof(rbuf));
            b->Write(T_RAM, 0, rbuf, sizeof(rbuf));
            b->ReadEEPROM(1, rbuf, sizeof(rbuf));
            for (i=n_error=0 ; i<16384 ; i++)
               if (buf[i] != rbuf[i]) {
                  printf("%04X %04X - %04X\n", i, buf[i], rbuf[i]);
                  n_error++;
               }

            printf("32 kb written, %d errors\n", n_error);
         } while (!drs_kbhit());

         while (drs_kbhit())
            getch();
      }

      /* set frequency ---------- */
      else if (match(param[0], "freq")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Frequency: ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            b->SetDebug(1);

            if (param[2][0] && atoi(param[2]))
               b->RegulateFrequency(atof(str));
            else
               b->SetFrequency(atof(str), true);
         }
      }

      /* set calibration voltage ---------- */
      else if (match(param[0], "volt")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Voltage or \"off\": ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            if (str[0] == 'o') {
               b->EnableAcal(0, 0);
               puts("Calibration voltage turned off");
            } else {
               b->EnableAcal(1, atof(str));
               printf("Voltage set to %1.3lf Volt\n", atof(str));
            }
         }
      }

      /* set channel configuration ---------- */
      else if (match(param[0], "chn")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Number of channels (8,4,2,1): ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            if (b->SetChannelConfig(0, 8, atoi(str)))
               printf("DRS4 configured for %d channels\n", atoi(str));
         }
      }

      /* set trigger level ---------- */
      else if (match(param[0], "tlevel")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Voltage: ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            b->SetTriggerLevel(atof(str));
            printf("Trigger level set to %1.3lf Volt\n", atof(str));
         }
      }

      /* trigger on/off ---------- */
      else if (match(param[0], "trig")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->EnableTrigger(atoi(param[1]), 0);
            if (atoi(param[1]) == 1) {
               puts("Hardware fast trigger is on");
            } else if (atoi(param[1]) == 2) {
               puts("Hardware slow trigger is on");
            } else {
               puts("Hardware trigger is off");
            }
         }
      }

      /* timing calibration signal on/off ---------- */
      else if (match(param[0], "tcs")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->EnableTcal(atoi(param[1]), 0, 0);
            b->SelectClockSource(0);
            if (atoi(param[1]))
               puts("Timing calibration signal is on");
            else
               puts("Timing calibration signal is off");
         }
      }

      /* timing calibration signal on/off ---------- */
      else if (match(param[0], "refclk")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->SetRefclk(atoi(param[1]));
            // re-set frequency since LMK configuration needs to be changed
            b->SetFrequency(b->GetNominalFrequency(), true);
            if (atoi(param[1]))
               puts("Refclock set to external through P2");
            else
               puts("Refclock set to internal (FPGA)");
         }
      }

      /* domino mode 0/1 ---------- */
      else if (match(param[0], "dmode")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (atoi(param[1]) == 1) {
               b->SetDominoMode(1);
               puts("Domino mode switched to cyclic");
            } else {
               b->SetDominoMode(0);
               puts("Domino mode switched to single shot");
            }
         }
      }

      /* active mode 0/1 ---------- */
      else if (match(param[0], "active")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (atoi(param[1]) == 1) {
               b->SetDominoActive(1);
               puts("Domino wave active during readout");
            } else {
               b->SetDominoMode(0);
               puts("Domino wave stopped during readout");
            }
         }
      }

      /* delayed start on/off ---------- */
      else if (match(param[0], "del")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (b->GetDRSType() == 4)
               puts("Delayed start not possible for DRS4");
            else {
               if (atoi(param[1]) == 1) {
                  b->SetDelayedStart(1);
                  puts("Delayed start is on");
               } else {
                  b->SetDelayedStart(0);
                  puts("Delayed start is off");
               }
            }
         }
      }

      /* transparent mode on/off ---------- */
      else if (match(param[0], "trans")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (b->GetDRSType() != 4)
               puts("Transparen mode only possible for DRS4");
            else {
               if (atoi(param[1]) == 1) {
                  b->SetTranspMode(1);
                  puts("Transparent mode is on");
               } else {
                  b->SetTranspMode(0);
                  puts("Transparent mode is off");
               }
            }
         }
      }

      /* standby mode on/off ---------- */
      else if (match(param[0], "standby")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (b->GetDRSType() != 4)
               puts("Standby mode only possible for DRS4");
            else {
               if (atoi(param[1]) == 1) {
                  b->SetStandbyMode(1);
                  puts("Standby mode is on");
               } else {
                  b->SetStandbyMode(0);
                  puts("Standby mode is off");
               }
            }
         }
      }

      /* offset ---------- */
      else if (match(param[0], "offset")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->SetVoltageOffset(atof(param[1]), atof(param[2]));
         }
      }

      /* phase ---------- */
      else if (match(param[0], "phase")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            b->SetADCClkPhase(atoi(param[1]), atoi(param[2]) > 0);
         }
      }

      /* directory ---------- */
      else if (match(param[0], "dir")) {

#ifdef HAVE_VME
#ifdef CF_VIA_USB
         {
            if (param[2][0])
               i = atoi(param[2]);
            else
               i = 1;
            printf("Physical drive %d:\n", i);

            if (ace_init(NULL, i, &ace) != ACE_SUCCESS) {
               printf("Cannot access ACE on physical drive %d\n", i);
            } else {
#else
         for (i=i_start ; i<i_end ; i++) {

            /* do only once per VME board */
            if (i_end - i_start > 1 && (i % 2) == 1)
               continue;

            b = drs->GetBoard(i);

            printf("VME slot %2d:   ", (b->GetSlotNumber() >> 1) + 2);

            if (ace_init(b->GetVMEInterface(), (b->GetSlotNumber() >> 1)+2, &ace) != ACE_SUCCESS) {
               printf("Cannot access ACE in slot %d\n", (b->GetSlotNumber() >> 1)+2);
            } else {
#endif
            ace_dir(&ace);
            }
         }
#else
         printf("No VME support compiled into drscl\n");
#endif // HAVE_VME
      }

      /* upload ---------- */
      else if (match(param[0], "upload")) {

#ifdef HAVE_VME
#ifdef CF_VIA_USB
         {
            if (param[2][0])
               i = atoi(param[2]);
            else
               i = 1;
            printf("Physical drive %d:\n", i);

            if (ace_init(NULL, i, &ace) != ACE_SUCCESS) {
               printf("Cannot access ACE on physical drive %d\n", i);
            } else {
#else

            /* use SVN file as default */
            if (param[1][0] == 0) {
#ifdef _MSC_VER
               if (b->GetDRSType() == 4)
                  strcpy(str, "c:\\meg\\online\\VPC\\drs4\\2vp30\\cflash\\drs4\\rev0\\rev0.ace");
               else if (b->GetDRSType() == 3)
                  strcpy(str, "c:\\meg\\online\\VPC\\drs3\\2vp30\\cflash\\drs3\\rev0\\rev0.ace");
               else
                  strcpy(str, "c:\\meg\\online\\VPC\\drs2\\2vp30\\cflash\\drs2\\rev0\\rev0.ace");
#else
               if (b->GetDRSType() == 4)
                  strcpy(str, "/home/meg/meg/online/VPC/drs4/2vp30/cflash/drs4/rev0/rev0.ace");
               else if (b->GetDRSType() == 3)
                  strcpy(str, "/home/meg/meg/online/VPC/drs3/2vp30/cflash/drs3/rev0/rev0.ace");
               else
                  strcpy(str, "/home/meg/meg/online/VPC/drs2/2vp30/cflash/drs2/rev0/rev0.ace");
#endif
               printf("Enter filename or hit return for \n%s\n", str);
               fgets(line, sizeof(line), stdin);
               if (line[0] == '\r' || line[0] == '\n')
                  strcpy(file_name, str);
               else
                  strcpy(file_name, line);
               strcpy(param[1], str);
            } else
               strcpy(file_name, param[1]);

         for (i=i_start ; i<i_end ; i++) {

            /* do only once per VME board */
            if (i_end - i_start > 1 && (i % 2) == 1)
               continue;

            b = drs->GetBoard(i);

            if (b->GetTransport() == TR_USB) {
               printf("Cannot upload to USB board.\n");
            } else {
               printf("VME slot %d:\n", (b->GetSlotNumber() >> 1)+2);
               if (ace_init(b->GetVMEInterface(), (b->GetSlotNumber() >> 1)+2, &ace) != ACE_SUCCESS) {
                  printf("Cannot access ACE in slot %d\n", (b->GetSlotNumber() >> 1)+2);
               } else {
#endif
                  status = ace_upload(&ace, file_name);
               }
            }
         }
         printf("\nPlease issue a power cycle to activate new firmware\n");
#else
         printf("No VME support compiled into drscl\n");
#endif // HAVE_VME
      }

      /* download ---------- */
      else if (match(param[0], "download")) {

#ifdef HAVE_VME
         b = drs->GetBoard(i_start);

         if (b->GetTransport() == TR_USB) {
            printf("Cannot upload to USB board.\n");
         } else {
            printf("VME slot %d:\n", (b->GetSlotNumber() >> 1)+2);
            if (ace_init(b->GetVMEInterface(), (b->GetSlotNumber() >> 1)+2, &ace) != ACE_SUCCESS) {
               printf("Cannot access ACE in slot %d\n", (b->GetSlotNumber() >> 1)+2);
            } else {
               strcpy(str, "rev0.ace");
               if (param[1][0] == 0) {
                  printf("Enter filename or hit return for \n%s\n", str);
                  fgets(line, sizeof(line), stdin);
                  if (line[0] == '\r' || line[0] == '\n')
                     strcpy(file_name, str);
                  else
                     strcpy(file_name, line);
                  strcpy(param[1], str);
               } else
                  strcpy(file_name, param[1]);

               if (strchr(file_name, '\r'))
                  *strchr(file_name, '\r') = 0;
               if (strchr(file_name, '\n'))
                  *strchr(file_name, '\n') = 0;

               status = ace_download(&ace, file_name);
               }
         }
#else
         printf("No VME support compiled into drscl\n");
#endif // HAVE_VME
      }

      /* calibration ---------- */
      else if (match(param[0], "calib")) {
         debug = strcmp(param[1], "debug") == 0 || strcmp(param[2], "debug") == 0 || strcmp(param[3], "debug") == 0;
         if (param[1][0]) {
            strlcpy(dir, param[1], sizeof(str));
         } else
            getcwd(dir, sizeof(dir));

         while (dir[strlen(dir)-1] == '\n' || dir[strlen(dir)-1] == '\r')
           dir[strlen(dir)-1] = 0;

         b = drs->GetBoard(i_start);

         printf("\n           Enter calibration frequency [GHz]: ");
         fgets(line, sizeof(line), stdin);
         freq = atof(line);

         if (b->GetDRSType() == 2) {
            printf("   Enter the expected trigger frequency [Hz]: ");
            fgets(line, sizeof(line), stdin);
            triggerfreq = atof(line);
         } else
            triggerfreq = 0;

         ext_refclk = 0;
         if (b->GetBoardType() == 6) {
            printf("Use [e]xternal or [i]nternal reference clock: ");
            fgets(line, sizeof(line), stdin);
            ext_refclk = line[0] == 'e';
         }

         if (b->GetDRSType() == 4) {
            printf("                             Enter range [V]: ");
            fgets(line, sizeof(line), stdin);
            range = atof(line);

            printf("        Enter mode [1]024 or [2]048 bin mode: ");
            fgets(line, sizeof(line), stdin);
            cascading = atoi(line);
         } else {
            range = 0;
            cascading = 0;
         }

         if (b->GetDRSType() == 4) {
            printf("\nPlease make sure that no input signal are present then hit any key\r");
            fflush(stdout);
            while (!drs_kbhit());
            printf("                                                                  \r");
            while (drs_kbhit())
               getchar();
         }

         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (b->GetTransport() == TR_VME)
               printf("Creating Calibration of Board in VME slot %2d %s, serial #%04d\n",  
                       (b->GetSlotNumber() >> 1)+2, ((b->GetSlotNumber() & 1) == 0) ? "upper" : "lower", 
                        b->GetBoardSerialNumber());
            else
               printf("Creating Calibration of Board on USB, serial #%04d\n",  
                        b->GetBoardSerialNumber());
            if (b->GetDRSType() == 4) {
               ProgressBar p;
               if (b->GetTransport() == TR_VME) {
                  if (cascading == 2)
                     b->SetChannelConfig(7, 8, 4);  // 7 means read all 9 channels per chip
                  else
                     b->SetChannelConfig(7, 8, 8);
               } else {
                  if (cascading == 2)
                     b->SetChannelConfig(0, 8, 4);
                  else
                     b->SetChannelConfig(0, 8, 8);
               }

               b->SetRefclk(ext_refclk);
               b->SetFrequency(freq, true);
               b->SetInputRange(range);
               b->CalibrateVolt(&p);
            } else {
               b->SetDebug(debug);
               b->Init();
               b->SetFrequency(freq, true);
               b->SoftTrigger();

               if (b->GetDRSType() == 3)
                  b->GetResponseCalibration()->SetCalibrationParameters(1,11,0,20,0,0,0,0,0);
               else
                  b->GetResponseCalibration()->SetCalibrationParameters(1,36,110,20,19,40,15,triggerfreq,0);
               if (!strcmp(dir,"lab"))
                  b->SetCalibrationDirectory("C:/experiment/calibrations");
               else if (!strcmp(dir,"area"))
                  b->SetCalibrationDirectory("/home/meg/meg/online/calibrations");
               else
                  b->SetCalibrationDirectory(dir);
               for (j=0;j<2;j++) {
                  b->GetResponseCalibration()->ResetCalibration();
                  while (!b->GetResponseCalibration()->RecordCalibrationPoints(j)) {}
                  while (!b->GetResponseCalibration()->FitCalibrationPoints(j)) {}
                  while (!b->GetResponseCalibration()->OffsetCalibration(j)) {}
                  if (!b->GetResponseCalibration()->WriteCalibration(j))
                     break;
               }
            }
         }
      }
        
      /* timing calibration ---------- */
      else if (match(param[0], "tcalib")) {
         
         freq = 0;
         if (param[1][0])
            freq = atof(param[1]);

         if (freq == 0) {
            printf("Enter calibration frequency [GHz]: ");
            fgets(line, sizeof(line), stdin);
            freq = atof(line);
         }
         
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (b->GetDRSType() < 4)
               printf("Timing calibration not possivle for DRS2 or DRS3\n");
            else if (b->GetFirmwareVersion() < 13279)
               printf("Firmware revision 13279 or later required for timing calibration\n");
            else if (b->GetDRSType() == 4) {
               printf("Creating Timing Calibration of Board #%d\n", b->GetBoardSerialNumber());
               ProgressBar p;
               b->SetFrequency(freq, true);
               status = b->CalibrateTiming(&p);
               if (!status)
                  printf("Error performing timing calibration, please check waveforms\n");
               printf("\n");
            }
         }
      }

      /* tcout ---------- */
      else if (match(param[0], "tcout")) {
         float time[1024];
         int chip;
         int k;
         int idx = 0;
         int first_board = i_start;
         int last_board = i_end;

         file_name[0] = 0;
         strcpy(file_name, param[1]);
         if (file_name[0]) {
            f = fopen(file_name, "wt");
            if (f == NULL) {
               printf("Cannot open file \"%s\"\n", file_name);
            } else {
               first_board = 0;
               last_board = drs->GetNumberOfBoards();
            }
            idx += atoi(param[2]);
         } else
            f = NULL;

         if (f) {
            fprintf(f, "-- Replace %%%% with correct id\n");
         }
         for (i=first_board ; i<last_board ; i++) {
            b = drs->GetBoard(i);
            if (b->GetDRSType() >= 4) {
               for (chip = 0; chip < b->GetNumberOfChips(); chip++) {
                  b->GetTime(chip, 0, b->GetTriggerCell(0), time, true, false);
                  if (f) {
                     fprintf(f, "INSERT INTO MEGDRSTimeCalibration VALUES(%%%%,%d,%d", idx,
                            static_cast<int>(b->GetNominalFrequency() * 10 + 0.5) * 100);
                     for (j=0 ; j<1024 ; j++)
                        fprintf(f, ",%g", time[j] * 1e-9);
                     fprintf(f, ",%d,%d", b->GetBoardSerialNumber(), chip);
                     fprintf(f, ",%g);\n", 1 / (b->GetNominalFrequency() * 1e9) * 1024);
                     idx++;
                  } else {
                     printf("Board %d\n", b->GetBoardSerialNumber());
                     for (j=0 ; j<128 ; j++) {
                        printf("%4d: ", j*8);
                        for (k=0 ; k<7 ; k++)
                           printf("%6.1lf ", time[j*8+k]);
                        printf("%6.1lf\n", time[j*8+k]);
                     }
                     printf("n");
                  }
               }
            } else {
               // DRS2 or DRS3
               idx += 2;
            }
         }
         if (f) {
            fclose(f);
            printf("Data successfully written to \"%s\"\n", file_name);
         }
      }

      /* read */   
      else if (match(param[0], "read")) {
         float waveform[2048];
         short swaveform[2048];
         calib = 0;

         file_name[0] = 0;
         if (param[1][0]) {
            idx = atoi(param[1]);
            calib = atoi(param[2]);
            if (strlen(param[2]) > 2)
               strcpy(file_name, param[2]);
            else
               strcpy(file_name, param[3]);
         } else {
            printf("Enter channel number (0..19): ");
            fgets(line, sizeof(line), stdin);
            idx = atoi(line);
         }

         if (idx<0 || idx>19)
            printf("Channel number must be between 0 and 19\n");
         else {
            b = drs->GetBoard(i_start);
            if (!b->IsEventAvailable())
               printf("Error: Domino wave is running, please issue a \"stop\" first\n");
            else {
               if (calib == 1) {
                  if (b->GetDRSType() == 4) {
                     if (!b->IsVoltageCalibrationValid()) {
                        printf("Calibration not valid for board #%d\n", b->GetBoardSerialNumber());
                        calib = 0;
                     }

                  } else {
#ifdef _MSC_VER
                     b->SetCalibrationDirectory("C:/experiment/calibrations");
#else
                     b->SetCalibrationDirectory("/home/meg/meg/online/calibrations");
#endif
                     if (!b->GetResponseCalibration()->IsRead(0))
                        if (!b->GetResponseCalibration()->ReadCalibration(0))
                           calib = 0;
                     if (!b->GetResponseCalibration()->IsRead(1))
                        if (!b->GetResponseCalibration()->ReadCalibration(1))
                           calib = 0;
                  }
               }

               status = b->TransferWaves(idx, idx);
               if (file_name[0]) {
                  f = fopen(file_name, "wt");
                  if (f == NULL)
                     printf("Cannot open file \"%s\"\n", file_name);
               } else
                  f = NULL;

               if (calib) {
                  status = b->GetWave(idx/b->GetNumberOfChannels(), idx%b->GetNumberOfChannels(), waveform, 
                                      true, b->GetTriggerCell(idx/b->GetNumberOfChannels()), b->GetStopWSR(idx/b->GetNumberOfChannels()));
                  if (status == 0) {
                     if (f)
                        for (i=0 ; i<b->GetChannelDepth() ; i++)
                           fprintf(f, "%6.1lf\n", waveform[i]);
                     else {
                        for (i=0 ; i<b->GetChannelDepth()/8 ; i++) {
                           printf("%4d: ", i*8);
                           for (j=0 ; j<7 ; j++)
                              printf("%6.1lf ", waveform[i*8+j]);
                           printf("%6.1lf\n", waveform[i*8+j]);
                        }
                     }
                  }
               } else {
                  status = b->GetWave(idx/b->GetNumberOfChannels(), idx%b->GetNumberOfChannels(), swaveform, 0, 0);
                  if (status == 0) {
                     if (f)
                        for (i=0 ; i<b->GetChannelDepth() ; i++)
                           fprintf(f, "%4d\n", swaveform[i]);
                     else {
                        for (i=0 ; i<b->GetChannelDepth()/16 ; i++) {
                           for (j=0 ; j<15 ; j++)
                              printf("%4d ", swaveform[i*16+j] >> 4);
                           printf("%4d\n", swaveform[i*16+j] >> 4);
                        }
                     }
                  }
               }
            }
         }

         if (f) {
            fclose(f);
            printf("Data successfully written to \"%s\"\n", file_name);
         }
      }

      /* register test ---------- */
      else if (match(param[0], "reg")) {
         b->RegisterTest();
      }

      /* RAM test */
      else if (match(param[0], "ram")) {
         if (param[1][0] == 0)
            b->RAMTest(3);
         else
            b->RAMTest(atoi(param[1]));
      }
      
      /* Change input range */
      else if (match(param[0], "range")) {
         for (i=i_start ; i<i_end ; i++) {
            b = drs->GetBoard(i);
            if (param[1][0] == 0) {
               printf("Input range: ");
               fgets(str, sizeof(str), stdin);
            } else
               strlcpy(str, param[1], sizeof(str));

            b->SetInputRange(atof(str));
            printf("Range set to %1.2lg V ... %1.2lg V\n", atof(str)-0.5, atof(str)+0.5);
         }
      }

      /* Chip Test */
      else if (match(param[0], "ct")) {
         if (drs->GetNumberOfBoards() == 0)
            puts("No DRS board found");
         else {
            puts("Press 'q' to quit, any other key to repeat test.\n");
            do {
               if (b->ChipTest())
                  puts("Chip test successfully finished");
               else
                  puts("\007Chip Error!");

               b->SetStandbyMode(1);
               for (i=0 ; i<8 ; i++)
                  b->SetDAC(i, 0);
               i = getch();
               b->SetStandbyMode(0);
            } while (i != 'q');
         }
      }
      
      /* calib0 for speed vs. temperature calibration */
      else if (match(param[0], "c0")) {
         
         double volt, freq;
         
         b->Init();
         b->SetFrequency(5, true);
         b->EnableAcal(0, 0);
         b->SetDominoMode(1);

         for (volt=2.5 ; volt > 0 ; volt -= 0.05) {
            printf("%4.1lf - %5.3lf ", b->GetTemperature(), volt);
            b->SetDAC(1, volt);
            b->SetDAC(2, volt);
            Sleep(100);
            b->ReadFrequency(0, &freq);

            printf("%5.3lf\n", freq);

            if (drs_kbhit())
               break;
         }

         while (drs_kbhit())
            getch();

         b->Init(); // reset voltage offset
      }

      /* calib1 */
      else if (match(param[0], "c1")) {
         
         short swaveform[1024];
         double volt;
         double av[1024];
         int k;
         
         b->Init();
         b->SetFrequency(5, true);
         b->SetDominoMode(1);
         b->SetDominoActive(1);
         b->SetReadoutMode(1);

         for (volt=-0.5 ; volt <= 0.5001 ; volt += 0.02) {
            printf("%4.1lf - %6.0lf ", b->GetTemperature(), 1000*volt);
            b->EnableAcal(1, volt);
            b->StartDomino();
            Sleep(100);
            
            memset(av, 0, sizeof(av));

            for (j=0 ; j<100 ; j++) {
               for (i=0 ; i<10 ; i++)
                  b->IsBusy();
               b->SoftTrigger();
               while (b->IsBusy());
               b->StartDomino();
               b->TransferWaves(b->GetNumberOfChannels()*b->GetNumberOfChips());
               i = b->GetTriggerCell(0);
               b->GetWave(0, 0, swaveform, false, i, 1);

               for (k=0 ; k<1024 ; k++)
                  av[k] += swaveform[k];

               if (drs_kbhit())
                  break;
            }

            for (k=0 ; k<1024 ; k++)
               av[k] /= j;

            for (k=0 ; k<5 ; k++)
               printf("%10.2lf ", 1000*(av[k]/65536-0.5));
            printf("\n");

            if (drs_kbhit())
               break;
         }
         // keep chip "warm"
         b->StartDomino();
      }

      /* test0 */
      else if (match(param[0], "t0")) {
         b->Init();
         b->SetDominoMode(1);
         b->SetDominoActive(1);
         b->SetReadoutMode(1);
         b->SetFrequency(0.8, true);
         b->EnableTrigger(1, 0);
         b->SetTriggerLevel(1);
         b->SetChannelConfig(0, 8, 4);

         do {
            b->StartDomino();
            while (b->IsBusy())
               if (drs_kbhit())
                  break;

            b->TransferWaves();

            if (b->GetBoardType() == 5) {
               printf("%04d(0x%03X) - %3d\n", b->GetTriggerCell(0), b->GetTriggerCell(0), 
                  b->GetStopWSR(0));
            } else {
               printf("%04d %04d %04d %04d - %3d %3d %3d\n", 
                  b->GetTriggerCell(0),
                  b->GetTriggerCell(1),
                  b->GetTriggerCell(2),
                  b->GetTriggerCell(3),
                  b->GetTriggerCell(1)-b->GetTriggerCell(0),
                  b->GetTriggerCell(2)-b->GetTriggerCell(0),
                  b->GetTriggerCell(3)-b->GetTriggerCell(0));
            }
            Sleep(300);
         } while (!drs_kbhit());

         while (drs_kbhit())
            getch();
      }

      /* test1 simple start/stop loop */
      else if (match(param[0], "t1")) {
         time_t t1, t2;
         
         b->SetDebug(1);
         b->Init();
         b->SetFrequency(5, true);
         b->SetDominoMode(1);
         b->SetReadoutMode(0);
         b->SetTranspMode(0);
         b->SetDominoActive(1);
         b->EnableAcal(1, 0.5);
         b->EnableTcal(1);
         time(&t1);
         do {
            time(&t2);
         } while (t1 == t2);
         i=0;
         t1 = t2;
         do {
            b->StartDomino();
            b->SoftTrigger();
            b->TransferWaves();
            i++;
            time(&t2);
            if (t2 > t1) {
               printf("%d events/sec\n", i);
               i = 0;
               t1 = t2;
            }
         } while (!drs_kbhit());

         while (drs_kbhit())
            getch();
      }

      /* test2 readout from stop position */
      else if (match(param[0], "t2")) {
         short sw[1024];
         double volt = 0.5;

         b->Init();
         b->SetNumberOfChannels(10);
         b->SetChannelConfig(0, 9, 12);
         b->SetFrequency(2, true);
         b->EnableTcal(1);
         b->SetReadoutMode(0);
         b->SetDominoActive(0);
         b->SetDominoMode(1);
         b->SetCalibTiming(0, 0);
         b->StartDomino();
         b->EnableAcal(1, 0.5);
         if (!b->GetResponseCalibration()->IsRead(0))
            if (!b->GetResponseCalibration()->ReadCalibration(0))
               printf("cannot read calibration\n");

         do {
            //volt += 0.25;
            if (volt > 1)
               volt = 0;
            b->SoftTrigger();
            while (b->IsBusy());
            b->StartDomino();
            b->EnableAcal(1, volt);
            b->TransferWaves();

            b->GetWave(0, 1, sw, 0, 0);
            printf("%d ", sw[100]);
            b->GetWave(0, 1, sw, 1, 0);
            printf("%1.4lf\n", sw[100]/4096.0);
         } while (!drs_kbhit());
         while (drs_kbhit()) getch();
      }

      /* DAC Loop */
      else if (match(param[0], "t3")) {
         double volt;
         do {
            for (volt=2.5 ; volt > 0 ; volt -= 0.05) {
               
               printf("%4.1lf - %5.3lf\n", b->GetTemperature(), volt);
               b->SetDAC(0, volt);
               b->SetDAC(1, 2.5-volt);
               Sleep(100);
               if (drs_kbhit())
                  break;
            }
         } while (!drs_kbhit());

         while (drs_kbhit())
            getch();
      }

      /* noise measurement */
      else if (match(param[0], "t4")) {
         int i, n;
         short sw[1024];
         double ofs[1024], sx, sxx, avg, stdev, enob;

         b->Init();
         b->SetFrequency(2, true);
         b->EnableTcal(0);
         b->SetDominoMode(1);
         b->StartDomino();
         b->EnableAcal(1, 0.5);
         Sleep(100);
         b->SoftTrigger();
         while (b->IsBusy());
         b->StartDomino();
         Sleep(100);
         memset(ofs, 0, sizeof(ofs));

         for (i=0 ; i<10 ; i++) {
            b->SoftTrigger();
            while (b->IsBusy());
            b->StartDomino();
            b->TransferWaves(1);
            b->GetWave(0, 0, sw, 0, 0);
            sx = sxx = 0;
            for (n=0 ; n<1024 ; n++) {
               ofs[n] += sw[n];
            }
         }

         for (n=0 ; n<1024 ; n++)
            ofs[n] /= i;

         for (i=0 ; i<10 ; i++) {
            b->SoftTrigger();
            while (b->IsBusy());
            b->StartDomino();
            b->TransferWaves(1);
            b->GetWave(0, 0, sw, 0, 0);

            sx = sxx = 0;
            for (n=10 ; n<1014 ; n++) {
               sx += (sw[n]-ofs[n])/4096.0;
               sxx += (sw[n]-ofs[n])/4096.0*(sw[n]-ofs[n])/4096.0;
            }

            if (i>5)
               Sleep(5000);

            avg = sx / n;
            stdev = sqrt((sxx-sx*sx/n)/(n-1));
            enob = log(1/stdev)/log(2.);
            printf("avg=%1.4lf sd=%1.4lf ENOB=%1.1lf\n", avg, stdev, enob);
         };
      }

      /* exit/quit ---------- */
      else if (match(param[0], "exit") || match(param[0], "quit"))
         break;

      else {
         if (strchr(param[0], '\r'))
            *strchr(param[0], '\r') = 0;
         if (strchr(param[0], '\n'))
            *strchr(param[0], '\n') = 0;
         printf("Unknon command \"%s\"\n", param[0]);
      }

   } while (1);

   delete drs;
}

/*------------------------------------------------------------------*/

int main()
{
   printf("DRS command line tool, Revision %d\n", atoi(drscl_svn_revision+15));
   printf("Type 'help' for a list of available commands.\n\n");

   cmd_loop();
   return 1;
}
