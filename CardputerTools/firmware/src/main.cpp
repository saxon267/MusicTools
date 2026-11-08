// Cardputer SD bridge.
//
// Turns the Cardputer into a serial file server so CardputerTools can list,
// read, write and delete files on the microSD card over USB.
//
// This is a separate firmware from MusicTools: flash it, manage the card, then
// flash MusicTools back when you are done.
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

namespace {

bool gSd = false;
uint8_t gBuf[512];

constexpr uint16_t kBg   = 0x2104;  // #282828
constexpr uint16_t kFg   = 0xEF7B;  // #ebdbb2
constexpr uint16_t kDim  = 0x948E;  // #928374
constexpr uint16_t kAcc  = 0xFD8F;  // #fabd2f
constexpr uint16_t kGrn  = 0xBDA4;  // #b8bb26
constexpr uint16_t kRed  = 0xFA68;  // #fb4934

void say(const char* s)
{
    Serial.println(s);
}

void fail(int code, const char* msg)
{
    Serial.printf("ERR %d %s\n", code, msg);
}

// Blocks until a newline arrives or the timeout expires.
bool readLine(String& out, uint32_t timeoutMs)
{
    static String buf;
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        while (Serial.available()) {
            const char c = (char)Serial.read();
            if (c == '\n') {
                out = buf;
                buf = "";
                out.trim();
                return true;
            }
            if (c != '\r' && buf.length() < 512) buf += c;
        }
        delay(1);
    }
    return false;
}

void splitVerb(const String& line, String& verb, String& args)
{
    const int sp = line.indexOf(' ');
    if (sp < 0) {
        verb = line;
        args = "";
    } else {
        verb = line.substring(0, sp);
        args = line.substring(sp + 1);
        args.trim();
    }
}

void drawStatus(const char* msg)
{
    auto& d = M5.Display;
    d.fillScreen(kBg);
    d.setTextColor(kAcc, kBg);
    d.setTextSize(2);
    d.setCursor(10, 10);
    d.print("SD BRIDGE");
    d.setTextSize(1);
    d.setTextColor(kDim, kBg);
    d.setCursor(10, 40);
    d.print("MusicTools companion");
    d.setTextColor(gSd ? kGrn : kRed, kBg);
    d.setCursor(10, 60);
    d.print(gSd ? "microSD ready" : "no card");
    d.setTextColor(kFg, kBg);
    d.setCursor(10, 80);
    d.print(msg);
}

bool isDir(const String& path)
{
    File f = SD.open(path);
    if (!f) return false;
    const bool d = f.isDirectory();
    f.close();
    return d;
}

void doLs(const String& path)
{
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        fail(2, "not a directory");
        if (dir) dir.close();
        return;
    }

    int count = 0;
    File f = dir.openNextFile();
    while (f) {
        ++count;
        f.close();
        f = dir.openNextFile();
    }
    dir.rewindDirectory();

    Serial.printf("OK %d\n", count);
    f = dir.openNextFile();
    while (f) {
        // name, size, type; tab separated so names may contain spaces
        Serial.printf("%s\t%u\t%c\n", f.name(), (unsigned)f.size(), f.isDirectory() ? 'd' : 'f');
        f.close();
        f = dir.openNextFile();
    }
    dir.close();
}

void doGet(const String& path)
{
    if (isDir(path)) {
        fail(3, "is a directory");
        return;
    }
    File f = SD.open(path, FILE_READ);
    if (!f) {
        fail(4, "open failed");
        return;
    }
    const uint32_t size = f.size();
    Serial.printf("OK %u\n", (unsigned)size);

    uint32_t sent = 0;
    while (sent < size) {
        const size_t n = f.read(gBuf, sizeof gBuf);
        if (n == 0) break;
        Serial.write(gBuf, n);
        sent += n;
    }
    f.close();
}

void doPut(const String& path, uint32_t size)
{
    Serial.println("READY");
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        fail(5, "open failed");
        return;
    }

    uint32_t got  = 0;
    uint32_t last = millis();
    while (got < size && millis() - last < 8000) {
        if (!Serial.available()) {
            delay(1);
            continue;
        }
        const size_t want = (size - got) < sizeof gBuf ? (size - got) : sizeof gBuf;
        const size_t n    = Serial.readBytes(gBuf, want);
        if (n == 0) continue;
        f.write(gBuf, n);
        got += n;
        last = millis();
    }
    f.close();
    Serial.printf("OK %u\n", (unsigned)got);
}

void handle(const String& line)
{
    String verb, args;
    splitVerb(line, verb, args);
    if (verb.length() == 0) return;

    if (verb == "PING") {
        Serial.println("PONG 1 cardputer-sd");
        return;
    }
    if (!gSd) {
        fail(1, "no card");
        return;
    }

    if (verb == "LS") {
        doLs(args.length() ? args : String("/"));
    } else if (verb == "GET") {
        doGet(args);
    } else if (verb == "PUT") {
        const int sp = args.lastIndexOf(' ');
        if (sp < 0) {
            fail(6, "usage PUT path size");
            return;
        }
        doPut(args.substring(0, sp), (uint32_t)args.substring(sp + 1).toInt());
    } else if (verb == "DEL") {
        Serial.println(SD.remove(args) ? "OK 0" : "ERR 7 remove failed");
    } else if (verb == "MKDIR") {
        Serial.println(SD.mkdir(args) ? "OK 0" : "ERR 8 mkdir failed");
    } else if (verb == "RMDIR") {
        Serial.println(SD.rmdir(args) ? "OK 0" : "ERR 9 rmdir failed");
    } else if (verb == "FREE") {
        Serial.printf("OK %u\n", (unsigned)(SD.totalBytes() - SD.usedBytes()));
    } else {
        fail(10, "unknown command");
    }
}

}  // namespace

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setBrightness(90);

    Serial.begin(115200);
    delay(300);

    const int sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
    const int miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
    const int mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
    const int cs   = M5.getPin(m5::pin_name_t::sd_spi_cs);

    SPI.begin(sclk, miso, mosi, cs);
    gSd = SD.begin(cs, SPI, 25000000);

    drawStatus("waiting for host");
    Serial.printf("READY %d\n", gSd ? 1 : 0);
}

void loop()
{
    String line;
    if (!readLine(line, 1000)) return;

    handle(line);
    const String msg = String("last: ") + line.substring(0, 18);
    drawStatus(msg.c_str());
}

