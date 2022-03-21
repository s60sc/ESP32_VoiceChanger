
/* 
  Management and storage of application configuration state.
  Configuration file stored on spiffs, except passwords which are stored in NVS
   
  Workflow:
  loadConfig:
    file -> loadConfigMap+loadKeyVal -> map -> getNextKeyVal+updatestatus+updateAppStatus -> vars 
                                               retrieveConfigMap (as required)
  statusHandler:
    map -> buildJsonString+buildAppJsonString -> browser 
  controlHandler: 
    browser -> updateStatus+updateAppStatus -> vars -> updateConfigMap -> map -> saveConfigMap -> file 

  s60sc 2022
*/

# include "myConfig.h"

static fs::FS fp = STORAGE;
static std::map<std::string, std::string> configs;
static Preferences prefs; 
char* jsonBuff = NULL;

static bool updateConfigMap(const char* variable, const char* value);

/********************* voiceChanger specific function **********************/

static esp_err_t updateAppStatus(const char* variable, const char* value) {
  // update vars from browser input
  int intVal = atoi(value);
  float fltVal = atof(value);
  if (!strcmp(variable, "RM")) RING_MOD = (bool)intVal;
  else if (!strcmp(variable, "BP")) BAND_PASS = (bool)intVal;
  else if (!strcmp(variable, "HP")) HIGH_PASS = (bool)intVal;
  else if (!strcmp(variable, "LP")) LOW_PASS = (bool)intVal;
  else if (!strcmp(variable, "HS")) HIGH_SHELF = (bool)intVal;
  else if (!strcmp(variable, "LS")) LOW_SHELF = (bool)intVal;
  else if (!strcmp(variable, "PK")) PEAK = (bool)intVal;
  else if (!strcmp(variable, "CP")) CLIPPING = (bool)intVal;
  else if (!strcmp(variable, "RV")) REVERB = (bool)intVal;
  // integer
  else if (!strcmp(variable, "BPcas")) BP_CAS = intVal;   
  else if (!strcmp(variable, "HPcas")) HP_CAS = intVal;  
  else if (!strcmp(variable, "LPcas")) LP_CAS = intVal;                
  else if (!strcmp(variable, "SineFreq")) SW_FREQ = intVal;  
  else if (!strcmp(variable, "SineAmp")) SW_AMP = intVal << 5;     
  else if (!strcmp(variable, "ClipFac")) CLIP_FACTOR = intVal;  
  else if (!strcmp(variable, "DecayFac")) DECAY_FACTOR = intVal;        
  else if (!strcmp(variable, "PreAmp")) PREAMP_GAIN = intVal;  
  else if (!strcmp(variable, "AmpVol")) AMP_VOL = intVal; 
  else if (!strcmp(variable, "Bright")) BRIGHTNESS = intVal;      
  else if (!strcmp(variable, "Srate")) SAMPLE_RATE = intVal; 
  else if (!strcmp(variable, "mType")) MIC_TYPE = (deviceType)intVal;
  else if (!strcmp(variable, "aType")) AMP_TYPE = (deviceType)intVal;
  
  // binary integer
  else if (!strcmp(variable, "MicChan")) {
    // set I2S port for microphone, amp is opposite
    if (intVal) {
      I2S_MIC_PORT = I2S_NUM_0;
      I2S_AMP_PORT = I2S_NUM_1;
    } else {
      I2S_MIC_PORT = I2S_NUM_1;
      I2S_AMP_PORT = I2S_NUM_0;
    }
  }
  
  // float
  else if (!strcmp(variable, "BPqval")) BP_Q = fltVal;
  else if (!strcmp(variable, "HPqval")) HP_Q = fltVal;
  else if (!strcmp(variable, "LPqval")) LP_Q = fltVal;
  else if (!strcmp(variable, "PKqval")) PK_Q = fltVal;

  // bool
  else if (!strcmp(variable, "Disable")) DISABLE = (bool)intVal;
  else if (!strcmp(variable, "VolPot")) USE_POT = (bool)intVal;

  // float with no decimal places
  else if (!strcmp(variable, "BPfreq")) BP_FREQ = fltVal; 
  else if (!strcmp(variable, "HPfreq")) HP_FREQ = fltVal;
  else if (!strcmp(variable, "LPfreq")) LP_FREQ = fltVal;  
  else if (!strcmp(variable, "HSfreq")) HS_FREQ = fltVal; 
  else if (!strcmp(variable, "HSgain")) HS_GAIN = fltVal;
  else if (!strcmp(variable, "LSfreq")) LS_FREQ = fltVal;
  else if (!strcmp(variable, "LSgain")) LS_GAIN = fltVal;
  else if (!strcmp(variable, "PKfreq")) PK_FREQ = fltVal;
  else if (!strcmp(variable, "PKgain")) PK_GAIN = fltVal;
  return ESP_OK; 
}

