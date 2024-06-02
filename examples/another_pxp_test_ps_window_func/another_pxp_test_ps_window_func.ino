
#include "T4_PXP.h"

//Select One
//#include "little_joe.h"
//#include "ida.h"
#include "flexio_teensy_mm.h"
//#include "testPattern.h"

/************************************************/
//Specify the pins used for Non-SPI functions of display
#if defined(ARDUINO_TEENSY_MICROMOD)
#define TFT_DC 4
#define TFT_CS 5
#define TFT_RST 2
#else
#define TFT_DC 9
#define TFT_CS 7
#define TFT_RST 8
#endif

#define use9488


uint8_t bytesperpixel = 2;
float ScaleFact = 1.01f; //0.9 for littlejoe and ida
//float ScaleFact = 0.8f;

/* timing */
uint32_t capture_time = 0;
uint32_t pxp_time = 0;
uint32_t display_time = 0;
/************************************************/

#if defined(use9488)
#include <ILI9488_t3.h>
ILI9488_t3 tft = ILI9488_t3(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BLACK ILI9488_BLACK
#define TFT_YELLOW ILI9488_YELLOW
#define TFT_RED ILI9488_RED
#define TFT_GREEN ILI9488_GREEN
#define TFT_BLUE ILI9488_BLUE
#define CENTER ILI9488_t3::CENTER

#else
#include <ILI9341_t3n.h>
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BLACK ILI9341_BLACK
#define TFT_YELLOW ILI9341_YELLOW
#define TFT_RED ILI9341_RED
#define TFT_GREEN ILI9341_GREEN
#define TFT_BLUE ILI9341_BLUE
#define CENTER ILI9341_t3n::CENTER
#endif
/************************************************/


// NOTE: THIS SHOULD BE THE SIZE OF THE DISPLAY
DMAMEM uint16_t s_fb[480 * 320] __attribute__((aligned(64)));
uint16_t d_fb[480 * 320] __attribute__((aligned(64)));
const uint32_t sizeof_s_fb = sizeof(s_fb);
const uint32_t sizeof_d_fb = sizeof(d_fb);


bool pxpStarted = 0;
uint16_t outputWidth, outputHeight;
uint16_t FRAME_HEIGHT, FRAME_WIDTH;
/************************************************/

//=================================================================================================
// PXP_ps_window_output
//=================================================================================================

void PXP_ps_window_output(uint16_t disp_width, uint16_t disp_height, uint16_t image_width, uint16_t image_height,
                          void *buf_in, uint8_t format_in, uint8_t bpp_in, uint8_t byte_swap_in,
                          void *buf_out, uint8_t format_out, uint8_t bpp_out, uint8_t byte_swap_out,
                          uint8_t rotation, bool flip, float scaling,
                          uint16_t *scr_width, uint16_t *scr_height) {

    // Lets see if we can setup a window into the camera data
    Serial.printf("PXP_ps_window_output(%u, %u, %u, %u.... %u, %u. %f)\n", disp_width, disp_height, image_width, image_height,
                  rotation, flip, scaling);

    // we may bypass some of the helper functions and output the data directly.
    volatile IMXRT_NEXT_PXP_t *next_pxp = PXP_next_pxp();


    // lets reduce down to basic.
    PXP_input_background_color(0, 0, 153);

    // Initial input stuff.
    PXP_input_buffer(buf_in, bpp_in, image_width, image_height);
    // VYUY1P422, PXP_UYVY1P422
    PXP_input_format(format_in, 0, 0, byte_swap_in);
    if (format_in == PXP_Y8 || format_in == PXP_Y4)
        PXP_set_csc_y8_to_rgb();

    // Output stuff
    uint16_t out_width, out_height, output_Width, output_Height;
    if (rotation == 1 || rotation == 3) {
        out_width = disp_height;
        out_height = disp_width;
    } else {
        out_width = disp_width;
        out_height = disp_height;
    }

    PXP_output_buffer(buf_out, bpp_out, disp_width, disp_height);
    PXP_output_format(format_out, 0, 0, byte_swap_out);

    // Set the clip rect.
    //PXP_output_clip(disp_width - 1, disp_height - 1);

    // Rotation
    /* Setting this bit to 1'b0 will place the rotationre sources at  *
   * the output stage of the PXP data path. Image compositing will  *
   * occur before pixels are processed for rotation.                *
   * Setting this bit to a 1'b1 will place the rotation resources   *
   * before image composition.                                      *
   */
    PXP_rotate_position(0);
    //Serial.println("Rotating");
    // Performs the actual rotation specified
    PXP_rotate(rotation);

    // flip - pretty straight forward
    PXP_flip(flip);

    // Lets set scaling
    if (scaling == 0) scaling = 1.0;
    PXP_setScaling(scaling);

    // now if the input image and the scaled output image are not the same size, we may want to center either
    // the input or the output...
    uint32_t scaled_image_width = (uint32_t)((float)(image_width) / scaling);
    uint32_t scaled_image_height = (uint32_t)((float)(image_height) / scaling);

    if (rotation == 1 || rotation == 3) {
        PXP_output_clip(disp_height - 1, disp_width - 1);
    } else {
        PXP_output_clip(disp_width - 1, disp_height - 1);
    }

    uint16_t ul_x = 0;
    uint16_t ul_y = 0;
    uint16_t lr_x = disp_width - 1;
    uint16_t lr_y = disp_height - 1;

    if (rotation == 1 || rotation == 3) {
        if (scaled_image_width < disp_height) {
            ul_x = (disp_height - scaled_image_width) / 2;
            lr_x = ul_x + scaled_image_width - 1;
        }
        if (scaled_image_height < disp_width) {
            ul_y = (disp_width - scaled_image_height) / 2;
            lr_y = ul_y + scaled_image_height - 1;
        }
    } else {
        if (scaled_image_width < disp_width) {
            ul_x = (disp_width - scaled_image_width) / 2;
            lr_x = ul_x + scaled_image_width - 1;
        }
        if (scaled_image_height < disp_height) {
            ul_y = (disp_height - scaled_image_height) / 2;
            lr_y = ul_y + scaled_image_height - 1;
        }
    }
    PXP_input_position(ul_x, ul_y, lr_x, lr_y);  // need this to override the setup in pxp_input_buffer

    // See if we should input clip the image...  That is center the large image onto the screen.  later allow for panning.
    uint8_t *buf_in_clipped = (uint8_t *)buf_in;

    if (rotation == 1 || rotation == 3) {
        if (scaled_image_width > disp_height) {
            buf_in_clipped += ((scaled_image_width - disp_height) / 2) * bpp_in;
        }

        if (scaled_image_height > disp_width) {
            buf_in_clipped += ((scaled_image_height - disp_width) / 2) * bpp_in * image_width;
        }
    } else {
        if (scaled_image_width > disp_width) {
            buf_in_clipped += ((scaled_image_width - disp_width) / 2) * bpp_in;
        }

        if (scaled_image_height > disp_height) {
            buf_in_clipped += ((scaled_image_height - disp_height) / 2) * bpp_in * image_width;
        }
    }
    if (buf_in_clipped != (uint8_t *)buf_in) {
        Serial.printf("Input clip Buffer(%p -> %p)\n", buf_in, buf_in_clipped);
        next_pxp->PS_BUF = buf_in_clipped;
    }

    *scr_width = disp_width;
    *scr_height = disp_height;

    PXP_process();
}

/*****************************************************************************/



void setup() {
    Serial.begin(9600);
    while (!Serial && millis() < 5000) {}

    if (CrashReport) {
        Serial.print(CrashReport);
        Serial.println("Press any key to continue");
        while (Serial.read() != -1) {}
        while (Serial.read() == -1) {}
        while (Serial.read() != -1) {}
    }

    // Start ILI9341
    tft.begin();
    // Must be set to HW rotation 0 for PXP to work properly
    tft.setRotation(0);
    test_display();

    /******************************************
   * Runs PXP_init and memset buffers to 0.
   ******************************************/
    start_pxp();

    showCommands();
}


void loop() {
    int ch;
    if (Serial.available()) {
        uint8_t command = Serial.read();
        switch (command) {
            case '0':
                Serial.println("Rotation 0:");
                run_pxp(0, false, 0.0f);
                break;
            case '1':
                Serial.println("Rotation 1:");
                run_pxp(1, false, 0.0f);
                break;
            case '2':
                Serial.println("Rotation 2:");
                run_pxp(2, false, 0.0f);
                break;
            case '3':
                Serial.println("Rotation 3:");
                run_pxp(3, false, 0.0f);
                break;
            case '4':
                Serial.println("Rotation 0 w/flip:");
                run_pxp(0, true, 0.0f);
                break;
            case '5':
                Serial.println("Rotation 2 w/scaling:");
                run_pxp(2, false, ScaleFact);
                start_pxp();
                break;
            case '6':
                Serial.println("Rotation 3 w/Scaling:");
                run_pxp(3, false, ScaleFact);
                start_pxp();
                break;
            case 'f':
                tft.setRotation(0);
                capture_frame(false);
                draw_frame(image_width, image_height, s_fb);
                tft.setRotation(0);
                break;
            case 's':
                Serial.println("Starting PXP.....");
                start_pxp();
                break;
            case 't':
                test_display();
                break;
            case 'x':
                draw_source();
                break;
            case 'p':
                PXPShow();
                break;
            case 'n':
                PXPShowNext();
                break;
            case '?':
                showCommands();
            default:
                break;
        }
    }
}

void run_pxp(uint8_t rotation, bool flip, float scaling) {
    tft.fillScreen(TFT_BLACK);
    capture_time = millis();
    capture_frame(false);
    Serial.printf("Capture time (millis): %d, ", millis() - capture_time);

    pxp_time = micros();
    PXP_ps_window_output(tft.width(), tft.height(),    /* Display width and height */
                         image_width, image_height,    /* Image width and height */
                         s_fb, PXP_RGB565, 2, 0,       /* Input buffer configuration */
                         d_fb, PXP_RGB565, 2, 0,       /* Output buffer configuration */
                         rotation, flip, scaling,      /* Rotation, flip, scaling */
                         &outputWidth, &outputHeight); /* Frame Out size for drawing */
    Serial.printf("PXP time(micros) : %d, ", micros() - pxp_time);

    display_time = millis();
    draw_frame(outputWidth, outputHeight, d_fb);
    Serial.printf("Display time: %d\n", millis() - display_time);
}

void start_pxp() {
    PXP_init();

    memset(s_fb, 0, sizeof_s_fb);
    memset(d_fb, 0, sizeof_d_fb);

    pxpStarted = true;
}

void draw_frame(uint16_t width, uint16_t height, const uint16_t *buffer) {
    tft.fillScreen(TFT_BLACK);
    tft.writeRect(CENTER, CENTER, width, height, buffer);
}

void showCommands() {
    Serial.println("\n============  Command List ============");
    Serial.println("\ts => Reset/Start PXP");
    Serial.println("\tf => Capture normal frame");
    Serial.println("\tt => Test display");
    Serial.println("\tp => Print Registers");
    Serial.println("\tn => Print Next Registers");
    Serial.println("\t0 => Display PXP Rotation 0");
    Serial.println("\t1 => Display PXP Rotation 1");
    Serial.println("\t2 => Display PXP Rotation 2");
    Serial.println("\t3 => Display PXP Rotation 3");
    Serial.println("\t4 => PXP rotation 0 w/flip both axes");
    Serial.println("\t5 => PXP Scaling Test");
    Serial.println("\t6 => PXP Scaling w/Rotation 3 Test");


    Serial.println("\t? => Show menu.");
}

void capture_frame(bool show_debug_info) {
    tft.fillScreen(TFT_BLACK);
#if defined(has_ida)
    memcpy((uint16_t *)s_fb, ida, sizeof(ida));
#elif defined(has_littlejoe)
    memcpy((uint16_t *)s_fb, little_joe, sizeof(little_joe));
#elif defined(has_teensymm)
    memcpy((uint16_t *)s_fb, flexio_teensy_mm, sizeof(flexio_teensy_mm));
#elif defined(has_testpattern)
    memcpy((uint16_t *)s_fb, testPattern, sizeof(testPattern));
#endif
}

void test_display() {
    tft.setRotation(3);
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_BLACK);
    delay(500);
    tft.setRotation(0);
}

