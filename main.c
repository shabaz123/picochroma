/************************************************************************
 * picochroma - A digital lighting system built with Pi Pico
 * main.c
 * rev 1.0 - January 2023 - shabaz
 ************************************************************************/

// ********** header files *****************
#include <stdio.h>
#include <string.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// ***************** defines ***************
// Pico-Eurocard used GPIO22 for the LED.
#define LED_PIN 22
// #define LED_PIN PICO_DEFAULT_LED_PIN
#define BUTTON_PIN 27
#define ENC_A_PIN 7
#define ENC_B_PIN 6
// use a period of about 41 kHz for PWM to reduce risk of flicker
#define PWM_MAX 3048
// PWM_1PCT is PWM_MAX/100, rounded up.
#define PWM_1PCT 31
// set CKDIV to 1 for approx 41 kHz PWM frequency if PWM_MAX is 3048
// set to 2 for approximately 20.5 kHz, if your LED driver can't handle 41 kHz
#define CKDIV 2
// PWN pins for the lighting
// pin pairs are consecutive to use the same PWM slice per pair
#define COLD_PIN_0 16
#define WARM_PIN_0 17
#define COLD_PIN_1 18
#define WARM_PIN_1 19
#define LED_TYPE_COLD 0
#define LED_TYPE_WARM 1

// LED color-related definitions for Warm and Cold LEDs
// (_W and _C respectively).
// color temperatures
#define CCT_W 2700
#define CCT_C 7100
// max illumination (0.0-1.0)
#define EM_W 1.0
#define EM_C 0.85
// initial settings
#define CCT_DEFAULT 4000
// Brightness level is 0-9
#define BRIGHT_DEFAULT 5

// number of different color temperatures supported
#define CCT_ARR_SIZE 76

// GPIO pins for 7-seg display
#define SEG_A_PIN 8
#define SEG_B_PIN 9
#define SEG_C_PIN 10
#define SEG_D_PIN 11
#define SEG_E_PIN 12
#define SEG_F_PIN 13
#define SEG_G_PIN 14
#define SEG_DP_PIN 15
// 7-seg bitmaps for digits 0-9, DP, Blank
#define BM_0 0x3f
#define BM_1 0x06
#define BM_2 0x5b
#define BM_3 0x4f
#define BM_4 0x66
#define BM_5 0x6d
#define BM_6 0x7d
#define BM_7 0x07
#define BM_8 0x7f
#define BM_9 0x6f
#define BM_DP 0x80
#define BM_BLANK 0x00
#define BM_HYPHEN 0x40
// index into the above bitmap for non-digits:
#define IDX_DP 10
#define IDX_BLANK 11
#define IDX_HYPHEN 12

// 7-seg digit cathode drive
#define DIG1_PIN 20
#define DIG2_PIN 21

// strategy for leading zero handling on the 7-seg LEDs
#define SUPPRESS_DIG_NONE 0
#define SUPPRESS_DIG_LEFT 1
#define SUPPRESS_DIG_ALL 2

//Board LED
#define PICO_LED_ON gpio_put(LED_PIN, 1)
#define PICO_LED_OFF gpio_put(LED_PIN, 0)

// Inputs
#define BUTTON_UNPRESSED (gpio_get(BUTTON_PIN)!=0)
#define BUTTON_PRESSED (gpio_get(BUTTON_PIN)==0)
// Rotary encoder 2-bit value
#define ENC_VAL ((gpio_get(ENC_A_PIN)<<1) | gpio_get(ENC_B_PIN))
// button press and debounce states
#define BMENU_IDLE 0
#define BMENU_PRESSED 1
#define BMENU_DEBOUNCING 2
// button clicks result in these modes of operation
#define MODE_INTENSITY 0
#define MODE_COLOR 1
// if encoder is too granular, increase these values
#define MICROSTEP_MAX_INTENSITY 5
#define MICROSTEP_MAX_COLOR 2
// misc
#define FOREVER 1

// ******** constants ******************
const uint8_t SEG_PIN[8] = {SEG_A_PIN, SEG_B_PIN, SEG_C_PIN, SEG_D_PIN, SEG_E_PIN, SEG_F_PIN, SEG_G_PIN, SEG_DP_PIN};
const uint8_t DIG_BM[13] = {BM_0, BM_1, BM_2, BM_3, BM_4, BM_5, BM_6, BM_7, BM_8, BM_9, BM_DP, BM_BLANK, BM_HYPHEN};
const uint8_t DIG_PIN[] = {DIG1_PIN, DIG2_PIN};

