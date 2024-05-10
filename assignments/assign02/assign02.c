#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h" // The WS2812 driver code
#include "hardware/watchdog.h"

// needed for "implicit declaration of function" error, consider doing this for all?
int checkAnswer();
void initializeInputArray();
void ask_question();
int compute_Accuracy();
void print_Stats();

/*********************************************************************
 * @defgroup macros Macros
 * 
 * @brief Macros for use in the rest of the program
 * 
 * @{
*********************************************************************/

/**
 * @brief Sets the code to use the RGBW format
*/
#define IS_RGBW true

/**
 * @brief Sets the number of pixels used to 1 as there is only one WS2812 device in the chain
*/
#define NUM_PIXELS 1 

/**
 * @brief Sets the GPIO pin the WS2812 will be connected to
*/
#define WS2812_PIN 28

/**
 * @brief Introduction text to be printed to the console
 * 
 * @details A macro containing a string of text to be printing to the console.
 *          Storing this as a macro potentially allows the compiler to optimise how it's
 *          stored (e.g. storing it in flash memory instead of RAM). This saves memory,
 *          ensures consistency, and makes it easier to change the text later on if need be
 *          in one location. Can be accessed from C or assembly code without compatibility issues.
*/
#define WELCOME "************************************************************************************\n"\
"*                                                                                  *\n"\
"*          LEARN MORSE CODE - An interactive game on the RP2040                    *\n"\
"*                                                                                  *\n"\
"************************************************************************************\n"\
"*                                                                                  *\n"\
"*  Welcome to our game! This code was written by Group 15 in CSU23021              *\n"\
"*                                                                                  *\n"\
"*                    Some instructions to get you started:                         *\n"\
"*                                                                                  *\n"\
"*1. Choose your level by entering the morse code for the level number (don't worry *\n"\
"*   yet, these are provided for now.                                               *\n"\
"*2. Enter a (dot) by pressing the GP21 button on the MAKER-PI-PICO board for a     *\n"\
"*   short duration (255ms or less), and enter a (dash) by pressing the GP21        *\n"\
"*   button on the MAKER-PI-PICO board for a longer duration (longer than 255ms)    *\n"\
"*3. You will begin with 3 lives. Each time a correct answer is input, you will gain*\n"\
"*   a life (with 3 lives maximum), and each time your answer is incorrect, a life  *\n"\
"*   will be deducted. The RGB LED will indicate how many lives you have got left:  *\n"\
"*                           GREEN: 3 lives                                         *\n"\
"*                           YELLOW: 2 lives                                        *\n"\
"*                           ORANGE: 1 life                                         *\n"\
"*                           RED: 0 lives, GAME OVER                                *\n"\
"*                                                                                  *\n"\
"************************************************************************************\n"\
"*                                                                                  *\n"\
"*     please select a level using the GP21 button the MAKER-PI-PICO board!!        *\n"\
"*                                                                                  *\n"\
"*                     \".----\"   LEVEL 1 - chars (Easy)                             *\n"\
"*                     \"..---\"   LEVEL 2 - chars (Medium)                           *\n"\
"*                     \"...--\"   LEVEL 3 - words (Medium)                           *\n"\
"*                     \"....-\"   LEVEL 4 - words (Hard)                             *\n"\
"*                                                                                  *\n"\
"************************************************************************************\n"

// the frame above looks off but it needs to be extended to look straight due to the \"

/** @} */


/*********************************************************************
 * @defgroup gpio-init GPIO Initialisation
 * 
 * @brief Wrapper functions for interacting with the Pico SDK's GPIO
 * functionality.
 * 
 * @{
*********************************************************************/

/**
 * @brief Initialises a pin.
 * 
 * @param pin The pin to be initialised.
*/
void asm_gpio_init(uint pin)
{
    gpio_init(pin);
}

/**
 * @brief Sets the direction of a pin.
 * 
 * @param pin The pin to be initialised.
 * @param out The direction to be set: true for out, false for in.
*/
void asm_gpio_set_dir(uint pin, bool out)
{
    gpio_set_dir(pin, out);
}

/**
 * @brief Returns the value of a pin.
 * 
 * @param pin The pin to query.
 * 
 * @return The value of said pin.
*/ 
bool asm_gpio_get(uint pin)
{
    return gpio_get(pin);
}