// This  function prints a nicely formatted output of the PXP register settings
// The formatting does require using a monospaced font, like Courier
void PXPShow(void) {
    Serial.printf("CTRL:         %08X       STAT:         %08X\n", PXP_CTRL, PXP_STAT);
    Serial.printf("OUT_CTRL:     %08X       OUT_BUF:      %08X    \nOUT_BUF2:     %08X\n", PXP_OUT_CTRL, PXP_OUT_BUF, PXP_OUT_BUF2);
    Serial.printf("OUT_PITCH:    %8lu       OUT_LRC:       %3u,%3u\n", PXP_OUT_PITCH, PXP_OUT_LRC >> 16, PXP_OUT_LRC & 0xFFFF);

    Serial.printf("OUT_PS_ULC:    %3u,%3u       OUT_PS_LRC:    %3u,%3u\n", PXP_OUT_PS_ULC >> 16, PXP_OUT_PS_ULC & 0xFFFF,
                  PXP_OUT_PS_LRC >> 16, PXP_OUT_PS_LRC & 0xFFFF);
    Serial.printf("OUT_AS_ULC:    %3u,%3u       OUT_AS_LRC:    %3u,%3u\n", PXP_OUT_AS_ULC >> 16, PXP_OUT_AS_ULC & 0xFFFF,
                  PXP_OUT_AS_LRC >> 16, PXP_OUT_AS_LRC & 0xFFFF);
    Serial.println();  // section separator
    Serial.printf("PS_CTRL:      %08X       PS_BUF:       %08X\n", PXP_PS_CTRL, PXP_PS_BUF);
    Serial.printf("PS_UBUF:      %08X       PS_VBUF:      %08X\n", PXP_PS_UBUF, PXP_PS_VBUF);
    Serial.printf("PS_PITCH:     %8lu       PS_BKGND:     %08X\n", PXP_PS_PITCH, PXP_PS_BACKGROUND_0);
    Serial.printf("PS_SCALE:     %08X       PS_OFFSET:    %08X\n", PXP_PS_SCALE, PXP_PS_OFFSET);
    Serial.printf("PS_CLRKEYLOW: %08X       PS_CLRKEYLHI: %08X\n", PXP_PS_CLRKEYLOW_0, PXP_PS_CLRKEYHIGH_0);
    Serial.println();
    Serial.printf("AS_CTRL:      %08X       AS_BUF:       %08X    AS_PITCH: %6u\n", PXP_AS_CTRL, PXP_AS_BUF, PXP_AS_PITCH & 0xFFFF);
    Serial.printf("AS_CLRKEYLOW: %08X       AS_CLRKEYLHI: %08X\n", PXP_AS_CLRKEYLOW_0, PXP_AS_CLRKEYHIGH_0);
    Serial.println();
    Serial.printf("CSC1_COEF0:   %08X       CSC1_COEF1:   %08X    \nCSC1_COEF2:   %08X\n",
                  PXP_CSC1_COEF0, PXP_CSC1_COEF1, PXP_CSC1_COEF2);
    Serial.println();  // section separator
    Serial.printf("POWER:        %08X       NEXT:         %08X\n", PXP_POWER, PXP_NEXT);
    Serial.printf("PORTER_DUFF:  %08X\n", PXP_PORTER_DUFF_CTRL);
}

