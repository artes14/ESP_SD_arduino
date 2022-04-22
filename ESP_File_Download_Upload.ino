/*  Version 1
 *  
 *  ESP32/ESP8266 example of downloading and uploading a file from or to the ESP device's Filing System
 *  
 This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.
 
 Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
 3. You may not, except with my express written permission, distribute or commercially exploit the content.
 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

 The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
 software use is visible to an end-user.
 
 THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY 
 OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 See more at http://www.dsbird.org.uk
 *
*/
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

#ifdef ESP8266
  ESP8266WiFiMulti wifiMulti; 
  ESP8266WebServer server(80);
#else
  WiFiMulti wifiMulti;
  ESP32WebServer server(80);
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup(void){
  Serial.begin(115200);
  Serial.println("start!!!");
  delay(2000);
  // WIFI_MODE : "STA" or "AP"
  if(WIFI_MODE=="STA"){
    if (!WiFi.config(local_IP, gateway, subnet, dns)) { //WiFi.config(ip, gateway, subnet, dns1, dns2);
      Serial.println("WiFi STATION Failed to configure Correctly"); 
    } 
    wifiMulti.addAP(ssid_1, password_1);  // add Wi-Fi networks you want to connect to, it connects strongest to weakest
    wifiMulti.addAP(ssid_2, password_2);  // Adjust the values in the Network tab
    Serial.println("Connecting ...");
    // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    while (wifiMulti.run() != WL_CONNECTED) {  delay(250); Serial.print('.'); }

    Serial.println("\nConnected to "+WiFi.SSID()+" Use IP address: "+WiFi.localIP().toString()); // Report which SSID and IP is in use
    // The logical name http://fileserver.local will also access the device if you have 'Bonjour' running or your system supports multicast dns
    if (!MDNS.begin(servername)) {          // Set your preferred server name, if you use "myserver" the address would be http://myserver.local/
      Serial.println(F("Error setting up MDNS responder!")); 
      ESP.restart(); 
    } 
  }

  else if(WIFI_MODE=="AP"){
    Serial.println("setting WiFi as AP ...");
    WiFi.mode(WIFI_AP_STA); //need both to serve the webpage and take commands via tcp
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) { //WiFi.config(ip, gateway, subnet, dns1, dns2);
      Serial.println("WiFi STATION Failed to configure Correctly"); 
    } 
    Serial.println("Configuring WiFi ...");
    WiFi.softAP(ssid_ap, password_ap);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }
  
  
  #ifdef ESP32
    // Note: SD_Card readers on the ESP32 will NOT work unless there is a pull-up on MISO, either do this or wire one on (1K to 4K7)
    Serial.println(MISO);
    pinMode(19,INPUT_PULLUP);
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
  // Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise they do-not function.
  //----------------------------------------------------------------------   
  ///////////////////////////// Server Commands 
  server.on("/",         HomePage);
  server.on("/download", File_Download);
  server.on("/downloadf", File_Download_Folder);
  server.on("/upload",   File_Upload);
  server.on("/utest",   File_Upload_test);
  server.on("/umotor",   File_Upload_station);
  server.on("/uportable",   File_Upload_portable);
  server.on("/umobile",   File_Upload_mobile);
  server.on("/dtest",   File_Download_test);
  server.on("/dir",   File_Directory);
  server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);
  ///////////////////////////// End of Request commands
  server.begin();
  Serial.println("HTTP server started");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop(void){
  server.handleClient(); // Listen for client connections
}

