#ifdef ESP8266
#include <ESP8266WiFi.h>       // Built-in
#include <ESP8266WiFiMulti.h>  // Built-in
#include <ESP8266WebServer.h>  // Built-in
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>              // Built-in
#include <WiFiMulti.h>         // Built-in
#include <ESP32WebServer.h>    // https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>
#include "FS.h"
#endif

#include "Network.h"
#include "Sys_Variables.h"
#include "CSS.h"
#include <SD.h>
#include <SPI.h>
#include <WiFiClient.h>

#ifdef ESP8266
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);
#else
WiFiMulti wifiMulti;
ESP32WebServer server(80);
#endif

//#define FTP_TIME_OUT  5      // Disconnect client after 5 minutes of inactivity
//#define FTP_CMD_SIZE 255 + 8 // max size of a command
//#define FTP_CWD_SIZE 255 + 8 // max size of a directory name
//#define FTP_FIL_SIZE 255     // max size of a file name//
//#define FTP_BUF_SIZE 1024 //512   // size of file buffer for read/write
//#define FTP_BUF_SIZE 2*1460

#define PIN_BTN 34
//#define PIN_SEND 35
// to turn on : LOW // to turn off : HIGH
#define PIN_LED_MODEB 33 //B
#define PIN_LED_MODEG 32 //G


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void LED_WIFImode(String md){
  if(md=="STA"){
    digitalWrite(PIN_LED_MODEB, HIGH);
    digitalWrite(PIN_LED_MODEG, LOW);
  }
  else if(md=="AP"){
    digitalWrite(PIN_LED_MODEB, LOW);
    digitalWrite(PIN_LED_MODEG, HIGH);
  }
}
void LED_init(){
    digitalWrite(PIN_LED_MODEB, HIGH);
    digitalWrite(PIN_LED_MODEG, HIGH);
}

void setup(void) {
  delay(2000);
  pinMode(PIN_BTN, INPUT);
//  pinMode(PIN_SEND, INPUT);
  pinMode(PIN_LED_MODEB, OUTPUT);
  pinMode(PIN_LED_MODEG, OUTPUT);
  LED_init();
  Serial.begin(500000);
  Serial.println("\nServer rebooting... initializing");

#ifdef ESP32
  // Note: SD_Card readers on the ESP32 will NOT work unless there is a pull-up on MISO, either do this or wire one on (1K to 4K7)
  pinMode(19, INPUT_PULLUP);
#endif
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS_pin)) { // see if the card is present and can be initialised. Wemos SD-Card CS uses D8
    Serial.println(F("Card failed or not present, no SD Card data logging possible..."));
    SD_present = false;
  }
  else
  {
    Serial.println(F("Card initialised... file access enabled..."));
    SD_present = true;
  }

  File modefile = SD.open("/mode.txt");
  String md = "";
  while(modefile.available()) md += (char)modefile.read();
  if(md.length()>1)WIFI_MODE=md;
  Serial.print("configuring WIFI mode to : "); Serial.println(WIFI_MODE);

  // WIFI_MODE : "STA" or "AP"
  if (WIFI_MODE == "STA") {
    if (!WiFi.config(local_IP, gateway, subnet, dns)) { //WiFi.config(ip, gateway, subnet, dns1, dns2);
      Serial.println("WiFi STATION Failed to configure Correctly");
    }
    String ssid     = "";
    String password = "";
    md ="";
    File sta_info = SD.open("/STA_INFO.txt");
    while(sta_info.available()) md += (char)sta_info.read();
    if(md.length()>1){
      int index = md.indexOf(',');
      int len = md.length();
      ssid=md.substring(0, index);
      password=md.substring(index+1, len);
      Serial.println("configure WIFI from file");
    }
    else{
      ssid=ssid_1;
      password=password_1;
    }
    wifiMulti.addAP(ssid.c_str(), password.c_str());
//    wifiMulti.addAP(ssid_1, password_1);  // add Wi-Fi networks you want to connect to, it connects strongest to weakest
//    wifiMulti.addAP(ssid_2, password_2);  // Adjust the values in the Network tab
    Serial.println("Connecting ...");
    // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    while (wifiMulti.run() != WL_CONNECTED) {
      delay(250);
      Serial.print('.');
    }

    Serial.println("Connected to " + WiFi.SSID() + " Use IP address: " + WiFi.localIP().toString()); // Report which SSID and IP is in use
    // The logical name http://fileserver.local will also access the device if you have 'Bonjour' running or your system supports multicast dns
    if (!MDNS.begin(servername)) {          // Set your preferred server name, if you use "myserver" the address would be http://myserver.local/
      Serial.println(F("Error setting up MDNS responder!"));
      ESP.restart();
    }
  }

  else if (WIFI_MODE == "AP") {
    Serial.println("setting WiFi as AP ...");
    WiFi.mode(WIFI_AP_STA); //need both to serve the webpage and take commands via tcp
    WiFi.softAP(ssid_ap, password_ap);
    delay(100);
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) { //WiFi.config(ip, gateway, subnet, dns1, dns2);
      Serial.println("WiFi STATION Failed to configure Correctly");
    }
    Serial.println("Configuring WiFi ...");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }



  // Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise they do-not function.
  //----------------------------------------------------------------------
  ///////////////////////////// Server Commands
  server.on("/",         HomePage);
  server.on("/download", File_Download);
  //  server.on("/downloadf", File_Download_Folder);
  server.on("/upload",   File_Upload);
  server.on("/utest",   File_Upload_test);
  server.on("/umotor",   File_Upload_station);
  server.on("/uportable",   File_Upload_portable);
  server.on("/umobile",   File_Upload_mobile);
  server.on("/dtest",   File_Download_test);
  server.on("/dir",   File_Directory);
  server.on("/delete",   File_Delete);
  server.on("/fupload",  HTTP_POST, fupload_do , handleFileUpload);
  ///////////////////////////// End of Request commands
  server.begin();
  LED_WIFImode(WIFI_MODE);