const unsigned int CCT[CCT_ARR_SIZE] = {2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700,
                                        3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000,
                                        5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000,
                                        6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000, 7100, 7200, 7300,
                                        7400, 7500, 7600, 7700, 7800, 7900, 8000, 8100, 8200, 8300, 8400, 8500, 8600,
                                        8700, 8800, 8900, 9000, 9100, 9200, 9300, 9400, 9500, 9600, 9700, 9800, 9900,
                                        10000};
const double X_COORD[CCT_ARR_SIZE] = {0.476993298, 0.468234043, 0.459857792, 0.451855627, 0.444216942, 0.436929834,
                                      0.429981469, 0.423358394, 0.417046802, 0.411032757, 0.405302374, 0.39984196,
                                      0.394638128, 0.38967788, 0.384948667, 0.380438429, 0.376135624, 0.37202924,
                                      0.368108798, 0.364364351, 0.360786471, 0.35736624, 0.354095228, 0.350965477,
                                      0.347969481, 0.345100161, 0.342350848, 0.33971526, 0.337187481, 0.334761938,
                                      0.332433384, 0.330196881, 0.328047774, 0.325981682, 0.323994478, 0.32208227,
                                      0.320241393, 0.318468391, 0.316760005, 0.315113158, 0.313524949, 0.311992639,
                                      0.310513639, 0.309085506, 0.307705927, 0.306372719, 0.305083813, 0.303837255,
                                      0.302631191, 0.301463867, 0.300333621, 0.299238876, 0.298178139, 0.297149991,
                                      0.296153085, 0.295186142, 0.29424795, 0.293337352, 0.292453251, 0.291594603,
                                      0.290760414, 0.289949738, 0.289161674, 0.288395363, 0.287649987, 0.286924766,
                                      0.286218954, 0.285531841, 0.284862749, 0.28421103, 0.283576065, 0.282957262,
                                      0.282354056, 0.281765905, 0.281192291, 0.28063272};
const double Y_COORD[CCT_ARR_SIZE] = {0.41367529, 0.41229905, 0.410598847, 0.40862976, 0.406440454, 0.404073617,
                                      0.401566449, 0.398951179, 0.396255578, 0.393503449, 0.390715097, 0.387907751,
                                      0.385095955, 0.382291914, 0.379505808, 0.37674607, 0.37401962, 0.371332087,
                                      0.368687987, 0.366090887, 0.363543545, 0.361048031, 0.35860583, 0.356217933,
                                      0.353884917, 0.351607005, 0.349384133, 0.347215989, 0.345102064, 0.343041682,
                                      0.341034034, 0.339078203, 0.337173188, 0.335317921, 0.333511285, 0.331752126,
                                      0.330039268, 0.328371519, 0.32674768, 0.325166555, 0.323626954, 0.322127697,
                                      0.320667621, 0.319245579, 0.317860448, 0.316511126, 0.315196535, 0.313915625,
                                      0.31266737, 0.311450773, 0.310264865, 0.309108702, 0.30798137, 0.306881982,
                                      0.305809677, 0.304763623, 0.303743012, 0.302747064, 0.301775023, 0.300826159,
                                      0.299899765, 0.298995157, 0.298111676, 0.297248683, 0.296405562, 0.295581717,
                                      0.294776573, 0.293989574, 0.293220182, 0.29246788, 0.291732165, 0.291012554,
                                      0.290308579, 0.289619789, 0.288945746, 0.28828603};
const double BRIGHT_TABLE[10] = {0.121, 0.153, 0.193, 0.244, 0.309, 0.391, 0.494, 0.625, 0.791, 1};

