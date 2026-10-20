#pragma once

#include <Arduino.h>

class CliShell {
public:
    static CliShell& instance();

    void init();
    void process();

private:
    CliShell();
    void printBanner();
    void printPrompt();
    void executeCommand(const String& cmd_line);

    // Command Handlers
    void cmdHelp();
    void cmdSysinfo();
    void cmdSensor();
    void cmdBattery();
    void cmdI2cScan();
    void cmdPs();
    void cmdOffset(const String& arg);
    void cmdScreen(const String& arg);
    void cmdLed(const String& arg);
    void cmdWifiScan();
    void cmdReboot();

    String input_buffer;
};
