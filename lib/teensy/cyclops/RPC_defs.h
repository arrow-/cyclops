/** @file RPC_defs.h
    @brief Defines a few constants of the RPC Format Specification.
           This file is part of CL.
    
    @page rpc-desc RPC Format Description

    @tableofcontents

    @section rpc-overview Overview
    There are two kinds of Serial packets, single byte and multi byte.
    The size of a packet is determined by MSB of header and the ``command`` field.

    The teensy will respond to all serial commands that it reads, see the section on @ref return-codes "Return Codes"

    @section S-header-desc Single Byte Packets
    Single byte Headers are distinguished from other headers by the MSB bit. For
    Single byte headers, this bit is always set.
    
    Field       | Bits  | Description
    ----------- | ----- | -----------
    Reserved    | [7]   | Always ``1``.
    ``channel`` | [6:3] | This is a bitmask and determines if "command" is applied on the Channel ``x``
    ``command`` | [2:0] | The command field
    
    @subsection S-cmd-desc Command descriptions
    
    command[2:0] | Name     | Effect                                                                                                                                                                                                   | Invoke in System State                |
    ------------ | -------  | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------- |
    ``000``      | start    | Launch Waveform generation.                                                                                                                                                                              | ``Expt. Active``                      |
    ``001``      | stop     | Pause Waveform generation.                                                                                                                                                                               | ``Expt. Active``                      |
    ``010``      | reset    | Reset selected sources. @attention The system is *not* reset to _initial configuration_!                                                                                                                 | ``Expt. Active``                      |
    ``011``      | swap     | Swap the Cyclops instances of the 2 high ``channel`` bits. @attention Picks the two Lowest Significant bits (in case more than 2 bits are high).                                                         | ``Expt. Active``                      |
    ``100``      | launch   | Launch the experiment main-loop.                                                                                                                                                                         | ``Expt. notActive``                   |
    ``101``      | end      | Stop the experiment main-loop. The teensy will enter a restricted mode, responding only to some Single byte commands: [launch, identity]. For all commands, an error code will be returned.              | ``Expt. Active``                      |
    ``110``      | test     | Tests the selected LED channel (provided it is connected!) with a TEST signal waveform. @attention Tests only (one) channel, the one with the lowest ID, i.e. ``CH0`` is tested if bit-mask is ``1101``. | ``Expt. notActive``                   |
    ``111``      | identity | Send device description.                                                                                                                                                                                 | ``Expt. Active``, ``Expt. notActive`` |
    
    @section M-header-desc Multi Byte Packets
    Packet is formed by concatenating the header with argument bytes. These must be invoked _only when_ "Experiment is Active".

    Field       | Bits  | Description
    ----------- | ----- | -----------
    Reserved    | [7]   | Always ``0``.
    ``channel`` | [6:5] | Command is appplied on Channel ``channel[1:0]``
    ``command`` | [4:0] | The command field.

    @subsection M-cmd-desc Command descriptions

    command[4:0]  | Name               | Size(Bytes) | Effect
    ------------- | ------------------ | ----------- | --------
    ``00000``     | change_source_l    | 2           | Changes Source instance to the one reffered by @ref src-id-sec. Mode is set to ``LOOPBACK``.
    ``00001``     | change_source_o    | 2           | Changes Source instance to the one reffered by @ref src-id-sec. Mode is set to ``ONE_SHOT``.
    ``00010``     | change_source_n    | 3           | Changes Source instance to the one reffered by @ref src-id-sec. Mode is set to ``N_SHOT``. \f$N\f$ is set to @ref shot_cycle "shot_cycle".
    ``00011``     | change_time_period | 5           | Set time period of updates @attention Works only if Source::holdTime is a constant!
    ``00100``     | time_factor        | 5           | Scale Source::holdTime values by this factor. \f$\in [0, \infty)\f$.
    ``00101``     | voltage_factor     | 5           | Scale Source::getVoltage values by this factor. \f$\in [0, \infty)\f$.
    ``00110``     | voltage_offset     | 3           | Add this DC offset level to Source::getVoltage values. \f$\in [0, \infty)\f$.
    ``00111``     | square_on_time     | 5           | Set squareSource pulse "ON" time.
    ``01000``     | square_off_time    | 5           | Set squareSource pulse "OFF" time.
    ``01001``     | square_on_level    | 3           | Set squareSource pulse "ON" voltage.
    ``01010``     | square_off_level   | 3           | Set squareSource pulse "OFF" voltage.
    
    @note       Voltage scaling can also be manually accomplished by the
                tweaking the GAIN knob on Cyclops Front Panel.

    @subsubsection src-id-sec src_id
    Each Source has a unique ID which is internally used by Task. The OE plugin
    might use the number just as a reference. @sa Source::src_id
    
    @subsection M-arg-byte-desc Argument Bytes

    | Command Name       | Argument[0]   | Argument[1]      |
    | ------------------ | ------------- | ---------------- |
    | change_source_l    | uint8  src_id |                  |
    | change_source_o    | uint8  src_id |                  |
    | change_source_n    | uint8  src_id | @anchor shot_cycle uint8 shot_cycle |
    | change_time_period | uint32 val    |                  |
    | time_factor        | float  val    |                  |
    | voltage_factor     | float  val    |                  |
    | voltage_offset     | int16  val    |                  |
    | square_on_time     | uint32 val    |                  |
    | square_off_time    | uint32 val    |                  |
    | square_on_level    | uint16 val    |                  |
    | square_off_level   | uint16 val    |                  |

    @subsection return-codes Error and Success Codes

    These are the _codes_ which will be returned by the teensy when an RPC command is recieved by it.

    For each RPC command,

    * a **Success Code** is returned _only when_, command can be executed in the current system state AND device performed the intended action successfully.
    * an **Error Code** is returned when device failed to execute it OR command _cannot_ be executed in the current system state.

    ``EA`` _stands for_ **Experiment is Active**

    ``notEA`` _stands for_ **Experiment is Inactive**

    @subsubsection RC-success Success Codes

    Success codes never start with first nibble HIGH ``~(0xFX)``.

    | Name     | Upon recieving...                                                  | Action when ``EA``                                            | Action when ``notEA``                                  | Success Code |
    | -------- | ------------------------------------------------------------------ | ------------------------------------------------------------- | ------------------------------------------------------ | ------------ |
    | LAUNCH   | ``SB.launch``                                                      | **No Action**                                                 | **Starts main-loop**, thereby starting the experiment. | ``0x00``     |
    | END      | ``SB.end``                                                         | **Breaks out of mainloop**, resets sources, grounds all LEDs. | **No Action**                                          | ``0x01``     |
    | SB.DONE  | ``SB.*`` (except ``launch``, ``end``, ``identity``, ``test``)      | Suitable action is performed.                                 | **No Action**                                          | ``0x10``     |
    | IDENTITY | ``SB.identity``                                                    | Identification Info is returned.                              | Identification Info is returned.                       | ``0x11``     |
    | TEST     | ``SB.test``                                                        | **No Action**                                                 | A TEST waveform is run on the channel.                 | ``0x12``     |
    | MB.DONE  | ``MB.*``                                                           | Suitable action is performed.                                 | **No Action**                                          | ``0x20``     |
    
    @subsubsection RC-error Error Codes
    
    Error codes generally start with first nibble HIGH ``(0xFX)``

    | Name           | Reason                                                                                                                     | ...while in state   | Code     |
    | -------------- | -------------------------------------------------------------------------------------------------------------------------- | ------------------- | -------- |
    | RPC_FAIL_EA    | ``SB.*`` or ``MB.*`` failed due to either inability to perform task, or task cannot be done when experiment is _Live_.     | only when ``EA``    | ``0xf0`` |
    | RPC_FAIL_notEA | ``SB.*`` or ``MB.*`` failed due to either inability to perform task, or task cannot be done when experiment is _Not Live_. | only when ``notEA`` | ``0xf1`` |

    @sa Task

    @author Ananya Bahadur
*/