//  LED_init();
  Serial.println("HTTP server started");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop(void) {
  server.handleClient(); // Listen for client connections
  //
  //  WiFiClient cl=server.available();
  //  if(cl) Serial.println("")
  if (digitalRead(PIN_BTN)==HIGH) {
    File modefile = SD.open("/mode.txt");
    if (modefile) {
      String mod = "";
      while (modefile.available())mod += (char)modefile.read();
      Serial.print("current mode is : "); Serial.println(mod);
      if (mod == "STA")mod = "AP";
      else mod = "STA";
      Serial.print("changing mode to : "); Serial.println(mod);
      modefile.close();
      File newmode = SD.open("/mode.txt", FILE_WRITE);
      newmode.print(mod);
      newmode.close();
      delay(1000);
      ESP.restart();
    }
    else Serial.println("no modefile");
  }

}

// All supporting functions from here...
File UploadFile;
char c = 'a';
String dir = "";
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void fupload_do() {
  server.send(200);
}
void HomePage() {
  SendHTML_Header();
  webpage += F("<a href='/download'><button>Download</button></a>");
  //  webpage += F("<a href='/downloadf'><button>Download Folder</button></a>");
  webpage += F("<a href='/upload'><br><button>Upload</button></a>");
  //  webpage += F("<a href='/utest'><button>UploadTest</button></a>");
  webpage += F("<a href='/umotor'><button>UploadMotor</button></a>");
  webpage += F("<a href='/uportable'><button>UploadPortable</button></a>");
  webpage += F("<a href='/umobile'><button>UploadMobile</button></a>");
  webpage += F("<a href='/dir'><br><button>Show Directory</button></a>");
  webpage += F("<a href='/delete'><br><button>Delete File</button></a>");
  // webpage += F("<a href='/vid'><button>Vid</button></a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Directory() {
  Serial.println("\n========/DIRECTORY STATUS/========");
  File root = SD.open("/");
  //  Serial.println(printDirectory(root, 0));
  printDirectory(root, 0);
  root.close();
  HomePage();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("download")) SD_file_download(server.arg(0));
  }
  else SelectInput("Enter filename to download", "download", "download");
}

