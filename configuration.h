/**
 * @file configuration.h
 * @author Christof Menzenbach
 * @date 9 Feb 2018
 * @brief Configuration of touch inputs, touch thresholds and communication interval.
 *
 */
 
#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#define BUTTON_L T6
#define BUTTON_LM T7
#define BUTTON_RM T9
#define BUTTON_R T8

#define BUTTON_L_TH 60
#define BUTTON_LM_TH 60
#define BUTTON_RM_TH 65
#define BUTTON_R_TH 70


// duration between BLE communication for dedicated hour of day for battery saving
//                                 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24
const uint8_t commIntervalls[] = {10, 10, 15, 20, 30, 30, 10,  2,  2,  5,  5, 10,  5,  3,  5,  5,  5,  5,  3,  4,  4,  4,  4,  4, 10};

#endif