// ************ global variables *********************
uint slice_num[2]; // PWM slices
struct repeating_timer seg_refresh_timer;
// the two digits to display are stored in digbuf[], and the
// segment refresh timer callback will use this to control the 7-seg outputs
char digbuf[2] = {IDX_BLANK, IDX_BLANK};
char digscanidx = 0; // used only by the segment refresh timer callback (for LED multiplexing)
char old_enc_val = 0; // stores the rotary encoder value to determine rotation direction
int rotval = 0; // stores the value to show on the 7-seg display, set by the rotary encoder callback
int microstep = 0; // used to count small increments of the encoder, to reduce sensitivity to small rotation
char bmenu_state = BMENU_IDLE; // menu button state normally idle
char appmode = MODE_INTENSITY; // default mode (intensity control)
int debounce_count = 0; // used to debounce the button
int intensity; // intensity (0-9) or -1 for off
int color; // color temperature (in hundreds of K)
int enc_raw_intensity; // raw intensity value from the rotary encoder (to be divided)
int enc_raw_color; // raw color temperature value from the rotary encoder (to be divided)
int colmin, colmax; // min/max supported color temperatures (in hundreds of K)
int pwm_store[2][2];
int pwm_table_w[CCT_ARR_SIZE]; // PWM values for artifical max (i.e. un-boosted) brightnesses per color temperature
int pwm_table_c[CCT_ARR_SIZE];
int cct_tbl_min_div100; // stores the value of CCT[0]/100 (because it is used a lot)


// ********** functions *************************

void
print_title(void) {
    printf("\n");
    printf("######  ####    ######      #######        ######    ##   ##  ######     #######    ##    ##    ##   \n");
    printf("##   ##  ##   ##      #   ##       ##    ##      ##  ##   ##  ##   ##  ##       ##  ###  ###   #  #  \n");
    printf("##   ##  ##  ##          ##         ##  ##           ##   ##  ##   ## ##         ## ########  ##  ## \n");
    printf("######   ##  ##          ##         ##  ##           #######  ######  ##         ## ##    ## ##    ##\n");
    printf("##       ##  ##          ##         ##  ##           ##   ##  ## ##   ##         ## ##    ## ########\n");
    printf("##       ##   ##      #   ##       ##    ##      ##  ##   ##  ##  ##   ##       ##  ##    ## ##    ##\n");
    printf("##      ####    ######      #######        ######    ##   ##  ##   ##    #######    ##    ## ##    ##\n");
    printf("  A M B I E N T   /   P H O T O   /   V I D E O     L E D    L I G H T I N G    C O N T R O L L E R  \n");
    printf("\n");
    printf("Built on %s %s\n", __DATE__, __TIME__);
    printf("\n");
}

// general-purpose long delay timer if required
void
sleep_sec(uint32_t s) {
    sleep_ms(s * 1000);
}

// used to set the 7-seg digit buffers
void
set_dispval(int val, char suppress) {
    uint8_t d0, d1;

    // special case: handle a negative value
    if (val < 0) { // display just a hyphen
        digbuf[0] = IDX_BLANK;
        digbuf[1] = IDX_HYPHEN;
        return;
    }
    // handle positive values
    if (val > 99) { // special case: display 00
        digbuf[0] = 0;
        digbuf[1] = 0;
        return;
    }
    // all other values
    digbuf[0] = ((uint8_t) val / 10);
    digbuf[1] = (uint8_t) val % 10;
    switch (suppress) {
        case SUPPRESS_DIG_NONE:
            break;
        case SUPPRESS_DIG_LEFT:
            if (digbuf[0] == 0) {
                digbuf[0] = IDX_BLANK;
            }
            break;
        case SUPPRESS_DIG_ALL:
            if (digbuf[0] == 0) {
                digbuf[0] = IDX_BLANK;
            }
            if (digbuf[1] == 0) {
                digbuf[1] = IDX_BLANK;
            }
            break;
        default:
            break;
    }
}

// display_digit will output the correct GPIO levels to light up a single digit
void
display_digit(char idx, char val) {
    int i;
    int bm;
    gpio_put(DIG_PIN[0], 0);
    gpio_put(DIG_PIN[1], 0);
    bm = DIG_BM[val];
    for (i = 0; i < 7; i++) {
        if (bm & 0x01) {
            gpio_put(SEG_PIN[i], 1);
        } else {
            gpio_put(SEG_PIN[i], 0);
        }
        bm = bm >> 1;
    }
    gpio_put(DIG_PIN[idx], 1);
}

// scan out the digits at a fast rate:
bool seg_refresh_cb(struct repeating_timer *t) {
    display_digit(digscanidx, digbuf[digscanidx]);
    digscanidx++;
    if (digscanidx > 1) {
        digscanidx = 0;
    }
    return true;
}