/**
 * @brief Sets the value of a pin.
 * 
 * @param pin The pin to modify.
 * @param value The value to set the pin to.
*/
void asm_gpio_put(uint pin, bool value)
{
    gpio_put(pin, value);
}

/**
 * @brief Enables falling-edge interrupt for a pin.
 * 
 * @param pin The pin to modify.
*/
void asm_gpio_set_irq_fall(uint pin)
{
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
}

/**
 * @brief Enables rising-edge interrupt for a pin.
 * 
 * @param pin The pin to modify.
*/
void asm_gpio_set_irq_rise(uint pin) {
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
}

/**
 * @brief Initialise the watchdog timer.
*/
void watchdog_init()
{
    if (watchdog_caused_reboot())
    { // check if reboot was due to watchdog timeout
        printf("\nNo input was detected for timeout period. Rebooted by watchdog\n");
    }
    if (watchdog_enable_caused_reboot())
    { // check if enabling watchdog caused reboot
        printf("\nChip reboot due to watchdog enable\n");
    }
    watchdog_enable(0x7fffff, 1); // enable the watchdog timer to max time (approx 8.3 secs), pause on debug
    watchdog_update();
}

/** @} */


/*********************************************************************
 * @defgroup rgb-led RGB LED
 * 
 * @brief Functions for interacting with and controlling the RGB LED.
 * 
 * @{
*********************************************************************/

/**
 * @brief Pushes the 32-bit RGB colour value to the LED.
 * 
 * @details Wrapper function used to call the underlying PIO
 *         function that pushes the 32-bit RGB colour value
 *         out to the LED serially using the PIO0 block. The
 *         function does not return until all of the data has
 *         been written out.
 *
 * @param pixel_grb The 32-bit colour value generated by urgb_u32()
 */
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
 * @brief Generates a 32-bit RGB value from three 8-bit parameters.
 * 
 * @details Function to generate an unsigned 32-bit composite RGB value
 *          by combining the individual 8-bit paramaters for red, green 
 *          and blue together in the right order.
 *
 * @param r     The 8-bit intensity value for the red component
 * @param g     The 8-bit intensity value for the green component
 * @param b     The 8-bit intensity value for the blue component
 * @return uint32_t Returns the resulting composit 32-bit RGB value
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

/**
 * @brief Sets the LED to a colour depending on the number of lives remaining
 * 
 * @details Will set the LED to off if the player has no lives remaining,
 *          to red if the player has one life remaining,
 *          to orange if the player has two lives remaining,
 *          and to green if the player has three lives remaining.
 * 
 * @param remaining_lives An integer containing the number of lives remaining, should be in the range [0,3].
*/
void RGB_Lives_Display(int remaining_lives)
{
    if (remaining_lives == 0){
        put_pixel(urgb_u32(0x7F, 0x00, 0x00)); // Set the RGB LED color to red
        printf("Game over!!!\n");
    }
    else if (remaining_lives == 1){
        put_pixel(urgb_u32(0xFF, 0x55, 0x00)); // Set the RGB LED color to orange
        printf("Warning! 1 life remaining.\n");
    }
    else if (remaining_lives == 2){
        put_pixel(urgb_u32(0xFF, 0xA5, 0x00)); // Set the RGB LED color to yellow
        printf("2 lives remaining.\n");
    }
    else if (remaining_lives == 3){
        put_pixel(urgb_u32(0x00, 0x7F, 0x00)); // Set the RGB LED color to green
        printf("Congratulations! 3 lives remaining.\n");
    }
    else{
        printf("Throw error detection.\n"); // Throw an error as remaining_lives is invalid
    }
}

/** @} */


/*********************************************************************
 * @defgroup press-timing Button press timing
 * 
 * @brief Functions for determining how long the button was held for
 * 
 * @{
*********************************************************************/

/**
 * @brief Returns a timestamp in milliseconds.
 * 
 * @return The timestamp in milliseconds.
*/
int get_time_in_ms()
{
    absolute_time_t time = get_absolute_time();
    return to_ms_since_boot(time);
}

/**
 * @brief Calculates the length of time the button was held for.
 * 
 * @param end_time The time the button was released.
 * @param start_time The time the button was pressed.
 * 
 * @return The difference between the two, in milliseconds.
*/
int get_time_difference(int end_time, int start_time)
{
    return (end_time - start_time);
}

/** @} */