void draw_source() {
    tft.fillScreen(TFT_BLACK);
    draw_frame(image_width, image_height, s_fb);
    //tft.writeRect(CENTER, CENTER, image_width, image_height, (uint16_t *)s_fb);
}


void PXPShowNext(void) {
    volatile IMXRT_NEXT_PXP_t *next_pxp = PXP_next_pxp();
    Serial.printf("Show next PXP data(%p)\n", next_pxp);
    Serial.printf("CTRL:         %08X       STAT:         %08X\n", next_pxp->CTRL, next_pxp->STAT);
    Serial.printf("OUT_CTRL:     %08X       OUT_BUF:      %08X    \nOUT_BUF2:     %08X\n", next_pxp->OUT_CTRL, next_pxp->OUT_BUF, next_pxp->OUT_BUF2);
    Serial.printf("OUT_PITCH:    %8lu       OUT_LRC:       %3u,%3u\n", next_pxp->OUT_PITCH, next_pxp->OUT_LRC >> 16, next_pxp->OUT_LRC & 0xFFFF);

    Serial.printf("OUT_PS_ULC:    %3u,%3u       OUT_PS_LRC:    %3u,%3u\n", next_pxp->OUT_PS_ULC >> 16, next_pxp->OUT_PS_ULC & 0xFFFF,
                  next_pxp->OUT_PS_LRC >> 16, next_pxp->OUT_PS_LRC & 0xFFFF);
    Serial.printf("OUT_AS_ULC:    %3u,%3u       OUT_AS_LRC:    %3u,%3u\n", next_pxp->OUT_AS_ULC >> 16, next_pxp->OUT_AS_ULC & 0xFFFF,
                  next_pxp->OUT_AS_LRC >> 16, next_pxp->OUT_AS_LRC & 0xFFFF);
    Serial.println();  // section separator
    Serial.printf("PS_CTRL:      %08X       PS_BUF:       %08X\n", next_pxp->PS_CTRL, next_pxp->PS_BUF);
    Serial.printf("PS_UBUF:      %08X       PS_VBUF:      %08X\n", next_pxp->PS_UBUF, next_pxp->PS_VBUF);
    Serial.printf("PS_PITCH:     %8lu       PS_BKGND:     %08X\n", next_pxp->PS_PITCH, next_pxp->PS_BACKGROUND);
    float x_ps_scale = (next_pxp->PS_SCALE & 0xffff) / 4096.0;
    float y_ps_scale = (next_pxp->PS_SCALE >> 16) / 4096.0;
    Serial.printf("PS_SCALE:  %5.3f,%5.3f       PS_OFFSET:    %08X\n", x_ps_scale, y_ps_scale, next_pxp->PS_OFFSET);
    Serial.printf("PS_CLRKEYLOW: %08X       PS_CLRKEYLHI: %08X\n", next_pxp->PS_CLRKEYLOW, next_pxp->PS_CLRKEYHIGH);
    x_ps_scale *= 1 << ((next_pxp->PS_CTRL >> 10) & 0x3);
    y_ps_scale *= 1 << ((next_pxp->PS_CTRL >> 8) & 0x3);
    Serial.printf("SCALING:   %5.3f,%5.3f\n", x_ps_scale, y_ps_scale);
    Serial.println();
    Serial.printf("AS_CTRL:      %08X       AS_BUF:       %08X    AS_PITCH: %6u\n", next_pxp->AS_CTRL, next_pxp->AS_BUF, next_pxp->AS_PITCH & 0xFFFF);
    Serial.printf("AS_CLRKEYLOW: %08X       AS_CLRKEYLHI: %08X\n", next_pxp->AS_CLRKEYLOW, next_pxp->AS_CLRKEYHIGH);
    Serial.println();
    Serial.printf("CSC1_COEF0:   %08X       CSC1_COEF1:   %08X    \nCSC1_COEF2:   %08X\n",
                  next_pxp->CSC1_COEF0, next_pxp->CSC1_COEF1, next_pxp->CSC1_COEF2);
    Serial.println();  // section separator
    Serial.printf("POWER:        %08X       NEXT:         %08X\n", next_pxp->POWER, next_pxp->NEXT);
    Serial.printf("PORTER_DUFF:  %08X\n", next_pxp->PORTER_DUFF_CTRL);
}
