#include "grbl.h"
#include "commands.h"

#include "SettingsDefinitions.h"
#include <map>
void settings_restore(uint8_t restore_flag) {
    #if defined(ENABLE_BLUETOOTH) || defined(ENABLE_WIFI)
        if (restore_flag & SETTINGS_RESTORE_WIFI_SETTINGS) {
            #ifdef ENABLE_WIFI
                    wifi_config.reset_settings();
            #endif
            #ifdef ENABLE_BLUETOOTH
                    bt_config.reset_settings();
            #endif
        }
    #endif
    if (restore_flag & SETTINGS_RESTORE_DEFAULTS) {
        for (Command *s = CommandsList; s; s = s->next()) {
            bool restore_startup = restore_flag & SETTINGS_RESTORE_STARTUP_LINES;
            if (!s->getWebuiName()) {
                const char *name = s->getName();
                if (restore_startup || ((strcmp(name, "N0") != 0) && (strcmp(name, "N1") == 0))) {
                    s->setDefault();
                }
            }
        }
        //transfer_settings();
        // TODO commit changes
    }
    if (restore_flag & SETTINGS_RESTORE_PARAMETERS) {
        uint8_t idx;
        float coord_data[N_AXIS];
        memset(&coord_data, 0, sizeof(coord_data));
        for (idx = 0; idx <= SETTING_INDEX_NCOORD; idx++)  settings_write_coord_data(idx, coord_data);
    }
    if (restore_flag & SETTINGS_RESTORE_BUILD_INFO) {
        EEPROM.write(EEPROM_ADDR_BUILD_INFO, 0);
        EEPROM.write(EEPROM_ADDR_BUILD_INFO + 1, 0); // Checksum
        EEPROM.commit();
    }
}

// Get settings values from non volatile storage into memory
void load_settings()
{
    for (Command *s = CommandsList; s; s = s->next()) {
        s->load();
    }
}

extern void make_settings();
extern void make_web_settings();
void settings_init()
{
    make_settings();
    make_web_settings();
    load_settings();
}


// FIXME - jog may need to be special-cased in the parser, since
// it is not really a setting and the entire line needs to be
// sent to gc_execute_line.  It is probably also more time-critical
// than actual settings, which change infrequently, so handling
// it early is probably prudent.
uint8_t jog_set(uint8_t *value, uint8_t client) {
    // Execute only if in IDLE or JOG states.
    if (sys.state != STATE_IDLE && sys.state != STATE_JOG)  return STATUS_IDLE_ERROR;

    // restore the $J= prefix because gc_execute_line() expects it
#define MAXLINE 128
    char line[MAXLINE];
    strcpy(line, "$J=");
    strncat(line, (char *)value, MAXLINE-strlen("$J=")-1);

    return gc_execute_line(line, client); // NOTE: $J= is ignored inside g-code parser and used to detect jog motions.
}