#ifndef CL_RPC_DEFS_H
#define CL_RPC_DEFS_H

#define RPC_HEADER_SZ   1
#define RPC_MAX_ARGS    4
#define RPC_IDENTITY_SZ 26;

// Single Byte Definitions
#define SB_NUM_CMD       8
#define SB_CHANNEL_MASK  0x78 // channel[6:3]
#define SB_CMD_MASK      0x07 // command[2:0]
#define SB_CHANNEL_SHIFT 0x3
#define SB_CMD_SHIFT     0x0
#define EXTRACT_SB_CHANNELS(s_header_byte) (((uint8_t)s_header_byte & SB_CHANNEL_MASK) >> SB_CHANNEL_SHIFT)
#define EXTRACT_SB_CMD(s_header_byte)      (((uint8_t)s_header_byte & SB_CMD_MASK) >> SB_CMD_SHIFT)

// Multi Byte Definitions
#define MB_NUM_CMD       11
#define MB_CHANNEL_MASK  0x60 // channel[6:5]
#define MB_CMD_MASK      0x1f // command[4:0]
#define MB_CHANNEL_SHIFT 0x5
#define MB_CMD_SHIFT     0x0
#define EXTRACT_MB_CHANNEL(m_header_byte) ((m_header_byte & MB_CHANNEL_MASK) >> MB_CHANNEL_SHIFT)
#define EXTRACT_MB_CMD(m_header_byte)     ((m_header_byte & MB_CMD_MASK) >> MB_CMD_SHIFT)

// Success Codes
#define RPC_SUCCESS_LAUNCH   0x00
#define RPC_SUCCESS_END      0x01
#define RPC_SUCCESS_SB_DONE  0x10
#define RPC_SUCCESS_IDENTITY 0x11
#define RPC_SUCCESS_TEST     0x12
#define RPC_SUCCESS_MB_DONE  0x20

// Error Codes
#define RPC_FAILURE_EA       0xf0
#define RPC_FAILURE_notEA    0xf1

const static uint8_t multi_sz_map[MB_NUM_CMD] = {
  2, 2, 3, 5,
  5, 5, 3, 5,
  5, 3, 3
};

/**
 * @brief      Gets the packet size or a given "command".
 *
 * @param[in]  header_byte  The header byte
 *
 * @return     The packet size.
 */
inline uint8_t getPacketSize(uint8_t header_byte){
  if (header_byte & 0x80) return 1;
  return multi_sz_map[EXTRACT_MB_CMD(header_byte)];
}
#endif
