/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * feature/runout.cpp - Runout sensor support
 */

#include "../inc/MarlinConfigPre.h"

#if HAS_FILAMENT_SENSOR

#include "runout.h"

FilamentMonitor runout;

bool FilamentMonitorBase::enabled = true,
     FilamentMonitorBase::filament_ran_out;  // = false

#if ENABLED(HOST_ACTION_COMMANDS)
  bool FilamentMonitorBase::host_handling; // = false
#endif

#if ENABLED(TOOLCHANGE_MIGRATION_FEATURE)
  #include "../module/tool_change.h"
  #define DEBUG_OUT ENABLED(DEBUG_TOOLCHANGE_MIGRATION_FEATURE)
  #include "../core/debug_out.h"
#endif

#if HAS_FILAMENT_RUNOUT_DISTANCE
  float RunoutResponseDelayed::runout_distance_mm = FILAMENT_RUNOUT_DISTANCE_MM;
  volatile float RunoutResponseDelayed::runout_mm_countdown[NUM_RUNOUT_SENSORS];
  #if ENABLED(FILAMENT_MOTION_SENSOR)
    uint8_t FilamentSensorEncoder::motion_detected;
  #endif
#else
  int8_t RunoutResponseDebounced::runout_count[NUM_RUNOUT_SENSORS]; // = 0
#endif

//
// Filament Runout event handler
//
#include "../MarlinCore.h"
#include "../feature/pause.h"
#include "../gcode/queue.h"

#if ENABLED(HOST_ACTION_COMMANDS)
  #include "host_actions.h"
#endif

#if ENABLED(EXTENSIBLE_UI)
  #include "../lcd/extui/ui_api.h"
#elif ENABLED(DWIN_CREALITY_LCD_ENHANCED)
  #include "../lcd/e3v2/enhanced/dwin.h"
#endif

void event_filament_runout(const uint8_t extruder) {

  if (did_pause_print) return;  // Action already in progress. Purge triggered repeated runout.

  #if ENABLED(TOOLCHANGE_MIGRATION_FEATURE)
    if (migration.in_progress) {
      DEBUG_ECHOLNPGM("Migration Already In Progress");
      return;  // Action already in progress. Purge triggered repeated runout.
    }
    if (migration.automode) {
      DEBUG_ECHOLNPGM("Migration Starting");
      if (extruder_migration()) return;
    }
  #endif

  #if TRONXY_UI
    if (TERN0(DUAL_X_CARRIAGE, idex_is_duplicating())
    || TERN0(MULTI_NOZZLE_DUPLICATION, extruder_duplication_enabled))
    {
      if(READ(FIL_RUNOUT_PIN))out_extruder = 0;
      else out_extruder = 1;
    }
    else out_extruder = extruder;
    yevt.addEvt(TASK_SEQUENCE,screenFilament);
    yevt.addEvt(TASK_SEQUENCE,popupFilamentOut);
  #else
    TERN_(EXTENSIBLE_UI, ExtUI::onFilamentRunout(ExtUI::getTool(extruder)));
    TERN_(DWIN_CREALITY_LCD_ENHANCED, DWIN_FilamentRunout(extruder));
  #endif

  #if ANY(HOST_PROMPT_SUPPORT, HOST_ACTION_COMMANDS, MULTI_FILAMENT_SENSOR)
    const char tool = '0' + TERN0(MULTI_FILAMENT_SENSOR, extruder);
  #endif

  //action:out_of_filament
  #if ENABLED(HOST_PROMPT_SUPPORT)
    host_action_prompt_begin(PROMPT_FILAMENT_RUNOUT, PSTR("FilamentRunout T"), tool);
    host_action_prompt_show();
  #endif

  const bool run_runout_script = !runout.host_handling;

  #if ENABLED(HOST_ACTION_COMMANDS)
    if (run_runout_script
      && ( strstr(FILAMENT_RUNOUT_SCRIPT, "M600")
        || strstr(FILAMENT_RUNOUT_SCRIPT, "M125")
        || TERN0(ADVANCED_PAUSE_FEATURE, strstr(FILAMENT_RUNOUT_SCRIPT, "M25"))
      )
    ) {
      host_action_paused(false);
    }
    else {
      // Legacy Repetier command for use until newer version supports standard dialog
      // To be removed later when pause command also triggers dialog
      #ifdef ACTION_ON_FILAMENT_RUNOUT
        host_action(PSTR(ACTION_ON_FILAMENT_RUNOUT " T"), false);
        SERIAL_CHAR(tool);
        SERIAL_EOL();
      #endif

      host_action_pause(false);
    }
    SERIAL_ECHOPGM(" " ACTION_REASON_ON_FILAMENT_RUNOUT " ");
    SERIAL_CHAR(tool);
    SERIAL_EOL();
  #endif // HOST_ACTION_COMMANDS

  if (run_runout_script) {
    #if MULTI_FILAMENT_SENSOR
      char script[strlen(FILAMENT_RUNOUT_SCRIPT) + 4];
      sprintf_P(script, PSTR(FILAMENT_RUNOUT_SCRIPT " T%c"), tool);
      #if ENABLED(FILAMENT_RUNOUT_SENSOR_DEBUG)
        SERIAL_ECHOLNPGM("Runout Command: ", script);
      #endif
      queue.inject(script);
    #else
      #if ENABLED(FILAMENT_RUNOUT_SENSOR_DEBUG)
        SERIAL_ECHOPGM("Runout Command: ");
        SERIAL_ECHOLNPGM(FILAMENT_RUNOUT_SCRIPT);
      #endif
      queue.inject_P(PSTR(FILAMENT_RUNOUT_SCRIPT));
    #endif
  }
}

#endif // HAS_FILAMENT_SENSOR