err_t report_gcode(uint8_t client) {
    report_gcode_modes(client);
    return STATUS_OK;
}
const char *map_grbl_value(const char *value) {
    if (strcmp(value, "Off") == 0) {
        return "0";
    }
    if (strcmp(value, "On") == 0) {
        return "1";
    }
    return value;
}
void show_grbl_settings(uint8_t client, group_t group, bool wantAxis) {
    //auto out = new ESPResponseStream(client);
    for (Command *cp = CommandsList; cp; cp = cp->next()) {
        if (cp->getGroup() == group && cp->getGrblName()) {
            bool isAxis = cp->getAxis() != NO_AXIS;
            // The following test could be expressed more succinctly with XOR,
            // but is arguably clearer when written out
            if ((wantAxis && isAxis) || (!wantAxis && !isAxis)) {
                Setting *s = (Setting *)cp;
                grbl_sendf(client, "%s=%s\r\n", s->getGrblName(), map_grbl_value(s->getStringValue()));
            }
        }
    }
}
err_t report_normal_settings(uint8_t client) {
    show_grbl_settings(client, GRBL, false);     // GRBL non-axis settings
    show_grbl_settings(client, GRBL, true);      // GRBL axis settings
    return STATUS_OK;
}
err_t report_extended_settings(uint8_t client) {
    show_grbl_settings(client, GRBL, false);     // GRBL non-axis settings
    show_grbl_settings(client, EXTENDED, false); // Extended non-axis settings
    show_grbl_settings(client, GRBL, true);      // GRBL axis settings
    show_grbl_settings(client, EXTENDED, true);  // Extended axis settings
    return STATUS_OK;
}
err_t list_settings(uint8_t client)
{
    for (Command *cp = CommandsList; cp; cp = cp->next()) {
        if (cp->getGroup() <= WEBUI) {
            Setting* s = (Setting*)cp;
            grbl_sendf(client, "%s=%s\r\n", s->getName(), s->getStringValue());
        }
    }
    return STATUS_OK;
}
err_t toggle_check_mode(uint8_t client) {
    // Perform reset when toggling off. Check g-code mode should only work if Grbl
    // is idle and ready, regardless of alarm locks. This is mainly to keep things
    // simple and consistent.
    if (sys.state == STATE_CHECK_MODE) {
        mc_reset();
        report_feedback_message(MESSAGE_DISABLED);
    } else {
        if (sys.state)  return (STATUS_IDLE_ERROR);  // Requires no alarm mode.
        sys.state = STATE_CHECK_MODE;
        report_feedback_message(MESSAGE_ENABLED);
    }
    return STATUS_OK;
}
err_t disable_alarm_lock(uint8_t client) {
    if (sys.state == STATE_ALARM) {
        // Block if safety door is ajar.
        if (system_check_safety_door_ajar())
            return (STATUS_CHECK_DOOR);
        report_feedback_message(MESSAGE_ALARM_UNLOCK);
        sys.state = STATE_IDLE;
        // Don't run startup script. Prevents stored moves in startup from causing accidents.
    } // Otherwise, no effect.
    return STATUS_OK;
}
err_t report_ngc(uint8_t client) {
    report_ngc_parameters(client);
    return STATUS_OK;
}
err_t home(uint8_t client, int cycle) {
    if (bit_isfalse(settings.flags, BITFLAG_HOMING_ENABLE))
        return (STATUS_SETTING_DISABLED);
    if (system_check_safety_door_ajar())
        return (STATUS_CHECK_DOOR); // Block if safety door is ajar.
    sys.state = STATE_HOMING;     // Set system state variable
    mc_homing_cycle(cycle);
    if (!sys.abort) {                         // Execute startup scripts after successful homing.
        sys.state = STATE_IDLE; // Set to IDLE when complete.
        st_go_idle();           // Set steppers to the settings idle state before returning.
        if (cycle == HOMING_CYCLE_ALL) {
            char line[128];
            system_execute_startup(line);
        }
    }
    return STATUS_OK;
}
err_t home_all(uint8_t client) {
    return home(client, HOMING_CYCLE_ALL);
}
err_t home_x(uint8_t client) {
    return home(client, HOMING_CYCLE_X);
}
err_t home_y(uint8_t client) {
    return home(client, HOMING_CYCLE_Y);
}
err_t home_z(uint8_t client) {
    return home(client, HOMING_CYCLE_Z);
}
err_t home_a(uint8_t client) {
    return home(client, HOMING_CYCLE_A);
}
err_t home_b(uint8_t client) {
    return home(client, HOMING_CYCLE_B);
}
err_t home_c(uint8_t client) {
    return home(client, HOMING_CYCLE_C);
}
err_t sleep_grbl(uint8_t client) {
    system_set_exec_state_flag(EXEC_SLEEP);
    return STATUS_OK;
}
err_t get_report_build_info(uint8_t client) {
    char line[128];
    settings_read_build_info(line);
    report_build_info(line, client);
    return STATUS_OK;
}
err_t report_startup_lines(uint8_t client) {
    report_startup_line(0, startup_line_0->get(), client);
    report_startup_line(1, startup_line_1->get(), client);
    return STATUS_OK;
}

// The following table is used if the line is of the form "$key\n"
// i.e. dollar commands without "="
// The key value is matched against the string and the corresponding
// function is called with no arguments.
// If there is no key match an error is reported
typedef err_t (*Command_t)(uint8_t);
std::map<const char*, Command_t, cmp_str> dollarCommands = {
    { "$", report_normal_settings },
    { "+", report_extended_settings },
    { "S", list_settings },
    { "G", report_gcode },
    { "C", toggle_check_mode },
    { "N", report_nvs_stats },
    { "X", disable_alarm_lock },
    { "#", report_ngc },
    { "H", home_all },
    { "HX", home_x },
    { "HY", home_y },
    { "HZ", home_z },
    { "HA", home_a },
    { "HB", home_b },
    { "HC", home_c },
    { "SLP", sleep_grbl },
    { "I", get_report_build_info },
    { "N", report_startup_lines },
};
// FIXME See Store startup line [IDLE/ALARM]

std::map<const char*, uint8_t, cmp_str> restoreCommands = {
    { "$", SETTINGS_RESTORE_DEFAULTS },
    { "settings", SETTINGS_RESTORE_DEFAULTS },
    { "#", SETTINGS_RESTORE_PARAMETERS },
    { "gcode", SETTINGS_RESTORE_PARAMETERS },
    { "*", SETTINGS_RESTORE_ALL },
    { "all", SETTINGS_RESTORE_ALL },
    { "@", SETTINGS_RESTORE_WIFI_SETTINGS },
    { "wifi", SETTINGS_RESTORE_WIFI_SETTINGS },
};
// normalize_key puts a key string into canonical form -
// without whitespace.
// start points to a null-terminated string.
// Returns the first substring that does not contain whitespace.
// Case is unchanged because comparisons are case-insensitive.
char *normalize_key(char *start) {
    char c;

    // In the usual case, this loop will exit on the very first test,
    // because the first character is likely to be non-white.
    // Null ('\0') is not considered to be a space character.
    while (isspace(c = *start) && c != '\0') {
        ++start;
    }

    // start now points to either a printable character or end of string
    if (c == '\0') {
        return start;
    }

    // Having found the beginning of the printable string,
    // we now scan forward until we find a space character.
    char *end;
    for (end = start; (c = *end) != '\0' && !isspace(c); end++) {
    }

    // end now points to either a whitespace character of end of string
    // In either case it is okay to place a null there
    *end = '\0';

    return start;
}