static void buildAppJsonString(bool quick) {
  // build app specific part of json string
  char* p = jsonBuff + 1;
  *p = 0;
}

/********************* generic Config functions ****************************/

static bool getNextKeyVal(char* keyName, char* keyVal) {
  // return next key and value from config map on each call
  static std::map<std::string, std::string>::iterator it = configs.begin();
  if (it != configs.end()) {
    strcpy(keyName, it->first.c_str());
    strcpy(keyVal, it->second.c_str()); 
    it++;
    return true;
  }
  // end of map reached
  it = configs.begin();
  return false;
}

static bool updateConfigMap(const char* variable, const char* value) {
  std::string thisKey(variable);
  std::string thisVal(value);
  if (configs.find(thisKey) != configs.end()) {
    configs[thisKey] = thisVal; 
    return true;
  } 
  LOG_DBG("Key %s not found", variable);
  return false; 
}

static bool retrieveConfigMap(const char* variable, char* value) {
  std::string thisKey(variable);
  std::string thisVal(value);
  if (configs.find(thisKey) != configs.end()) {
    strcpy(value, configs[thisKey].c_str()); 
    return true;
  } 
  LOG_WRN("Key %s not found", variable);
  return false; 
}

static void loadKeyVal(const std::string keyValPair) {
  // extract a key value pair from input and load into configs map
  if (keyValPair.length()) {
    size_t colon = keyValPair.find(':');
    if (colon != std::string::npos) {
      configs[keyValPair.substr(0, colon)] =
        keyValPair.substr(colon + 1, keyValPair.length()); 
    } else LOG_ERR("Unable to parse <%s>, len %u", keyValPair.c_str(), keyValPair.length());
  }
}

static void saveConfigMap() {
  File file = fp.open(CONFIG_FILE_PATH, FILE_WRITE);
  if (!file) LOG_ERR("Failed to save to configs file");
  else {
    for (const auto& elem : configs) {
      file.write((uint8_t*)elem.first.c_str(), elem.first.length()); 
      file.write((uint8_t*)":", 1);
      file.write((uint8_t*)elem.second.c_str(), elem.second.length());
      file.write((uint8_t*)"\n", 1);
    }
    file.close();
    LOG_INF("Config file saved");
  }
}

static void loadConfigMap() {
  File file = fp.open(CONFIG_FILE_PATH, FILE_READ);
  if (!file) LOG_ERR("Failed to load file %s", CONFIG_FILE_PATH);
  else {
    configs.erase(configs.begin(), configs.end());
    esp_err_t res = ESP_OK; 
    std::string keyValPair;
    // populate configs map from configs file
    while (true) {
      String kvPairStr = file.readStringUntil('\n');
      if (!ALLOW_SPACES) keyValPair = std::regex_replace(kvPairStr.c_str(), std::regex("\\s+"), "");
      if (!keyValPair.length()) break;
      loadKeyVal(keyValPair);
    } 
    file.close();
  }
}

static bool savePrefs(bool bClear = false) {
  // use preferences for passwords
  if (!prefs.begin(APP_NAME, false)) {  
    LOG_ERR("Failed to save preferences");
    return false;
  }
  if (bClear) { 
    prefs.clear(); 
    LOG_ERR("Cleared preferences");
    return true;
  }
  prefs.putString("st_ssid", ST_SSID);
  prefs.putString("st_pass", ST_Pass);
  prefs.putString("ap_pass", AP_Pass); 
#ifdef INCLUDE_FTP
  prefs.putString("ftp_pass", ftp_pass);
  prefs.putString("smtp_pass", smtp_pass);
#endif
  prefs.end();
  return true;
}

static bool loadPrefs() {
  // use preferences for passwords
  if (!prefs.begin(APP_NAME, false)) {  
    savePrefs(); // if prefs do not yet exist
    return false;
  }
  prefs.getString("st_ssid", ST_SSID, 32);
  prefs.getString("st_pass", ST_Pass, MAX_PWD_LEN);
  prefs.getString("ap_pass", AP_Pass, MAX_PWD_LEN); 
#ifdef INCLUDE_FTP
  prefs.getString("ftp_pass", ftp_pass, MAX_PWD_LEN);
#endif
#ifdef INCLUDE_SMTP
  prefs.getString("smtp_pass", smtp_pass, MAX_PWD_LEN);
#endif
  prefs.end();
  return true;
}

