/*
    external_driver_4x.h
    Part of Grbl_ESP32

    Pin assignments for the buildlog.net 4-axis external driver board
    https://github.com/bdring/4_Axis_SPI_CNC

    2018    - Bart Dring
    2020    - Mitch Bradley

    Grbl_ESP32 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Grbl is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grbl_ESP32.  If not, see <http://www.gnu.org/licenses/>.
*/

#define MACHINE_NAME            "External 4 Axis Driver Board V2"

#ifdef N_AXIS
        #undef N_AXIS
#endif
#define N_AXIS 4

#define X_STEP_PIN              GPIOout(0)
#define X_DIRECTION_PIN         GPIOout(2)
#define Y_STEP_PIN              GPIOout(26)
#define Y_DIRECTION_PIN         GPIOout(15)
#define Z_STEP_PIN              GPIOout(27)
#define Z_DIRECTION_PIN         GPIOout(33)
#define A_STEP_PIN              GPIOout(12)
#define A_DIRECTION_PIN         GPIOout(14)
#define STEPPERS_DISABLE_PIN    GPIOout(13)

/*
#define SPINDLE_TYPE            SPINDLE_TYPE_PWM // only one spindle at a time
*/

#define SPINDLE_OUTPUT_PIN      GPIOout(25)
#define SPINDLE_ENABLE_PIN      GPIOout(22)


#define SPINDLE_TYPE            SPINDLE_TYPE_HUANYANG // only one spindle at a time
#define HUANYANG_TXD_PIN        GPIO_NUM_17
#define HUANYANG_RXD_PIN        GPIO_NUM_4
#define HUANYANG_RTS_PIN        GPIO_NUM_16

#define X_LIMIT_PIN             GPIOin(34)
#define Y_LIMIT_PIN             GPIOin(35)
#define Z_LIMIT_PIN             GPIOin(36)

#if (N_AXIS == 3)
        #define LIMIT_MASK      B0111
#else
        #define A_LIMIT_PIN     GPIO_NUM_39
        #define LIMIT_MASK      B1111
#endif

#define PROBE_PIN               GPIOin(32)
#define COOLANT_MIST_PIN        GPIOout(21)