// This is for changing settings with $key=value .
// Lookup key in the Commands list, considering both
// the text name and the grbl compatible name, if any.
// If found, execute the object's "setStringValue" method.
// Otherwise fail.
// There is no "out" parameter because this does not
// generate any output; it just returns status
err_t do_command_or_setting(const char *key, char *value, ESPResponseStream* out) {
    // If value is NULL, set it to the empty string to simplify
    // subsequent tests.
    if (!value) {
        value = "";
    }
    // First search the list of settings.  If found, set a new
    // value if one is given, otherwise display the current value
    for (Command *cp = CommandsList; cp; cp = cp->next()) {
        if (cp->getGroup() <= WEBUI &&
            ((strcasecmp(cp->getName(), key) == 0) ||
             (cp->getGrblName() && (strcasecmp(cp->getGrblName(), key) == 0)))
        ) {
            Setting* s = (Setting*)cp;
            if (*value) {
                return s->setStringValue(value);
            } else {
                grbl_sendf(out->client(), "$%s=%s\n", s->getName(), s->getStringValue());
                return STATUS_OK;
            }
        }
    }
    // If a setting was not found, check the map of special $ commands
    if (*value) {
        if (strcasecmp(key, "RST") == 0) {
            auto it = restoreCommands.find(value);
            if (it == restoreCommands.end()) {
                return STATUS_INVALID_STATEMENT;
            }
            settings_restore(it->second);
            return STATUS_OK;
        }
    } else {
        std::map<const char*, Command_t, cmp_str>::iterator it = dollarCommands.find(key);
        if (it != dollarCommands.end()) {
            return it->second(out->client());
        }
    }

    // If we have not already found the command or setting, look for
    // a WebUI command.  They handle values internally; you cannot
    // determine whether to set or display solely based on the presence
    // of a value.
    for (Command *cp = CommandsList; cp; cp = cp->next()) {
        if ((cp->getGroup() > WEBUI) &&
                ((strcasecmp(cp->getName(), key) == 0) ||
                 (cp->getGrblName() && strcasecmp(cp->getGrblName(), key) == 0)
                )
        ) {
            return cp->action(value, out);
        }
    }
    return STATUS_INVALID_STATEMENT;
}

// This is for bare commands like "$RST" - no equals sign.
// Lookup key in the dollarCommands map.  If found, execute
// the corresponding command.
// As an enhancement to Classic GRBL, if the key is not found
// in the commands map, look it up in the lists of settings
// and display the current value.
err_t do_command(const char *key, ESPResponseStream* out) {

    std::map<const char*, Command_t, cmp_str>::iterator it = dollarCommands.find(key);
    if (it != dollarCommands.end()) {
        return it->second(out->client());
    }

    // Enhancement - not in Classic GRBL:
    // If it is not a command, look up the key
    // as a setting and display the value.
    for (Command *cp = CommandsList; cp; cp = cp->next()) {
        if ((strcasecmp(cp->getName(), key) == 0)
        || (cp->getGrblName() && (strcasecmp(cp->getGrblName(), key) == 0))) {
            if (cp->getGroup() <= WEBUI) {
                Setting* s = (Setting*)cp;
                grbl_sendf(out->client(), "$%s=%s\n", s->getName(), s->getStringValue());
            }
            return STATUS_OK;
        }
    }

    return STATUS_INVALID_STATEMENT;
}

uint8_t system_execute_line(char* line, ESPResponseStream* out, level_authenticate_type auth_level) {
    char *value;
    if (*line++ == '[') { // [ESPxxx] form
        value = strrchr(line, ']');
        if (!value) {
            // Missing ] is an error in this form
            return STATUS_INVALID_STATEMENT;
        }
        // ']' was found; replace it with null and set value to
        // to the rest of the line, which might be empty
        *value++ = '\0';
    } else {
        // $xxx form
        value = strrchr(line, '=');
        if (value) {
            // $xxx=yyy form.
            *value++ = '\0';
        }
    }
    char *key = normalize_key(line);
    // At this point there are three possibilities for value
    // NULL - $xxx without =
    // empty string - [ESPxxx] or $xxx= with nothing after
    // non-empty string - [ESPxxx]yyy or $xxx=yyy
    return do_command_or_setting(key, value, out);
}
uint8_t system_execute_line(char* line, uint8_t client) {
    return system_execute_line(line, new ESPResponseStream(client, true), LEVEL_GUEST);
}

void system_execute_startup(char* line) {
    err_t status_code;
    const char *gcline = startup_line_0->get();
    if (*gcline) {
        status_code = gc_execute_line(gcline, CLIENT_SERIAL);
        report_execute_startup_message(gcline, status_code, CLIENT_SERIAL);
    }
    gcline = startup_line_1->get();
    if (*gcline) {
        status_code = gc_execute_line(gcline, CLIENT_SERIAL);
        report_execute_startup_message(gcline, status_code, CLIENT_SERIAL);
    }
}