/*********************************************************************
 * @defgroup global-vars Global Variables
 * 
 * @brief A selection of global variables used for the rest of the 
 *        program
 * 
 * @{
*********************************************************************/

/**
 * @brief The player's current number of lives, ranging from 0 to 3.
*/
int Remaining_Lives = 3;

/**
 * @brief The number of lives the player has gained over the course of the game.
*/
int Gained_Lives = 0;

/**
 * @brief The number of correct answers in a row achieved by the player, reset to 0
 *        when the player provides an incorrect answer.
*/
int Correct_Counter = 0;

/**
 * @brief The total number of correct answers achieved by the player.
*/
int Correct_Total = 0;

/**
 * @brief The level the player is currently on.
*/
int levelIndex = 0;

/**
 * @brief The user's inputs.
*/
char *userInput;

/**
 * @brief Stores the length of the user input array.
*/
int userInputLength;

/**
 * @brief Stores the user's most recent input.
*/
int lastInput;

/**
 * @brief Stores the user's input.
*/
char inputSequence[20];

/** 
 * @brief The length of the user's input sequence.
*/
int i = 0;

/**
 * @brief used to check index of question between subroutines
*/
int tmpIndex = 0;

/**
 * @brief Indicates whether the user's input is complete.
*/
int inputComplete = 0;

/**
 * @brief The number of questions the player has answered incorrectly.
*/
int Wrong_Counter = 0;

/**
 * @brief The level the user inputs.
*/
int select_level_input = 0;

/** @} */


/*********************************************************************
 * @defgroup morse-table Morse Code Table
 * 
 * @brief The code necessary for interpreting morse code.
 * 
 * @{
*********************************************************************/

/**
 * @brief The number of acceptable morse code characters.
*/
#define table_SIZE 36

/**
 * @brief Contains a character in morse code.
 * 
 * @details Each character has two elements:
 *          the character 'letter', which stores the morse code 
 *          equivalent in the Latin alphabet, and the character
 *          array 'code', which stores the sequence of dots and dashes.
*/
typedef struct morsecode{
    char letter;
    char *code;
} morsecode;

/**
 * @brief The table for storing morse code characters.
*/
morsecode table[table_SIZE];

/**
 * @brief Initialises a table with each of the 36 characters in morse code.
*/
void morse_init(){

    table[0].letter = 'A';
    table[1].letter = 'B';
    table[2].letter = 'C';
    table[3].letter = 'D';
    table[4].letter = 'E';
    table[5].letter = 'F';
    table[6].letter = 'G';
    table[7].letter = 'H';
    table[8].letter = 'I';
    table[9].letter = 'J';
    table[10].letter = 'K';
    table[11].letter = 'L';
    table[12].letter = 'M';
    table[13].letter = 'N';
    table[14].letter = 'O';
    table[15].letter = 'P';
    table[16].letter = 'Q';
    table[17].letter = 'R';
    table[18].letter = 'S';
    table[19].letter = 'T';
    table[20].letter = 'U';
    table[21].letter = 'V';
    table[22].letter = 'W';
    table[23].letter = 'X';
    table[24].letter = 'Y';
    table[25].letter = 'Z';
    table[26].letter = '0';
    table[27].letter = '1';
    table[28].letter = '2';
    table[29].letter = '3';
    table[30].letter = '4';
    table[31].letter = '5';
    table[32].letter = '6';
    table[33].letter = '7';
    table[34].letter = '8';
    table[35].letter = '9';

    table[0].code = ".-";
    table[1].code = "-...";
    table[2].code = "-.-.";
    table[3].code = "-..";
    table[4].code = ".";
    table[5].code = "..-.";
    table[6].code = "--.";
    table[7].code = "....";
    table[8].code = "..";
    table[9].code = ".---";
    table[10].code = "-.-";
    table[11].code = ".-..";
    table[12].code = "--";
    table[13].code = "-.";
    table[14].code = "---";
    table[15].code = ".--.";
    table[16].code = "--.-";
    table[17].code = ".-.";
    table[18].code = "...";
    table[19].code = "-";
    table[20].code = "..-";
    table[21].code = "...-";
    table[22].code = ".--";
    table[23].code = "-..-";
    table[24].code = "-.--";
    table[25].code = "--..";
    table[26].code = "-----";
    table[27].code = ".----";
    table[28].code = "..---";
    table[29].code = "...--";
    table[30].code = "....-";
    table[31].code = ".....";
    table[32].code = "-....";
    table[33].code = "--...";
    table[34].code = "---..";
    table[35].code = "----.";
}