// handle button presses
void button_handler(void) {
    if (bmenu_state == BMENU_IDLE) {
        if (appmode == MODE_INTENSITY) {
            appmode = MODE_COLOR;
            rotval = color;
        } else {
            appmode = MODE_INTENSITY;
            rotval = intensity;
        }
        set_dispval(rotval, SUPPRESS_DIG_LEFT);
        bmenu_state = BMENU_PRESSED;
        microstep = 0;
    }
}

// initial color/brightness table values setup
void
led_tables_init(void) {
    int i;
    int maxval = 0;
    double scale = 1.0;
    double xw, yw, xc, yc; // chromaticity co-ordinates for LEDs
    double xt; // chromaticity co-ordinates for target color
    int kt; // target color temperature
    double rwc; // ratio of weighting coefficients
    double ry; // ratio of LED chromaticity ordinates
    double rem; // ratio of max LED illumination
    double rdc; // ratio of LED duty cycles
    double et; // desired illuminance (must be less than EM_W+EM_C)
    double dc_w, dc_c; // duty cycles for LEDs

    cct_tbl_min_div100 = CCT[0] / 100;
    intensity = BRIGHT_DEFAULT;
    color = CCT_DEFAULT / 100;
    enc_raw_intensity = intensity * MICROSTEP_MAX_INTENSITY;
    enc_raw_color = color * MICROSTEP_MAX_COLOR;
    colmin = CCT_W / 100;
    colmax = CCT_C / 100;

    i = (CCT_W - CCT[0]) / 100;
    xw = X_COORD[i];
    yw = Y_COORD[i];
    i = (CCT_C - CCT[0]) / 100;
    xc = X_COORD[i];
    yc = Y_COORD[i];
    et = EM_C + EM_W;

    printf("Cold %d K (xc,yc) = (%f,%f)\n", CCT_C, xc, yc);
    printf("Warm %d K (xw,yw) = (%f,%f)\n", CCT_W, xw, yw);
    printf("Max illumination ratio (cold,warm) (%.2f, %.2f)\n", EM_C, EM_W);

    for (i = 0; i < CCT_ARR_SIZE; i++) {
        pwm_table_w[i] = 0;
        pwm_table_c[i] = 0;
    }

    // build up table of PWM values for all color temperatures
    for (i = colmin - cct_tbl_min_div100; i < (colmax - cct_tbl_min_div100) + 1; i++) {
        xt = X_COORD[i];
        kt = (i * 100) + CCT[0];
        rwc = (xw - xt) / (xt - xc);
        ry = yc / yw;
        rem = EM_C / EM_W;
        rdc = (rwc * ry) / rem;
        dc_w = et / (EM_W * (1 + (rem * rdc)));
        dc_c = rdc * dc_w;
        pwm_table_w[i] = dc_w * PWM_MAX;
        pwm_table_c[i] = dc_c * PWM_MAX;
        // printf("%d:(%dW,%dC), ", i+cct_tbl_min_div100, pwm_table_w[i], pwm_table_c[i]);
    }

    // normalize table so that duty cycle is never greater than PWM_MAX
    // seek all values except the CCT values of the LEDs
    for (i = (colmin - cct_tbl_min_div100) + 1; i < (colmax - cct_tbl_min_div100); i++) {
        if (pwm_table_w[i] > maxval) {
            maxval = pwm_table_w[i];
        }
        if (pwm_table_c[i] > maxval) {
            maxval = pwm_table_c[i];
        }
    }
    scale = PWM_MAX / (double) maxval;
    for (i = 0; i < CCT_ARR_SIZE; i++) {
        pwm_table_w[i] = (int) (((double) pwm_table_w[i]) * scale);
        pwm_table_c[i] = (int) (((double) pwm_table_c[i]) * scale);
    }
    // trim the PWM for the CCT values of the LEDs to the max allowed
    if (pwm_table_w[colmin - cct_tbl_min_div100] > PWM_MAX) {
        pwm_table_w[colmin - cct_tbl_min_div100] = PWM_MAX;
    }
    if (pwm_table_c[colmax - cct_tbl_min_div100] > PWM_MAX) {
        pwm_table_c[colmax - cct_tbl_min_div100] = PWM_MAX;
    }

    // print out the LED tables
    printf("\nLED Lookup Table:\n\n");
    //printf("[CCT](PWM_C,PWM_W) = ");
    //for (i = 0; i < CCT_ARR_SIZE; i++) {
    //    printf("[%d](%d,%d), ", CCT[i], pwm_table_c[i], pwm_table_w[i]);
    //}
    //printf("END\n\n");
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?><tbl>");
    for (i = 0; i < CCT_ARR_SIZE; i++) {
        if (pwm_table_c[i]==0 && pwm_table_w[i]==0) {
            // skip unused entries
            continue;
        }
        printf("<r K=\"%d\">", CCT[i]);
        printf("<c>%d</c>", pwm_table_c[i]);
        printf("<w>%d</w>", pwm_table_w[i]);
        printf("</r>");
    }
    printf("</tbl>\n");
    printf("\n");
}