bool updateStatus(const char* variable, const char* value) {
  // called from controlHandler() to update app status from changes made on browser
  // or from loadConfig() to update app status from stored preferences
  int intVal = atoi(value);
  int fltVal = atof(value);
  updateConfigMap(variable, value); // update config map
  updateAppStatus(variable, value); 
  if(!strcmp(variable, "hostName")) strcpy(hostName, value);
  else if(!strcmp(variable, "ST_SSID")) strcpy(ST_SSID, value);
  else if(!strcmp(variable, "ST_Pass")) strcpy(ST_Pass, value);
#ifdef INCLUDE_FTP
  else if(!strcmp(variable, "ftp_server")) strcpy(ftp_server, value);
  else if(!strcmp(variable, "ftp_port")) ftp_port = intVal;
  else if(!strcmp(variable, "ftp_user")) strcpy(ftp_user, value);
  else if(!strcmp(variable, "ftp_pass")) strcpy(ftp_pass, value);
  else if(!strcmp(variable, "ftp_wd")) strcpy(ftp_wd, value);
#endif
#ifdef INCLUDE_SMTP
  else if(!strcmp(variable, "smtp_port")) smtp_port = intVal;
  else if(!strcmp(variable, "smtp_login")) strcpy(smtp_login, value);
  else if(!strcmp(variable, "smtp_server")) strcpy(smtp_server, value);
  else if(!strcmp(variable, "smtp_email")) strcpy(smtp_email, value);
  else if(!strcmp(variable, "smtp_pass")) strcpy(smtp_pass, value);
#endif
  else if(!strcmp(variable, "dbgVerbose")) {
    dbgVerbose = (intVal) ? true : false;
    Serial.setDebugOutput(dbgVerbose);
  } 
  else if(!strcmp(variable, "logMode")) {  
    logMode = intVal; 
    if (!(logMode == 2 && WiFi.status() != WL_CONNECTED)) remote_log_init();
    else logMode = 0;
  }
  else if(!strcmp(variable, "resetLog")) reset_log(); 
  else if(!strcmp(variable, "reset")) return false;
  else if(!strcmp(variable, "clear")) savePrefs(true);
  else if(!strcmp(variable, "deldata")) {  
    // /control?deldata=1
    if ((fs::SPIFFSFS*)&STORAGE == &SPIFFS) startSpiffs(true);
    else deleteFolderOrFile(DATA_DIR);
    return false;
  }
  else if(!strcmp(variable, "save")) {
    saveConfigMap();
    savePrefs();
  } 
  return true;
}

void buildJsonString(bool quick) {
  // called from statusHandler() to build json string with current status to return to browser 
  char* p = jsonBuff;
  *p++ = '{';
  buildAppJsonString(quick);
  p += strlen(jsonBuff) - 1;
  if (!quick) {
    // populate first part of json string from config map
    for (const auto& elem : configs) 
      p += sprintf(p, "\"%s\":\"%s\",", elem.first.c_str(), elem.second.c_str());
    // stored on NVS 
    p += sprintf(p, "\"ST_Pass\":\"%.*s\",",strlen(ST_Pass), FILLSTAR);
    p += sprintf(p, "\"AP_Pass\":\"%.*s\",",strlen(AP_Pass), FILLSTAR);
#ifdef INCLUDE_FTP 
    p += sprintf(p, "\"ftp_pass\":\"%.*s\",",strlen(ftp_pass), FILLSTAR);
#endif
#ifdef INCLUDE_SMTP
    p += sprintf(p, "\"smtp_pass\":\"%.*s\",",strlen(smtp_pass), FILLSTAR);
#endif
    // other
    p += sprintf(p, "\"fw_version\":\"%s\",", APP_VER); 
  }
  *p--;
  *p++ = '}'; // overwrite final comma
  *p = 0;
}

bool loadConfig() {
  // called on startup
  LOG_INF("Load config");
  if (jsonBuff == NULL) jsonBuff = (char*)ps_malloc(JSON_BUFF_LEN); 
  loadPrefs();
  loadConfigMap();

  // set default hostname if config is null
  AP_SSID.toUpperCase();
  retrieveConfigMap("hostName", hostName);
  if (!strcmp(hostName, "null") || !strlen(hostName)) {
    strcpy(hostName, AP_SSID.c_str());
    updateStatus("hostName", hostName);
  }

  // ST_SSID value priority: configs > prefs > hardcoded
  retrieveConfigMap("ST_SSID", jsonBuff);
  if (!strcmp(jsonBuff, "null")) {
    // not defined in configs, use retrieved prefs 
    if (strlen(ST_SSID)) {
      updateConfigMap("ST_SSID", ST_SSID);
    }
    else {
      // not in prefs, use hardcoded values
      updateConfigMap("ST_SSID", ST_SSID);
      updateConfigMap("ST_ip", ST_ip);
      updateConfigMap("ST_gw", ST_gw);
      updateConfigMap("ST_sn", ST_sn);
      updateConfigMap("ST_ns1", ST_ns1);
      updateConfigMap("ST_ns1", ST_ns2);
    }
  }

  // load variables from stored config map
  char variable[32] = {0,};
  char value[FILE_NAME_LEN] = {0,};
  while(getNextKeyVal(variable, value)) updateStatus(variable, value);
  if (strlen(ST_SSID)) LOG_INF("Using ssid: %s%s %s", ST_SSID, !strlen(ST_ip) ? " " : " with static ip ", ST_ip);
  return true;
}