/**
 * @brief The morse code alphabet, containing A-Z and 0-9.
*/
static const char *alp[] = {
    ".-",    // A
    "-...",  // B
    "-.-.",  // C
    "-..",   // D
    ".",     // E
    "..-.",  // F
    "--.",   // G
    "....",  // H
    "..",    // I
    ".---",  // J
    "-.-",   // K
    ".-..",  // L
    "--",    // M
    "-.",    // N
    "---",   // O
    ".--.",  // P
    "--.-",  // Q
    ".-.",   // R
    "...",   // S
    "-",     // T
    "..-",   // U
    "...-",  // V
    ".--",   // W
    "-..-",  // X
    "-.--",  // Y
    "--..",  // Z
    "-----", // 0
    ".----", // 1
    "..---", // 2
    "...--", // 3
    "....-", // 4
    ".....", // 5
    "-....", // 6
    "--...", // 7
    "---..", // 8
    "----.", // 9
    "-.-. .- -", // CAT
    "-.-. --- -.. .", // CODE
    "-- .. -.-. .-. ---", // MICRO
    ".. -. - . .-..", // INTEL
    "--. .-. --- ..- .--.", // GROUP
};

/**
 * @brief The Latin alphabet, containing A-Z and 0-9.
*/
static const char *que[] = {
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "CAT",
    "CODE",
    "MICRO",
    "INTEL",
    "GROUP"
};

/** @} */


/***********************************************************************
 * @defgroup level-logic Level Logic
 * 
 * @brief Code to start the game and select levels.
 * 
 * @{
***********************************************************************/

/**
 * @brief Starts the game.
 * 
 * @details Runs after the level has been chosen.
 *          Sets the LED to blue.
 *          Initialises four global variables: Correct_Counter, Remaining_Lives,
 *          Wrong_Counter and Gained_lives.
*/
void gameStart()
{
    put_pixel(urgb_u32(0x00, 0x7F, 0x00)); // Set the RGB LED color to green
    Correct_Counter = 0;
    Remaining_Lives = 3;
    Wrong_Counter = 0;
    Gained_Lives = 0;
}

/**
 * @brief Sets the global variable 'select_level_input' to true.
*/
void level_select_true(){
    select_level_input = 1;
}

/**
 * @brief Sets the global variable 'select_level_input' to false.
*/
void level_select_false(){
    select_level_input = 0;
}


/**
 * @brief Determine if the input is a valid alphanumeric character
*/
void checkAlphanumeric(){
    int n = 0; 
    int check = 0;
    while(n <= 40){
        if (strcmp(inputSequence, alp[n])==0)
        {
            printf("\nIncorrect! you have actually input %s in morse code\n", que[n]);
            check = 1;
        }
        n++;
    }
    if (check == 0 ){
        printf("\n?\n");
    }
    
}

/**
 * @brief Determines what level has been selected based on the 
 *        value in 'inputSequence'.
 * 
 * @return An integer from 1-4 representing the level selected.
*/
int select_level(){

    int level;
    if (strcmp(inputSequence, alp[27])==0){    // return 1 for level 1
        printf("Level 1 selected");
        level = 1;
    }
    else if(strcmp(inputSequence, alp[28])==0){    // return 2 for level 2
        printf("Level 2 selected");
        level = 2;
    }
    else if(strcmp(inputSequence, alp[29])==0){    // return 3 for level 3
        printf("Level 3 selected");
        level = 3;
    }
    else if(strcmp(inputSequence, alp[30])==0){    // return 4 for level 4
        printf("Level 4 selected");
        level = 4;
    }
    else{
        level = 5;   // return 5 for invalid result
    }
    initializeInputArray();                  // reset input array for new inputs
    return level;

}

/**
 * @brief Progress to the next level if the player has all 5 answers correct.
*/
void progress_level()
{
    if(levelIndex < 4)
    {
        levelIndex++;
        printf("\ncongrats! You've progressed to the next level\n"\
                "you are now on level %d", levelIndex);
        Correct_Counter = 0;
        ask_question();
    }

    else    // they completed the game
    {
        printf( "\n\n");
        printf("||=======================================================||\n");
        printf("||    WooHoo Congratulations!!!! You beat the game!!!    ||\n");
        printf("||        Thank you for taking the time to play :)       ||\n");
        printf("||                                                       ||\n");
        printf("||               Here are your final stats               ||\n");
        printf("||=======================================================||\n");

        print_Stats();
    }

}


