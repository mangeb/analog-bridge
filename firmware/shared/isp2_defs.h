/**
 *  Analog Bridge — ISP2 Protocol Definitions
 *
 *  Innovate Motorsports ISP2 serial protocol constants and
 *  voltage-to-unit conversion macros. Platform-agnostic.
 *
 *  Daisy-chain order (as wired on the 1969 Nova):
 *    SSI-4 #1: ch0=coolant, ch1=oilp
 *    LC-1  #1: wideband AFR bank 1
 *    LC-1  #2: wideband AFR bank 2
 *    SSI-4 #2: ch2=MAP, ch3=VSS
 */
#ifndef AB_ISP2_DEFS_H
#define AB_ISP2_DEFS_H

//----------------------------------------------------------------
// ISP2 Protocol Constants
//----------------------------------------------------------------
#define ISP2_BAUD          19200
#define ISP2_H_SYNC_MASK   0xA2   // High sync byte: bits 7,5,1 set
#define ISP2_L_SYNC_MASK   0x80   // Low sync byte: bit 7 set
#define ISP2_LC1_FLAG      0x40   // Bit 14 in high byte = LC-1 sub-packet
#define ISP2_MAX_WORDS     16     // Max words per packet
#define ISP2_TIMEOUT_MS    200    // Resync if payload stalls this long

//----------------------------------------------------------------
// Aux channel voltage-to-unit conversions
// Tune for your specific sensors
//----------------------------------------------------------------

// Coolant temp: placeholder linear (TODO: Steinhart-Hart for thermistor)
#define AUX_COOLANT_F(v)   ((v) * 100.0f)

// Oil pressure: 0.5-4.5V = 0-100 PSI linear sender
#define AUX_OILP_PSI(v)    (((v) - 0.5f) * 25.0f)

// MAP: 1-bar sensor (0-5V = -14.7 to +14.7 inHg)
#define AUX_MAP_INHG(v)    ((v) * 5.858f - 14.696f)

//----------------------------------------------------------------
// VSS (Vehicle Speed Sensor) calibration
// Derived from: 235/70R15 tire, 3.73 final drive, 17-tooth reluctor
//   Wheel circumference: 2.23 m (710 mm dia)
//   VSS @ 100 mph: 1271 Hz → 12.71 Hz per MPH
// SSI-4 frequency mode: 0-5V maps linearly to 0..SSI4_VSS_FREQ_MAX Hz
//----------------------------------------------------------------
#define SSI4_VSS_FREQ_MAX  1500.0f  // Hz — SSI-4 configured max
#define VSS_HZ_PER_MPH     12.71f   // Hz per MPH for this drivetrain
#define AUX_VSS_MPH(v)     (((v) / 5.0f * SSI4_VSS_FREQ_MAX) / VSS_HZ_PER_MPH)

#endif // AB_ISP2_DEFS_H