// set the PWM level (0 to PWM_MAX) for the first or second lighting module
// where ledtype is either LED_TYPE_COLD or LED_TYPE_WARM
void
set_pwm_level(char module, char ledtype, int level) {
    if (level > PWM_MAX) {
        level = PWM_MAX;
    }
    pwm_set_chan_level(slice_num[module], ledtype, level); // set PWM value
}

// set the PWM percentage (0 to 100) for the first or second lighting module
// where ledtype is either LED_TYPE_COLD or LED_TYPE_WARM
void
set_pwm_percent(char module, char ledtype, int percent) {
    int level;
    level = PWM_1PCT * percent;
    if (level > PWM_MAX) {
        level = PWM_MAX;
    }
    pwm_set_chan_level(slice_num[module], ledtype, level); // set PWM value
}

// sets the PWM for the warm and cold LEDs
// col is the desired color temperature / 100
// bright is the brightness in the range 0-9 (-1 sets it completely off)
void
set_lighting(char module, int col, int bright) {
    double level_w, level_c;
    if (bright >= 0) {
        level_w = BRIGHT_TABLE[bright] * (double) pwm_table_w[col - cct_tbl_min_div100];
        level_c = BRIGHT_TABLE[bright] * (double) pwm_table_c[col - cct_tbl_min_div100];
        set_pwm_level(module, LED_TYPE_WARM, (int) (level_w));
        set_pwm_level(module, LED_TYPE_COLD, (int) (level_c));
        printf("pwm (cold,warm) (%d,%d)\n", (int) level_c, (int) level_w);
    } else { // switch LEDs off
        set_pwm_level(module, LED_TYPE_WARM, 0);
        set_pwm_level(module, LED_TYPE_COLD, 0);
    }
}

// handle input events (mainly rotary encoder).
void input_cb(uint gpio, uint32_t events) {
    char enc_val;
    char enc_state;
    int incr;

    if (gpio == BUTTON_PIN) {
        button_handler();
        return;
    }

    // rotary encoder state machine, to determine CW or CCW rotation:
    enc_val = ENC_VAL;
    enc_state = (enc_val << 2) | old_enc_val;
    old_enc_val = enc_val;
    switch (enc_state) {
        case 0x1:
        case 0x7:
        case 0xe:
        case 0x8:
            incr = 1; // CW
            break;
        case 0xd:
        case 0x4:
        case 0x2:
        case 0xb:
            incr = -1; // CCW
            break;
        default:
            incr = 0;
            break;
    }

    // algorithm to count multiple steps of the encoder before
    // incrementing/decrementing the intensity and color variables
    if (appmode == MODE_INTENSITY) {
        enc_raw_intensity = enc_raw_intensity + incr;
        if (enc_raw_intensity > MICROSTEP_MAX_INTENSITY * 9) {
            enc_raw_intensity = MICROSTEP_MAX_INTENSITY * 9;
        } else if (enc_raw_intensity < MICROSTEP_MAX_INTENSITY * -1) {
            enc_raw_intensity = MICROSTEP_MAX_INTENSITY * -1;
        }
    } else { // MODE_COLOR
        enc_raw_color = enc_raw_color + incr;
        if (enc_raw_color > MICROSTEP_MAX_COLOR * colmax) {
            enc_raw_color = MICROSTEP_MAX_COLOR * colmax;
        } else if (enc_raw_color < MICROSTEP_MAX_COLOR * colmin) {
            enc_raw_color = MICROSTEP_MAX_COLOR * colmin;
        }
    }

    // update rotval, and set the intensity and color variables depending on current mode
    if (incr != 0)  {
        // encoder was rotated
        if ((appmode == MODE_INTENSITY) && (enc_raw_intensity % MICROSTEP_MAX_INTENSITY == 0)) {
            intensity = enc_raw_intensity / MICROSTEP_MAX_INTENSITY;
            rotval = intensity;
        } else if ((appmode == MODE_COLOR) && (enc_raw_color % MICROSTEP_MAX_COLOR == 0)){ // MODE_COLOR
            color = enc_raw_color / MICROSTEP_MAX_COLOR;
            rotval = color;
        }
        printf("(%d,%d): ", color, intensity);
        set_dispval(rotval, SUPPRESS_DIG_LEFT); // updates 7-seg values for refresh
        set_lighting(0, color, intensity); // updates the PWM registers for the lighting
        if (intensity == -1) {
            printf("LEDs off\n");
        }
    }
}