// All supporting functions from here...
File UploadFile; 
File root;
char c='a';
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void HomePage(){
  SendHTML_Header();
  webpage += F("<a href='/download'><button>Download</button></a>");
  webpage += F("<a href='/downloadf'><button>Download Folder</button></a>");
  webpage += F("<a href='/upload'><br><button>Upload</button></a>");
  webpage += F("<a href='/utest'><button>UploadTest</button></a>");
  webpage += F("<a href='/umotor'><button>UploadMotor</button></a>");
  webpage += F("<a href='/uportable'><button>UploadPortable</button></a>");
  webpage += F("<a href='/umobile'><button>UploadMobile</button></a>");
  webpage += F("<a href='/dir'><br><button>Show Directory</button></a>");
  // webpage += F("<a href='/vid'><button>Vid</button></a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Directory(){
  Serial.println("\n========/DIRECTORY STATUS/========");
  root = SD.open("/");
  Serial.println(printDirectory(root, 0));
  HomePage();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download(){ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("download")) SD_file_download(server.arg(0));
  }
  else SelectInput("Enter filename to download","download","download");
}
void File_Download_Folder(){
  if (server.args()>0){
    if(server.hasArg("downloadf")){
      String foldername = server.arg(0); // foldername sent to server
      if(SD_present) {
        File folder = SD.open("/"+foldername); // open dir
        if(folder){ //if directory exists
          String filename = "";
          while(true){
            File entry =  folder.openNextFile();  // open next file in dir
            if(!entry) break;  // no next file in dir
            else Serial.println("sending filename : " + foldername + "/" + entry.name());
            if(entry.isDirectory()){
              continue;
            }
            else {
              SD_file_download("/" + foldername + "/" + entry.name());
            }
            entry.close();
            delay(100);
          }
        }
        folder.close();
      }
    }
  }
  else SelectInput("Enter file directory to download", "downloadf", "downloadf");
}

void File_Download_test(){ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  
  SD_file_download("images/momo3.jpg");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_download(String filename){
  if (SD_present) { 
    File download = SD.open("/"+filename);
    if (download) {
      Serial.println(download.name());
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename="+filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download"); 
  } else ReportSDNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Upload(){
  c='r'; //root
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer(); 
  server.send(200, "text/html",webpage);
}
void File_Upload_test(){
  c='t'; //test
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}
void File_Upload_station(){
  c='s'; //motor
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}
void File_Upload_portable(){
  c='p'; //portable
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}
void File_Upload_mobile(){
  c='m'; //mobile
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fuploadTest' id = 'fuploadTest' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void handleFileUpload(){ // upload a new file to the Filing system
  unsigned long time=millis();
  String dir="";
    if(c=='r') dir="/";
    else if(c=='t') dir="/test/";
    else if(c=='s') dir="/motor/";
    else if(c=='p') dir="/portable/";
    else if(c=='m') dir="/mobile/";
  HTTPUpload& uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if(uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = dir+filename;
    Serial.print("Upload File Name: "); Serial.println(filename);
    SD.remove(filename);                         // Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(filename, FILE_WRITE);  // Open the file for writing in SPIFFS (create it, if doesn't exist)
    filename = String();
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } 
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if(UploadFile)          // If the file was successfully created
    {                                    
      UploadFile.close();   // Close the file again
      unsigned long elapsed = millis()-time;
//      Serial.print("elapsed: ");Serial.println(millis()-time);
      //SD_file_download(uploadfile.filename);
      Serial.print("Upload Size: "); Serial.println(file_size(uploadfile.totalSize));
      Serial.print("Upload Speed: "); Serial.println(file_speed(uploadfile.totalSize, elapsed));
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>"); 
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename+"</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br>"; 
      append_page_footer();
      server.send(200,"text/html",webpage);
      root = SD.open(dir);
      Serial.println("\n========/DIRECTORY STATUS/======== " + dir);
      Serial.println(printDirectory(root, 0));
    } 
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
  // Serial.print("elapsed total: ");Serial.println(millis()-time);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Header(){
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
void SendHTML_Content(){
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop(){
  server.sendContent("");
  server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name){
  SendHTML_Header();
  webpage += F("<h3>"); webpage += heading1 + "</h3>"; 
  webpage += F("<FORM action='/"); webpage += command + "' method='get'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
  webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<a href='/'>[Back]</a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportSDNotPresent(){
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>"); 
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target){
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportCouldNotCreateFile(String target){
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String file_size(int bytes){
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
String file_speed(int bytes, unsigned long time){
  String fspeed = "";
  fspeed = String((double)bytes/1024.0/1024.0/(double)time*1000.0,3)+" MB/s";
  return fspeed;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String printDirectory(File dir, int numTabs) {
  String S="";
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) break;// no more files
    for (uint8_t i = 0; i < numTabs; i++) S += '\t';//Serial.print('\t');
    S += entry.name(); //Serial.print(entry.name());
    if (entry.isDirectory()) {
      S += "/\n"; //Serial.println("/");
      S += printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      S += "\t\t"; //Serial.print("\t\t");
      S += file_size(entry.size()); //Serial.println(file_size(entry.size()));
      S += "\n";
    }
    entry.close();
  }
  return S;
}
