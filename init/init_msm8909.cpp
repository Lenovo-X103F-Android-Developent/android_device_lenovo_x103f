/*
   Copyright (c) 2019, Vladimir Bely <vlwwwwww@gmail.com>

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <android-base/properties.h>
#include <android-base/file.h>
#include <android-base/strings.h>

#include "vendor_init.h"
#include "property_service.h"
#include "log.h"
#include "util.h"

#define SERIAL_NUMBER_FILE "/persist/.sn.bin"
#define BT_ADDR_FILE "/persist/.bt_nv.bin"

using android::base::GetProperty;
using android::base::ReadFileToString;
using android::base::Trim;
using android::init::property_set;

__attribute__ ((weak))
void init_target_properties()
{
}

static void init_alarm_boot_properties()
{
    char const *boot_reason_file = "/proc/sys/kernel/boot_reason";
    std::string boot_reason;
    std::string tmp = GetProperty("ro.boot.alarmboot","");

    if (ReadFileToString(boot_reason_file, &boot_reason)) {
        /*
         * Setup ro.alarm_boot value to true when it is RTC triggered boot up
         * For existing PMIC chips, the following mapping applies
         * for the value of boot_reason:
         *
         * 0 -> unknown
         * 1 -> hard reset
         * 2 -> sudden momentary power loss (SMPL)
         * 3 -> real time clock (RTC)
         * 4 -> DC charger inserted
         * 5 -> USB charger insertd
         * 6 -> PON1 pin toggled (for secondary PMICs)
         * 7 -> CBLPWR_N pin toggled (for external power supply)
         * 8 -> KPDPWR_N pin toggled (power key pressed)
         */
        if (Trim(boot_reason) == "3" || tmp == "true")
            property_set("ro.alarm_boot", "true");
        else
            property_set("ro.alarm_boot", "false");
    }
}

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_dual(char const system_prop[], char const vendor_prop[], char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
}

void set_fingerprint()
{
	property_override_dual("ro.build.fingerprint", "ro.boot.fingerprint", 
        "Lenovo/TB-X103F/TB-X103F:6.0.1/LenovoTB-X103F/TB-X103F_S000038_180317_ROW:user/release-keys");

    property_override("ro.build.description", 
        "msm8909-user 6.0.1 LenovoTB-X103F TB-X103F_S000038_180317_ROW release-keys");
}

void set_sn()
{
    char serial_number1[30];
    char serial_number2[10];
    unsigned char cursymbol;
    int got = 0;

    FILE *serial_number_file;

    if ((serial_number_file = fopen(SERIAL_NUMBER_FILE, "r")) == NULL) {
        printf("%s: failed to open %s", __func__, SERIAL_NUMBER_FILE);
        return;
    } else {
        //read 1st part of SN
        serial_number1[0] = '\0';
        fscanf(serial_number_file, "%c", &cursymbol);
        do {
            serial_number1[strlen(serial_number1)+1] = '\0';
            serial_number1[strlen(serial_number1)] = cursymbol;
            got = 0;
            got += fscanf(serial_number_file, "%c", &cursymbol);
        } while (got>0 && cursymbol!='\0');

        //skip null-symbols
        while (cursymbol == '\0' && got>0) {
            got = 0;
            got += fscanf(serial_number_file, "%c", &cursymbol);
        }

        //read 2nd part of SN
        serial_number2[0] = '\0';
        do {
            serial_number2[strlen(serial_number2)+1] = '\0';
            serial_number2[strlen(serial_number2)] = cursymbol;

            got = 0;
            got += fscanf(serial_number_file, "%c", &cursymbol);
        } while (got>0 && cursymbol!='\0');

        if (strlen(serial_number1)<10 || strlen(serial_number2)<5) {
            printf("%s: error read %s", __func__, SERIAL_NUMBER_FILE);
        } else {
            printf("HW BOARD ADDR is %s\nHW SN is%s\n", serial_number1, serial_number2);
            property_override("ro.boardserialno", serial_number1);
            property_override("ro.serialno", serial_number2);
        }
        fclose(serial_number_file);
    }
}

void set_bt_mac() {
    unsigned char bt_mac[6];
    char prop_bt_mac [18];

    FILE *bt_addr_file;

    if ((bt_addr_file = fopen(BT_ADDR_FILE, "r")) == NULL) {
        printf("%s: failed to open %s", __func__, BT_ADDR_FILE);
        return;
    } else {
        fscanf(bt_addr_file, "%*c%*c%*c%c", &bt_mac[5]);
        fscanf(bt_addr_file, "%c", &bt_mac[4]);
        fscanf(bt_addr_file, "%c", &bt_mac[3]);
        fscanf(bt_addr_file, "%c", &bt_mac[2]);
        fscanf(bt_addr_file, "%c", &bt_mac[1]);
        fscanf(bt_addr_file, "%c", &bt_mac[0]);

        sprintf(prop_bt_mac,"%02X:%02X:%02X:%02X:%02X:%02X", 
            bt_mac[0], bt_mac[1], bt_mac[2], bt_mac[3], bt_mac[4], bt_mac[5]);

        printf("BT ADDR is %s",prop_bt_mac);
        property_override("persist.service.bdroid.bdaddr", prop_bt_mac);
        property_override("ro.boot.btmacaddr", prop_bt_mac);
    
        fclose(bt_addr_file);
    }
}

void vendor_load_properties()
{
    set_fingerprint();
    set_sn();
    set_bt_mac();
    init_alarm_boot_properties();
}
