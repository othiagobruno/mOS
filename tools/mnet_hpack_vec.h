/* RFC 7541 Apêndice C.3-C.6 */
struct hv { int sec, n; const char *hex; int nh; const char *h[12][2]; };
static const struct hv hvec[] = {
  {3,1,"828684410f7777772e6578616d706c652e636f6d",4,{{":method","GET"},{":scheme","http"},{":path","/"},{":authority","www.example.com"}}},
  {3,2,"828684be58086e6f2d6361636865",5,{{":method","GET"},{":scheme","http"},{":path","/"},{":authority","www.example.com"},{"cache-control","no-cache"}}},
  {3,3,"828785bf400a637573746f6d2d6b65790c637573746f6d2d76616c7565",5,{{":method","GET"},{":scheme","https"},{":path","/index.html"},{":authority","www.example.com"},{"custom-key","custom-value"}}},
  {4,1,"828684418cf1e3c2e5f23a6ba0ab90f4ff",4,{{":method","GET"},{":scheme","http"},{":path","/"},{":authority","www.example.com"}}},
  {4,2,"828684be5886a8eb10649cbf",5,{{":method","GET"},{":scheme","http"},{":path","/"},{":authority","www.example.com"},{"cache-control","no-cache"}}},
  {4,3,"828785bf408825a849e95ba97d7f8925a849e95bb8e8b4bf",5,{{":method","GET"},{":scheme","https"},{":path","/index.html"},{":authority","www.example.com"},{"custom-key","custom-value"}}},
  {5,1,"4803333032580770726976617465611d4d6f6e2c203231204f637420323031332032303a31333a323120474d546e1768747470733a2f2f7777772e6578616d706c652e636f6d",4,{{":status","302"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:21 GMT"},{"location","https://www.example.com"}}},
  {5,2,"4803333037c1c0bf",4,{{":status","307"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:21 GMT"},{"location","https://www.example.com"}}},
  {5,3,"88c1611d4d6f6e2c203231204f637420323031332032303a31333a323220474d54c05a04677a69707738666f6f3d4153444a4b48514b425a584f5157454f50495541585157454f49553b206d61782d6167653d333630303b2076657273696f6e3d31",6,{{":status","200"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:22 GMT"},{"location","https://www.example.com"},{"content-encoding","gzip"},{"set-cookie","foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"}}},
  {6,1,"488264025885aec3771a4b6196d07abe941054d444a8200595040b8166e082a62d1bff6e919d29ad171863c78f0b97c8e9ae82ae43d3",4,{{":status","302"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:21 GMT"},{"location","https://www.example.com"}}},
  {6,2,"4883640effc1c0bf",4,{{":status","307"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:21 GMT"},{"location","https://www.example.com"}}},
  {6,3,"88c16196d07abe941054d444a8200595040b8166e084a62d1bffc05a839bd9ab77ad94e7821dd7f2e6c7b335dfdfcd5b3960d5af27087f3672c1ab270fb5291f9587316065c003ed4ee5b1063d5007",6,{{":status","200"},{"cache-control","private"},{"date","Mon, 21 Oct 2013 20:13:22 GMT"},{"location","https://www.example.com"},{"content-encoding","gzip"},{"set-cookie","foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"}}},
};