/**
 * @brief A random number generator.
 * 
 * @details Uses the rand() function to generate a random integer between its
 *          two integer parameters (inclusive).
 * 
 * @param low The lower bound for the generated number.
 * @param high The upper bound for the generated number.
 * 
 * @return The randomly generated number.
*/
int r(int low, int high)
{
    if (low > high)
        return high;
    srand(time(NULL));                          // set seed of random generator to current time so it is actually random
    return low + (rand() % (high - low + 1));  // rand() is pseudorandom in that it has predertimined random generation based on a seed
}

/**
 * @brief Ask a level 1 question.
 * 
 * @details Ask the user to reproduce a random character in morse code,
 *          providing the morse code inputs necessary.
 * 
 * @return The integer index of the character the user should guess.
*/
int level_1_question()
{
    levelIndex = 1;
    int tmp = r(0, 35);
    tmpIndex = tmp;
    printf("\n\n");
    printf("||================================================||\n");
    printf("||     Reproduce this character in morse code     ||\n");
    printf("                          %s                        \n", alp[tmp]);
    printf("                          %s                        \n", que[tmp]);
    printf("||================================================||\n\n");
    return tmp;
}

/**
 * @brief Ask a level 2 question.
 * 
 * @details Ask the user to reproduce a random character in morse code.
 * 
 * @return The integer index of the character the user should input.
*/
int level_2_question()
{
    levelIndex = 2;
    int tmp = r(0, 35);
    tmpIndex = tmp;
    printf("\n\n");
    printf("||================================================||\n");
    printf("||           What is this in morse code?          ||\n");
    printf("                         %c                         \n", table[tmp].letter);
    printf("||================================================||\n\n");
    return tmp;
}

/**
 * @brief Indicates what word to ask for a level 3 question.
*/
int level3QuestionIdx =0;

/**
 * @brief Ask a level 3 question.
 * 
 * @details Ask the user to reproduce a word in morse code,
 *          providing the morse code inputs necessary.
 * 
 * @return An integer that indicates what word was asked.
*/
int level_3_question()
{
    levelIndex = 3;
    int toReturn =0;
    if (level3QuestionIdx == 0) {
		toReturn=36;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"CAT\" in morse code\n");
		printf("-.-. .- -\n");
        printf("================================================\n\n");
		level3QuestionIdx = 1;
		return toReturn;
	}
	if (level3QuestionIdx == 1) {
		toReturn =37;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"CODE\" in morse code\n");
		printf("-.-. --- -.. .\n");
        printf("================================================\n\n");
		level3QuestionIdx = 2;
		return toReturn;
	}
	if (level3QuestionIdx == 2) {
		toReturn =38;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"MICRO\" in morse code\n");
		printf("-- .. -.-. .-. ---\n");
        printf("================================================\n\n");
		level3QuestionIdx = 3;
		return toReturn;
	}
	if (level3QuestionIdx == 3) {
		toReturn =39;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"INTEL\" in morse code\n");
		printf(".. -. - . .-..\n");
        printf("================================================\n\n");
		level3QuestionIdx = 4;
		return toReturn;
	}
	if (level3QuestionIdx == 4) {
		toReturn =40;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"GROUP\" in morse code\n");
		printf("--. .-. --- ..- .--.\n");
        printf("================================================\n\n");
		level3QuestionIdx = 0;
		return toReturn;
	}	
	return toReturn;
}

/**
 * @brief Indicates what word to ask for a level 4 question.
*/
int level4QuestionIdx =0;

