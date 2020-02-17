{
TTree *t = new TTree("t", "t");
t->ReadFile("data.txt", "i:U");
t->Draw("i:U");
}