void
board_init(void) {
    int i;

    // PWM config
    gpio_set_function(COLD_PIN_0, GPIO_FUNC_PWM);
    gpio_set_function(WARM_PIN_0, GPIO_FUNC_PWM);
    gpio_set_function(COLD_PIN_1, GPIO_FUNC_PWM);
    gpio_set_function(WARM_PIN_1, GPIO_FUNC_PWM);

    slice_num[0] = pwm_gpio_to_slice_num(COLD_PIN_0);
    slice_num[1] = pwm_gpio_to_slice_num(COLD_PIN_1);

    pwm_set_clkdiv(slice_num[0], CKDIV);
    pwm_set_clkdiv(slice_num[1], CKDIV);

    pwm_set_wrap(slice_num[0], PWM_MAX);  // pwm period of about 41 kHz
    pwm_set_wrap(slice_num[1], PWM_MAX);

    printf("Setting initial (color,brightness) (%d,%d)\n", color, intensity);
    set_lighting(0, color, intensity);
    set_lighting(1, color, -1); // set module 1 completely off
    printf("\n");

    pwm_set_enabled(slice_num[0], true);
    pwm_set_enabled(slice_num[1], true);

    // PWM settings in percent
    pwm_store[0][LED_TYPE_COLD] = 0;
    pwm_store[0][LED_TYPE_WARM] = 0;
    pwm_store[1][LED_TYPE_COLD] = 0;
    pwm_store[1][LED_TYPE_WARM] = 0;

    // LED on Pico board
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // 7-seg config, set all 7-seg pins to outputs:
    for (i = 0; i < 8; i++) {
        gpio_init(SEG_PIN[i]);
        gpio_set_dir(SEG_PIN[i], GPIO_OUT);
        gpio_put(SEG_PIN[i], 0);
    }
    for (i = 0; i < 2; i++) { // set the common cathode control pins to outputs:
        gpio_init(DIG_PIN[i]);
        gpio_set_dir(DIG_PIN[i], GPIO_OUT);
        gpio_put(DIG_PIN[i], 0);
    }

    // config for 7-seg refresh; set up a repeating timer
    add_repeating_timer_ms(3, seg_refresh_cb, NULL, &seg_refresh_timer);

    // button for input
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_set_pulls(BUTTON_PIN, true, false); // pullup enabled
    // encoder
    gpio_init(ENC_A_PIN);
    gpio_set_dir(ENC_A_PIN, GPIO_IN);
    gpio_disable_pulls(ENC_A_PIN);
    gpio_init(ENC_B_PIN);
    gpio_set_dir(ENC_B_PIN, GPIO_IN);
    gpio_disable_pulls(ENC_B_PIN);
    // setup callback for input events (encoder and button)
    gpio_set_irq_enabled_with_callback(ENC_A_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &input_cb);
    gpio_set_irq_enabled(ENC_B_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_LEVEL_LOW, true);
}

void do_debounce(void) {
    if (bmenu_state == BMENU_PRESSED) {
        bmenu_state = BMENU_DEBOUNCING;
        debounce_count = 5;
    } else if (debounce_count > 0) {
        debounce_count--;
        if (debounce_count <= 0) {
            if (BUTTON_UNPRESSED) {
                bmenu_state = BMENU_IDLE;
            } else {
                debounce_count = 5;
            }
        }
    }
}