/**
 * @brief Ask a level 4 question.
 * 
 * @details Ask the user to reproduce a word in morse code.
 * 
 * @return An integer that indicates what word was asked.
*/
int level_4_question()
{
    levelIndex = 4;
    int toReturn =0;
    if (level4QuestionIdx == 0) {
		toReturn=36;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"CAT\" in morse code\n");
        printf("================================================\n\n");
		level4QuestionIdx = 1;
		return toReturn;
	}
	if (level4QuestionIdx == 1) {
		toReturn =37;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"CODE\" in morse code\n");
        printf("================================================\n\n");
		level4QuestionIdx = 2;
		return toReturn;
	}
	if (level4QuestionIdx == 2) {
		toReturn =38;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"MICRO\" in morse code\n");
        printf("================================================\n\n");
		level4QuestionIdx = 3;
		return toReturn;
	}
	if (level4QuestionIdx == 3) {
		toReturn =39;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"INTEL\" in morse code\n");
        printf("================================================\n\n");
		level4QuestionIdx = 4;
		return toReturn;
	}
	if (level4QuestionIdx == 4) {
		toReturn =40;
		tmpIndex = toReturn;
		printf("\n\n");
        printf("================================================\n");
		printf("Reproduce the word \"GROUP\" in morse code\n");
        printf("================================================\n\n");
		level4QuestionIdx = 0;
		return toReturn;
	}	
	return toReturn;
}

/**
 * @brief Asks a question depending on what level you're on.
*/
void ask_question()
{
    if(levelIndex == 1)
    {
        level_1_question();
    }
    else if(levelIndex == 2)
    {
        level_2_question();
    }
    else if(levelIndex == 3)
    {
        level_3_question();
    }
    else if(levelIndex == 4)
    {
        level_4_question();
    }
}

/** 
 * @brief Gets the currently selected level.
 * 
 * @return An integer from 1-4 representing the currently selected level.
*/
int get_level()
{
    return levelIndex;
}

/**
 * @brief Sets the currently selected level.
 * 
 * @param level An integer from 1-4 representing the currently selected level.
*/
void set_level(int level)
{
    levelIndex = level;
}

/**
 * @brief Prints statistics for the user.
*/
void print_Stats()
{
    printf("\n");
        printf("||========================================||\n");
        printf("||             Statistics Table           ||\n");
        printf("               Correct times: %d          \n", Correct_Total);
        printf("               Wrong times: %d            \n", Wrong_Counter);
        printf("               Gained Lives: %d           \n", Gained_Lives);
        printf("               Accuracy: %d%%              \n", compute_Accuracy());
        printf("||========================================||\n");
        printf("\n");
}

/**
 * @brief Computes accuracy of guesses.
 * 
 * @return An integer accuracy of guesses.
*/
int compute_Accuracy()
{
    float Accuracy = (float)Correct_Total/(float)(Wrong_Counter + Correct_Total);
    int Accuracy_adjusted = (int)(Accuracy*100);
    return Accuracy_adjusted;
}


/**
 * @brief Checks if the level is complete, or if the player has lost the game.
 * 
 * @details If the user has provided five correct answers, this function will return 1.
 *          If the user has no lives remaining, this function will return 2, and print
 *          a message informing the user and a table of statistics about the user's progress.
 *          Otherwise, the function will return 0, indicating the level is not complete and the
 *          game is still ongoing.
 * 
 * @return An integer from 0-2 representing the state of the game.
*/
int check_level_complete()
{
    if (Correct_Counter == 5)
    {
        return 1;
    }
    if (Remaining_Lives == 0)
    {
        printf("\n");
        printf("||=================================||\n");
        printf("||                                 ||\n");
        printf("||      No Lives remaining         ||\n");
        printf("||  Sorry, you've lost the game    ||\n");
        printf("||                                 ||\n");
        printf("||=================================||\n");
        print_Stats();
        return 2;
    }
    return 0;
}

/**
 * @brief Updates global variables based on correctness of answer.
 * 
 * @param answer 1 if the answer is correct, 0 otherwise.
*/
void response(int answer)
{
    
        if(answer)  // if they got it correct
        {
            inputComplete = 1;  // value = 1 if answer is correct, otherwise remain 0 
            Correct_Counter++;
            Correct_Total++;
            if(Remaining_Lives < 3)
            {
                Remaining_Lives++;
                Gained_Lives++;
            }  
        }
        else
        {
            Remaining_Lives--;
            Wrong_Counter++;
            Correct_Counter = 0;        // reset counter, need to get 5 in a row
            if (levelIndex == 1 || levelIndex == 2) {           //ensures that checkAlphanumeric only occurs for levels 1 and 2
                 checkAlphanumeric(); 
            }
           
        }

}

/** @} */