void File_Download_test() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments

  SD_file_download("images/momo3.jpg");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_download(String filename) {
  if (SD_present) {
    File download = SD.open("/" + filename);
    if (download) {
      int fsize = download.size();
      Serial.println(download.name());
      Serial.print("fsize : "); Serial.println(fsize);
      server.sendHeader("Content-Length", (String)(fsize));
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
      //      SD.remove("/"+filename);
      //      if(SD.exists("/"+filename)) Serial.println("file not deleted : " + filename);
      //      else Serial.println("file deleted : " + filename);
      //      File_Directory();
    } else ReportFileNotPresent("download");
  } else ReportSDNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Upload() {
  c = 'r'; //root
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>");
  webpage += F("<input type='text' name='up' value='motor'>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br></form>");
  // webpage += F("<progress id='progressBar' value='0' max='100' style='width:300px;'></progress><br>");
  // webpage += F("<h3 id='status'></h3><p id='loaded_n_total'></p>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
void File_Upload_test() {
  c = 't'; //test
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
void File_Upload_station() {
  c = 's'; //motor
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
void File_Upload_portable() {
  c = 'p'; //portable
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
void File_Upload_mobile() {
  c = 'm'; //mobile
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint32_t tt = 0;
void handleFileUpload() { // upload a new file to the Filing system

  String dir = "";
  if (c == 'r') dir = "/";
  else if (c == 't') dir = "/test/";
  else if (c == 's') dir = "/motor/";
  else if (c == 'p') dir = "/portable/";
  else if (c == 'm') dir = "/mobile/";
  HTTPUpload& uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/src

  // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if (uploadfile.status == UPLOAD_FILE_START)
  {

    tt = millis();
    String filename = uploadfile.filename;
    String fullname = dir + filename;
    Serial.print("Upload File Name: "); Serial.println(fullname);
    if (!SD.exists(fullname)) {
      if (!SD.exists(dir + "filename.txt")) {
        Serial.println("file list does not exist");
      }
      File filename = SD.open(dir + "filename.txt", FILE_APPEND);
      String fname = uploadfile.filename;
      if (filename) {
        Serial.println("file list exists... saving " + fname);
        filename.println(fname);
      }
      filename.close();
    }
    SD.remove(fullname);                         // Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(fullname, FILE_WRITE);  // Open the file for writing in SPIFFS (create it, if doesn't exist)
    filename = String();
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if (UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  }
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if (UploadFile)         // If the file was successfully created
    {
      UploadFile.close();   // Close the file again
      uint32_t elapsed = millis() - tt;
      //      Serial.print("elapsed: ");Serial.println(millis()-time);
      //SD_file_download(uploadfile.filename);
      Serial.print("Upload Size: "); Serial.println(file_size(uploadfile.totalSize));
      Serial.print("Upload Speed: "); Serial.println(file_speed(uploadfile.totalSize, elapsed));
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>");
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename + "</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br>";
      append_page_footer();
      server.send(200, "text/html", webpage);
      dir.remove(dir.length() - 1);
      File root = SD.open(dir);
      Serial.println("\n========/DIRECTORY STATUS/======== " + dir);
      //      Serial.println(printDirectory(root, 0));
      printDirectory(root, 0);
      root.close();

    }
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
  // Serial.print("elapsed total: ");Serial.println(millis()-time);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void File_Delete() {
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("delete")) SD_file_delete(server.arg(0));
  }
  else SelectInput("Enter filename to delete", "delete", "delete");
}
void SD_file_delete(String filename) {
  if (SD_present) {
    File deletefile = SD.open("/" + filename);
    if (deletefile) {
      int fsize = deletefile.size();
      Serial.println(deletefile.name());
      Serial.print("deleted : "); Serial.println(fsize);
      deletefile.close();
      SD.remove("/" + filename);
    }
  } else ReportSDNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Header() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content() {
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop() {
  server.sendContent("");
  server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name) {
  SendHTML_Header();
  webpage += F("<h3>"); webpage += heading1 + "</h3>";
  webpage += F("<FORM action='/"); webpage += command + "' method='get'>";
  webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<a href='/'>[Back]</a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportSDNotPresent() {
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target) {
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>");
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportCouldNotCreateFile(String target) {
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>");
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String file_size(int bytes) {
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes) + " B";
  else if (bytes < (1024 * 1024))      fsize = String(bytes / 1024.0, 3) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
  else                              fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
  return fsize;
}
String file_speed(int bytes, uint32_t t) {
  Serial.println(t);
  String fspeed = "";
  fspeed = String((double)bytes / 1024.0 / (double)t * 1000.0, 3) + " KB/s";
  return fspeed;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) break;// no more files
    for (uint8_t i = 0; i < numTabs; i++) Serial.print('\t');//S += '\t';
    //    S += entry.name();
    String S = entry.name();
    S = S.substring(S.lastIndexOf('/') + 1, S.length());

    Serial.print(S);
    if (entry.isDirectory()) {
      Serial.println("/");//S += "/\n";
      //      S += printDirectory(entry, numTabs + 1);
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");//S += "\t\t";
      Serial.println(file_size(entry.size())); //S += file_size(entry.size());
      //      S += "\n";
    }
    entry.close();
  }
  //  return S;
}
