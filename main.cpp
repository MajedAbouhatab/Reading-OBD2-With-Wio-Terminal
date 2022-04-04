#include "rpcBLEDevice.h"
#include "TFT_eSPI.h"

TFT_eSPI tft = TFT_eSPI();

static BLEAdvertisedDevice *OBD2Device;
static BLERemoteService *ThisService;
static BLERemoteCharacteristic *NotifyCharacteristic, *WriteCharacteristic;
static String OldString, NewString;
static int ItemsInArray = 5, I = ItemsInArray, OldScreen, NewScreen;
static char *LabelArray[] = {"Batt", "RPM ", "Velo", "ATmp", "ETmp"};
static char *CommandArray[] = {"AT RV\r", "01 0C 1\r", "01 0D 1\r", "01 0F 1\r", "01 05 1\r"};

static void NotifyCB(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  NewString = (char *)pData;
  NewString = NewString.substring(0, max(NewString.indexOf(">"), 0));
  NewString.trim();
  if (!NewString.length() || I == ItemsInArray)
    return;
  else
  {
    if (NewString.substring(0, 2) == "41")
    {
      unsigned long UnsignedLong = strtoul(NewString.substring(3, 5).c_str(), NULL, 16);
      NewString = NewString.substring(6, NewString.length());
      NewString.replace(" ", "");
      switch (UnsignedLong)
      {
      case 0x0C:
        UnsignedLong = strtoul(NewString.c_str(), NULL, 16) / 4;
        break;
      case 0x0D:
        UnsignedLong = strtoul(NewString.c_str(), NULL, 16) * 0.621371;
        break;
      case 0x05:
      case 0x0F:
        UnsignedLong = (strtoul(NewString.c_str(), NULL, 16) - 40) * 9 / 5 + 32;
        break;
      default:
        UnsignedLong = strtoul(NewString.c_str(), NULL, 16);
        break;
      }
      NewString = String(UnsignedLong);
    }
    if (NewScreen == ItemsInArray)
    {
      tft.setTextSize(3);
      tft.fillRect(0, tft.getCursorY() + 40, 240, 40, TFT_BLACK);
      tft.setCursor(10, tft.getCursorY() + 40);
      tft.setTextColor(TFT_GREEN);
      tft.print(LabelArray[I]);
      tft.setTextColor(TFT_RED);
      tft.printf(" %s", NewString.c_str());
    }
    else
    {
      tft.setTextSize(7);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_GREEN);
      tft.print(LabelArray[I]);
      tft.fillRect(0, 100, 240, 50, TFT_BLACK);
      tft.setCursor(10, 100);
      tft.setTextColor(TFT_RED);
      tft.printf("%s", NewString.c_str());
      OldString = NewString;
    }
    delay(10);
  }
}

class AdvertisedDeviceCB : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (advertisedDevice.haveServiceUUID() && !advertisedDevice.getAddressType() && advertisedDevice.getAppearance() == 576 && advertisedDevice.getServiceUUID().length() == 2)
    {
      BLEDevice::getScan()->stop();
      OBD2Device = new BLEAdvertisedDevice(advertisedDevice);
    }
  }
};

void ScreenUp(void)
{
  NewScreen--;
  NewScreen = constrain(NewScreen, 0, ItemsInArray);
  tone(WIO_BUZZER, 2500, 100);
}
void ScreenHome(void)
{
  NewScreen = ItemsInArray;
  tone(WIO_BUZZER, 2500, 100);
}
void ScreenDown(void)
{
  NewScreen++;
  NewScreen = constrain(NewScreen, 0, ItemsInArray);
  tone(WIO_BUZZER, 2500, 100);
}

void setup()
{
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  tft.init();
  tft.setTextWrap(false);
  tft.setTextSize(7);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLUE);
  tft.setCursor(40, 100);
  tft.print("OBD2");
  for (int i = 1000; i <= 3000; i += 500)
  {
    tone(WIO_BUZZER, i);
    delay(50);
    noTone(WIO_BUZZER);
    delay(50);
  }
  BLEDevice::init("");
  BLEScan *ThisBLEScan = BLEDevice::getScan();
  ThisBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCB());
  ThisBLEScan->setActiveScan(true);
  ThisBLEScan->start(20);
  BLEClient *ThisClient = BLEDevice::createClient();
  ThisClient->connect(OBD2Device);
  ThisService = ThisClient->getService((BLEUUID)0xfff0);
  NotifyCharacteristic = ThisService->getCharacteristic((BLEUUID)0xfff1);
  NotifyCharacteristic->registerForNotify(NotifyCB);
  WriteCharacteristic = ThisService->getCharacteristic((BLEUUID)0xfff2);
  WriteCharacteristic->writeValue("AT Z\r");
  WriteCharacteristic->writeValue("AT E0\r");
  WriteCharacteristic->writeValue("AT SP 0\r");
  WriteCharacteristic->writeValue("01 00\r");
  while (NewString.substring(0, 12) != "SEARCHING...")
    yield();
  while (NewString.length() > 0)
  {
    if (NewString.substring(0, 17) == "UNABLE TO CONNECT")
    {
      ItemsInArray = 1;
      NewString = "";
    }
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(40, 100);
    tft.print("OBD2");
    delay(10);
  }
  NewScreen = ItemsInArray;
  WriteCharacteristic->writeValue("AT I\r");
  while (NewString == "")
    yield();
  tft.fillScreen(TFT_BLACK);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), ScreenUp, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), ScreenHome, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), ScreenDown, FALLING);
}

void loop()
{
  tft.setCursor(10, -30);
  for (I = 0; I < ItemsInArray; I++)
  {
    if (OldScreen != NewScreen)
    {
      tft.fillScreen(TFT_BLACK);
      OldScreen = NewScreen;
      break;
    }
    if (NewScreen != ItemsInArray && NewScreen != I)
      continue;
    NewString = "";
    WriteCharacteristic->writeValue(CommandArray[I]);
    while (NewString == "")
      yield();
  }
  tft.setTextSize(3);
  tft.fillRect(0, 280, 240, 40, TFT_BLACK);
  tft.setCursor(10, 280);
  tft.setTextColor(TFT_BLUE);
  tft.printf("%d", int(millis() / 1000));
}