/***********************************************************************
 * @defgroup read-inputs Reading inputs
 * 
 * @brief Code to read the user's inputs.
 * 
 * @{
***********************************************************************/

/**
 * @brief Initialises the variables used to store user input.
 * 
 * @details Initialises the 'inputSequence' character array.
 *          Sets 'inputComplete' and 'i' to 0.
*/
void initializeInputArray()
{
    int maxsize = 20;
    for (int j = 0; j < maxsize; j++)
    {
        inputSequence[j] = '\0';
    }
    inputComplete = 0;
    i = 0;
}

/**
 * @brief Handles dots, dashes, spaces, and end-level inputs.
 * 
 * @details If a 1 is input, adds a dot to input sequence and print stream.
 *          If a 2 is input, adds a dash to input sequence and print stream.
 *          If a 3 is input, adds a space to input sequence and print stream.
 *          If a 4 is input, determines if 'level_select_input' is true. 
 *          If it is false, it calls select_level() to select the next level.
 *          Else, it indicates the input is complete.
 * 
 * @param input An integer from 1-4 representing the user inputs.
*/
void detectInput(int input)
{
    if (input == 1)
    {                           // 1 = "."
        inputSequence[i] = '.'; // add '.' to the array
        i++;
        printf(".");
    }
    else if (input == 2)
    {                           // 2 = "-"
        inputSequence[i] = '-'; // add '-' to the array
        i++;
        printf("-");
    }
    else if (input == 3)
    {                           // 3 = " "
        inputSequence[i] = ' '; // add ' ' to the array
        i++;
        printf(" ");
    }
    // if 'select_level_input' is true, then we are currently in a level
    else if (input == 4 && select_level_input == 1){
        inputSequence[i-1] = '\0';  // 4 = "enter"

        int answer = checkAnswer();
        response(answer);
        initializeInputArray();
        // display_user_input();
        RGB_Lives_Display(Remaining_Lives);
        int completion_status = check_level_complete();
        if(completion_status == 0 && answer)
        {
            ask_question();
        }
        
        else if(completion_status == 1)
        {
            progress_level();
        }
    }

    // if 'select_level_input' is false, it means we have yet to select a level
    else if (input == 4 && select_level_input == 0){
        inputSequence[i-1] = '\0';  // 4 = "enter"
        
        level_select_true();        // start with assumption that they made a proper level selection, reduces code lines
        int level = select_level();

        if (level > 0 && level < 5)
        {
            set_level(level);
            gameStart();    // sets up level in general
            ask_question();
        }

        else                            // they did not properly select a level
        {
            level_select_false();       
        }
        
    }
}

/** @} */


/***********************************************************************
 * @defgroup check-answer Check answer
 * 
 * @brief Determine if an answer is valid.
 * 
 * @{
***********************************************************************/

/**
 * @brief Compares the user's input to the expected answer.
 * 
 * @return A 1 if the answer is correct, and a 0 if the answer is incorrect.
*/
int checkAnswer()
{
    if (strcmp(inputSequence, alp[tmpIndex])==0)
    {
        printf("\ncorrect! you have input %s\n", que[tmpIndex]);
        return 1; // return 1 if equal
    }
    else
    {
        return 0; // return 0 if not
    }
}

/**
 * @brief Checks what Latin alphabet each morse code sequence is equal to and returns it.
 * 
 * @return The index of the character the morse code sequence is equivalent to, or -1 if there
 *          is no equivalence.
*/
int checkMorseCode()
{
    for (int i = 0; i < table_SIZE; i++)
    {
        if (inputSequence == table[i].code)
        {
            return i;
        }
    }
    return -1; // return -1 if not equivalence
}

/** @} */


/**
 * @brief The main assembly entrance point. Must be declared before use.
*/
void main_asm();

/**
 * @brief Main function.
 * 
 * @details Initialises all I/O, initialises the morse code table,
 *          prints the welcome message, enters the ASM code.
*/ 

int main() {
    // Initialise all STDIO
    stdio_init_all();
    morse_init();
    watchdog_init();

    // Initialise the PIO interface with the WS2812 code (FOR RBG LED)
    // initialisePIO();
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, IS_RGBW);
    put_pixel(urgb_u32(0x00, 0x00, 0x7F)); // Set the RGB LED color to blue

    printf(WELCOME); // print welcome message

    watchdog_update();
    main_asm();
    return(0);
}