void
display_keypress_list(void) {
    printf("Keypress Commands List\n");
    printf("h   - display this help\n");
    printf("b   - cycle through brightness settings\n");
    printf("c/d - increase/decrease color temperature (colder/warmer)\n");
    printf("q/a - increase/decrease cold PWM by 5 percent\n");
    printf("w/s - increase/decrease warm PWM by 5 percent\n\n");
}

void
check_for_keypress_input(void) {
    int c;
    int pwmlevel;
    c = getchar_timeout_us(1000);
    if (c == PICO_ERROR_TIMEOUT) {
        return;
    }
    switch (c) {
        case 'h':
            print_title();
            display_keypress_list();
            break;
        case 'b':
            printf("\nbrightness\n");
            if (intensity < 9) {
                intensity++;
            } else {
                intensity = -1;
            }
            printf("(color,brightness) (%d,%d)\n", color, intensity);
            if (intensity == -1) {
                printf("LEDs off\n");
            }
            set_lighting(0, color, intensity);
            break;
        case 'c':
            printf("\ncolor temp (CCT)\n");
            if (color > colmin) {
                color--;
            } else {
                color = colmin;
            }
            printf("(color,brightness) (%d,%d)\n", color, intensity);
            set_lighting(0, color, intensity);
            break;
        case 'd':
            printf("\ncolor temp (CCT)\n");
            if (color < colmax) {
                color++;
            } else {
                color = colmax;
            }
            printf("(color,brightness) (%d,%d)\n", color, intensity);
            set_lighting(0, color, intensity);
            break;
        case 'q':
            pwmlevel = pwm_store[0][LED_TYPE_COLD];
            pwmlevel = pwmlevel + 5;
            if (pwmlevel > 100) {
                pwmlevel = 100;
            }
            pwm_store[0][LED_TYPE_COLD] = pwmlevel;
            printf("[0][COLD] = %d percent\n", pwmlevel);
            set_pwm_percent(0, LED_TYPE_COLD, pwmlevel);
            break;
        case 'a':
            pwmlevel = pwm_store[0][LED_TYPE_COLD];
            pwmlevel = pwmlevel - 5;
            if (pwmlevel < 0) {
                pwmlevel = 0;
            }
            pwm_store[0][LED_TYPE_COLD] = pwmlevel;
            printf("[0][COLD] = %d percent\n", pwmlevel);
            set_pwm_percent(0, LED_TYPE_COLD, pwmlevel);
            break;
        case 'w':
            pwmlevel = pwm_store[0][LED_TYPE_WARM];
            pwmlevel = pwmlevel + 5;
            if (pwmlevel > 100) {
                pwmlevel = 100;
            }
            pwm_store[0][LED_TYPE_WARM] = pwmlevel;
            printf("[0][WARM] = %d percent\n", pwmlevel);
            set_pwm_percent(0, LED_TYPE_WARM, pwmlevel);
            break;
        case 's':
            pwmlevel = pwm_store[0][LED_TYPE_WARM];
            pwmlevel = pwmlevel - 5;
            if (pwmlevel < 0) {
                pwmlevel = 0;
            }
            pwm_store[0][LED_TYPE_WARM] = pwmlevel;
            printf("[0][WARM] = %d percent\n", pwmlevel);
            set_pwm_percent(0, LED_TYPE_WARM, pwmlevel);
            break;
        default:
            break;
    }
}

// ************ main function *******************
int main(void) {
    int i;

    // initialize stdio and wait for USB CDC connect
    stdio_init_all();
    sleep_ms(1000); // could remove this after debugging, or keep it in

    // print welcome message on the USB UART
    print_title();

    led_tables_init(); // build up the LED tables
    board_init(); // initialize all GPIO and PWM
    PICO_LED_ON;
    display_keypress_list(); // print helpful information

    // set initial value on the 7-seg display
    if (appmode == MODE_INTENSITY) {
        rotval = intensity;
    } else {
        rotval = color;
    }
    set_dispval(rotval, SUPPRESS_DIG_LEFT); // updates 7-seg values for refresh

    while (FOREVER) {
        do_debounce();
        check_for_keypress_input();

        PICO_LED_OFF;
        sleep_ms(20);
        PICO_LED_ON;
        sleep_ms(20);
    }
}


