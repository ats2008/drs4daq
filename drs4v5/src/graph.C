#include <TGraph.h>
#include <TCanvas.h>
#include <Getline.h>

void graph() {
   double x[10], y[10];

   TCanvas *c1 = new TCanvas();
   
   for (int n=0 ; n<5 ; n++) {
       for (int i=0 ; i<10 ; i++)
          x[i] = y[i] = i+n;
      
       TGraph *g = new TGraph(10, x, y);
       g->Draw("ACP");
       c1->Update();
       gPad->WaitPrimitive();
       delete g;
   }   
}