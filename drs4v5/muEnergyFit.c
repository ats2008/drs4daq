
Double_t langaufun(Double_t *x, Double_t *par) 
{


  // Numeric constants
  Double_t invsq2pi = 0.3989422804014;   // (2 pi)^(-1/2)
  Double_t mpshift  = -0.22278298;       // Landau maximum location
  
  // Control constants
  Double_t np = 100.0;      // number of convolution steps
  Double_t sc =   5.0;      // convolution extends to +-sc Gaussian sigmas
  
  // Variables
  Double_t xx;
  Double_t mpc;
  Double_t fland;
  Double_t sum = 0.0;
  Double_t xlow,xupp;
  Double_t step;
  Double_t i;
  
  
  // MP shift correction
  mpc = par[1] - mpshift * par[0]; 
  
  // Range of convolution integral
  xlow = x[0] - sc * par[3];
  xupp = x[0] + sc * par[3];
  
  step = (xupp-xlow) / np;
  
  // Convolution integral of Landau and Gaussian by sum
  for(i=1.0; i<=np/2; i++) {
    xx = xlow + (i-.5) * step;
    fland = TMath::Landau(xx,mpc,par[0]) / par[0];
    sum += fland * TMath::Gaus(x[0],xx,par[3]);
    
    xx = xupp - (i-.5) * step;
    fland = TMath::Landau(xx,mpc,par[0]) / par[0];
    sum += fland * TMath::Gaus(x[0],xx,par[3]);
  }
  
  return (par[2] * step * sum * invsq2pi / par[3]);
}

Double_t gausX(Double_t* x, Double_t* par){
  return par[0]*(TMath::Gaus(x[0], par[1], par[2], kTRUE));
}
Double_t totalfunc(Double_t* x, Double_t* par){
  return gausX(x, par) + langaufun(x, &par[3]);
}

int fitit(TString fname)
{
    gStyle->SetOptStat(11);
    gStyle->SetOptFit(1111);
    TFile * f= new TFile(fname,"read");
    TF1* fity = new TF1("fitey", totalfunc, -0.1, 256, 7);
    TH1D* qADC = (TH1D*) f->Get("qADC");; 
    TFitResultPtr FitRsltPtr(0);
    
      double  pary[7];
    pary[0]=0.5; // Gausian_Mean
    pary[1]=3.0; // Gaus_Sigma
    pary[2]=pary[5]*0.05;
    pary[3]=20.0;// Landau_Width
    pary[4]=qADC->GetMean();
    pary[5]=qADC->Integral();
    pary[6]=8;// Landau_Gausian_Width
    
    fity->SetParameters(pary);
    FitRsltPtr = qADC->Fit(fity, "R");

    delete fity;
    
    return 0;
}

int fitit2(TString fname)
{    
        gStyle->SetOptStat(11);
        gStyle->SetOptFit(1111);
    TFile * f= new TFile(fname,"read");
    TF1* fity = new TF1("fitey", langaufun, 5, 258, 4);
    TH1D* qADC =(TH1D*) f->Get("qADC"); 
    
     double  pary[7];
    pary[0]=0.5; // Gausian_Mean
    pary[1]=3.0; // Gaus_Sigma
    pary[2]=pary[5]*0.05;
    pary[3]=2.0;// Landau_Width
    pary[4]=qADC->GetMean();
    pary[5]=qADC->Integral();
    pary[6]=2;// Landau_Gausian_Width


    TFitResultPtr FitRsltPtr(0);
    
    fity->SetParameters(&pary[3]);
    FitRsltPtr = qADC->Fit(fity, "R");

    delete fity;
    
    return 0;
}


