/*
*   utils.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "utils.h"
#include "screeninit.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"

#define COLOR_TITLE    0xFF9900
#define COLOR_WHITE    0xFFFFFF
#define COLOR_RED      0x0000FF
#define COLOR_BLACK    0x000000

struct option {
    int posY;
    u32 enabled;
};

static u32 waitInput(void)
{
    u32 pressedKey = 0;
    u32 key;

    //Wait for no keys to be pressed
    while(HID_PAD);

    do {
        //Wait for a key to be pressed
        while(!HID_PAD);

        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x13000; i; i--)
        {
            if(key != HID_PAD) break;
            if(i == 1) pressedKey = 1;
        }
    }
    while(!pressedKey);

    return key;
}

void configureCFW(const char *configPath, const char *patchedFirms[])
{
    initScreens();

    drawString(CONFIG_TITLE, 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save and reboot", 10, 30, COLOR_WHITE);

    const char *optionsText[] = { "( ) Updated SysNAND mode (A9LH-only)",
                                  "( ) Use pre-patched FIRMs",
                                  "( ) Force A9LH detection",
                                  "( ) Use 9.0 FIRM as default",
                                  "( ) Use second EmuNAND as default",
                                  "( ) Show current NAND in System Settings",
                                  "( ) Show GBA boot screen in patched AGB_FIRM" };

    u32 optionsAmount = sizeof(optionsText) / sizeof(char *);
    struct option options[optionsAmount];

    //Parse the existing configuration
    for(u32 i = 0; i < optionsAmount; i++)
        options[i].enabled = (config >> i) & 1;

    options[optionsAmount].enabled = (config >> 10) & 3;

    //Pre-select the first configuration option
    u32 selectedOption = 0;

    //Boring configuration menu
    while(1)
    {
        u16 pressed = 0;

        do {
            options[optionsAmount].posY = drawString("Screen-init brightness: 4( ) 3( ) 2( ) 1( )", 10, 53, selectedOption == optionsAmount ? COLOR_RED : COLOR_WHITE);

            for(u32 i = 0; i < optionsAmount; i++)
            {
                options[i].posY = drawString(optionsText[i], 10, !i ? options[optionsAmount].posY + 2 * SPACING_Y : options[i - 1].posY + SPACING_Y, selectedOption == i ? COLOR_RED : COLOR_WHITE);
                drawCharacter('x', 10 + SPACING_X, options[i].posY, options[i].enabled ? (selectedOption == i ? COLOR_RED : COLOR_WHITE) : COLOR_BLACK);
            }

            for(u32 i = 0; i < 4; i++)
                drawCharacter('x', 10 + (26 + 5 * i) * SPACING_X, options[optionsAmount].posY, options[optionsAmount].enabled == i ? (selectedOption == optionsAmount ? COLOR_RED : COLOR_WHITE) : COLOR_BLACK);

            pressed = waitInput();
        }
        while(!(pressed & MENU_BUTTONS));

        switch(pressed)
        {
            case BUTTON_UP:
                selectedOption = !selectedOption ? optionsAmount : selectedOption - 1;
                break;
            case BUTTON_DOWN:
                selectedOption = selectedOption >= optionsAmount - 1 ? 0 : selectedOption + 1;
                break;
            case BUTTON_LEFT:
                selectedOption = 0;
                break;
            case BUTTON_RIGHT:
                selectedOption = optionsAmount - 1;
                break;
            case BUTTON_A:
                if(selectedOption != optionsAmount) options[selectedOption].enabled = !options[selectedOption].enabled;
                else options[optionsAmount].enabled = options[optionsAmount].enabled == 3 ? 0 : options[optionsAmount].enabled + 1;
                break;
        }

        if(pressed == BUTTON_START) break;
    }

    //If the user has been using A9LH and the "Updated SysNAND" setting changed, delete the patched 9.0 FIRM
    if(((config >> 16) & 1) && ((config & 1) != options[0].enabled)) fileDelete(patchedFirms[3]);

    //If the "Show GBA boot screen in patched AGB_FIRM" setting changed, delete the patched AGB_FIRM
    if(((config >> 6) & 1) != options[6].enabled) fileDelete(patchedFirms[5]);

    //Preserve the last-used boot options (last 12 bits)
    config &= 0xFFF000;

    //Parse and write the new configuration
    for(u32 i = 0; i < optionsAmount; i++)
        config |= options[i].enabled << i;

    config |= options[optionsAmount].enabled << 10;

    fileWrite(&config, configPath, 3);

    //Zero the last booted FIRM flag
    CFG_BOOTENV = 0;

    //Reboot
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(1);
}

void deleteFirms(const char *firmPaths[], u32 firms)
{
    while(firms)
    {
        fileDelete(firmPaths[firms - 1]);
        firms--;
    }
}

void error(const char *message)
{
    initScreens();

    drawString("An error has occurred:", 10, 10, COLOR_RED);
    int posY = drawString(message, 10, 30, COLOR_WHITE);
    drawString("Press any button to shutdown", 10, posY + 2 * SPACING_Y, COLOR_WHITE);

    waitInput();

    //Shutdown
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
    while(1);
}