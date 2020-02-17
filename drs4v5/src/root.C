{
TTree *t = new TTree("t", "t");
t->ReadFile("du_u.txt", "E:Ch:Cell:U:dU");
t->Draw("U:dU");
}
